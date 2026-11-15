#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Exporter.h"
#include "License.h"

namespace brl
{
    /** Lock-free, allocation-free (in realtime) recorder: audio goes into pre-allocated chunks
        that a timer on the message thread keeps topped up. */
    class ChunkedRecorder
    {
    public:
        static constexpr int kChunkFrames = 1 << 17;          // ~2.7 s @ 48 kHz
        static constexpr int kMaxChunks   = 1 << 11;          // ~1.5 h @ 48 kHz

        void reset (int numChannels);                         // message thread
        void topUp (int spare = 4);                           // message thread
        bool write (const float* const* src, int numCh, int n, bool mayAllocate);   // audio thread
        int64 getNumFrames() const { return frames.load(); }
        void  truncate (int64 n)   { frames = jmin (frames.load(), n); }
        AudioBuffer<float> collect() const;                   // after recording has stopped

    private:
        bool allocateNext();
        int channels = 2;
        std::array<std::unique_ptr<float[]>, kMaxChunks> chunks;
        std::atomic<int> allocated { 0 };
        std::atomic<int64> frames { 0 };
        SpinLock allocLock;
    };

    class BeatReelProcessor : public AudioProcessor, private Timer
    {
    public:
        enum RecState { recIdle, recArmed, recRecording, recFinished };

        BeatReelProcessor();
        ~BeatReelProcessor() override;

        // AudioProcessor
        void prepareToPlay (double sampleRate, int samplesPerBlock) override;
        void releaseResources() override {}
        bool isBusesLayoutSupported (const BusesLayout&) const override;
        void processBlock (AudioBuffer<float>&, MidiBuffer&) override;
        using AudioProcessor::processBlock;
        AudioProcessorEditor* createEditor() override;
        bool hasEditor() const override { return true; }
        const String getName() const override { return kProductName; }
        bool acceptsMidi() const override { return false; }
        bool producesMidi() const override { return false; }
        double getTailLengthSeconds() const override { return 0.0; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram (int) override {}
        const String getProgramName (int) override { return {}; }
        void changeProgramName (int, const String&) override {}
        void getStateInformation (MemoryBlock&) override;
        void setStateInformation (const void*, int) override;

        // ---- capture --------------------------------------------------------
        void toggleRec();                  // idle -> armed -> (recording) -> stop
        void cancelRec();
        int    getRecState() const        { return recState.load(); }
        double getRecordedSeconds() const { return (double) recorder.getNumFrames() / jmax (1.0, currentSampleRate.load()); }
        double getRecordedBars() const;
        float  getLoopProgress() const;   // 0..1 when the loop length is known, else -1
        int    maxBars = 0;               // 0 = until the loop ends / transport stops

        // host info for the UI
        std::atomic<bool>   hostPlaying { false }, hostLooping { false };
        std::atomic<double> hostBpm { 120.0 }, hostPpq { 0.0 };
        String getHostShortName() const;

        // ---- data -------------------------------------------------------------
        std::shared_ptr<const AnalysisData> getData() const  { const ScopedLock sl (dataLock); return data; }
        std::shared_ptr<const MidiData>     getMidi() const  { const ScopedLock sl (dataLock); return midi; }
        bool isDemo() const               { return demo.load(); }
        bool isAnalysing() const          { return worker.isBusy(); }
        String getLastError() const       { const ScopedLock sl (dataLock); return lastError; }
        void importAudio (const File&);
        void loadMidi (const File&);
        void clearMidi();
        void setBpm (double bpm);         // re-tempo the current capture (imported audio)
        File getCaptureFile() const       { return captureFile; }

        // ---- look -------------------------------------------------------------
        Style style;                      // message thread only
        const Image* getBackgroundImage();  // cached, scaled for preview
        std::atomic<int> styleVersion { 0 };

        // ---- preview playback (audio when the DAW is stopped) --------------------
        std::atomic<bool>  previewPlaying { true };
        std::atomic<bool>  previewAudio { false };
        std::atomic<int64> previewPos { 0 };
        std::atomic<uint32> lastAudioCallbackMs { 0 };
        double getPreviewSeconds();       // message thread: advances the clock if the host isn't calling us

        // ---- export / license / settings ----------------------------------------
        Exporter exporter;
        License::Info license;
        PropertiesFile& settings();
        File findFfmpeg();

    private:
        void timerCallback() override;
        void finishCapture();
        void setData (std::shared_ptr<AnalysisData> d, bool isDemoData);
        void reanalyse (AudioBuffer<float> audio, CaptureInfo info);

        struct Worker : private Thread
        {
            Worker() : Thread ("BEATREEL analysis") {}
            ~Worker() override { stopThread (20000); }
            void run (std::function<void()> fn)
            {
                stopThread (20000);
                job = std::move (fn);
                startThread();
            }
            bool isBusy() const { return isThreadRunning(); }
        private:
            void run() override { if (job) job(); }
            std::function<void()> job;
        } worker;

        ChunkedRecorder recorder;
        std::atomic<int> recState { recIdle };
        std::atomic<bool> stopRequested { false };
        std::atomic<double> currentSampleRate { 44100.0 };
        // capture bookkeeping (audio thread while recording)
        CaptureInfo pendingInfo;
        double expectedPpq = 0.0;
        int64  expectedTime = 0;
        bool   hadPpq = false, hadTime = false;
        int64  limitFrames = 0;           // 0 = none
        std::atomic<int64> loopFrames { 0 };
        std::atomic<uint32> lastWriteMs { 0 };

        mutable CriticalSection dataLock;
        std::shared_ptr<const AnalysisData> data;
        std::shared_ptr<const MidiData> midi;
        std::atomic<bool> demo { true };
        String lastError;
        File captureFile, midiFile;

        // audio-thread copy of the captured audio for preview playback
        SpinLock previewLock;
        std::shared_ptr<const AnalysisData> previewData;

        Image bgImage;
        String bgImagePath;
        std::unique_ptr<PropertiesFile> props;
        double previewClockMs = 0;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BeatReelProcessor)
    };
}
