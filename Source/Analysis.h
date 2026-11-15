#pragma once

#include "Common.h"

namespace brl
{
    /** Tempo / position info captured from the host when recording started. */
    struct CaptureInfo
    {
        double sampleRate   = 44100.0;
        double bpm          = 120.0;
        int    beatsPerBar  = 4;
        double startPpq     = 0.0;     // song position (quarter notes) of the first captured sample
        bool   bpmFromHost  = false;
        String title;
    };

    /** Everything the renderer needs, precomputed once after a capture. Immutable once built. */
    struct AnalysisData
    {
        static constexpr int   kOverview = 4096;   // overview buckets for the whole capture
        static constexpr double kFps     = 60.0;   // analysis frame rate
        static constexpr int   kBands    = 64;     // spectrum bands

        AudioBuffer<float> audio;                  // the captured audio, untouched (bit-exact)
        CaptureInfo info;
        double duration = 0.0;

        // whole-capture overview (mono)
        std::vector<float> ovMin, ovMax, ovRms, ovLow, ovMid, ovHigh;

        // per analysis frame
        int numFrames = 0;
        std::vector<float> peakL, peakR, rmsL, rmsR, lufsM, lufsS, low, mid, high, kickEnv, bassHz;
        std::vector<std::array<float, kBands>> spectrum;   // 0..1
        std::vector<std::array<float, 12>>     chroma;     // 0..1, normalised per frame

        std::vector<float>  bassSignal;                    // low-passed mono, full sample rate
        std::vector<double> kicks;                         // onset times (s) in the low band
        std::vector<double> hits;                          // onset times (s), full band
        float integratedLufs = -70.0f;
        int   keyRoot = 0;
        bool  keyMinor = false;

        // ---- helpers ----
        double secondsPerBeat() const  { return 60.0 / jmax (1.0, info.bpm); }
        double secondsPerBar() const   { return secondsPerBeat() * info.beatsPerBar; }
        double bars() const            { return duration / secondsPerBar(); }

        float frameValue (const std::vector<float>& v, double t) const;
        int   frameIndex (double t) const  { return jlimit (0, jmax (0, numFrames - 1), (int) std::floor (t * kFps)); }
        /** 0..1 decaying pulse from the most recent onset in `list` before t. */
        float pulse (const std::vector<double>& list, double t, double decay = 7.0) const;
        int   countBefore (const std::vector<double>& list, double t) const;
        int64 sampleIndex (double t) const { return (int64) (t * info.sampleRate); }
    };

    /** A note from a loaded .mid file, in quarter notes. */
    struct MidiNote { double start, end; int note; float velocity; };

    struct MidiData
    {
        std::vector<MidiNote> notes;
        double lengthQuarters = 0.0;     // rounded up to whole bars
        int    lowest = 60, highest = 72;
        String fileName;

        static std::shared_ptr<MidiData> load (const File& file, int beatsPerBar, String& error);
    };

    /** Builds AnalysisData from captured audio. Can take a second or two – call off the audio thread. */
    std::shared_ptr<AnalysisData> analyse (AudioBuffer<float>&& audio, const CaptureInfo& info,
                                           const MidiData* midiForKey = nullptr);

    /** Key from MIDI pitch classes when available, otherwise from the audio chroma. */
    void detectKey (AnalysisData& d, const MidiData* midi);

    /** Estimates tempo from onset autocorrelation (used for imported audio without host tempo). */
    double estimateBpm (const AnalysisData& d);

    /** Reads any audio file JUCE can decode into a float buffer (bit-exact for PCM/float WAV & AIFF). */
    bool loadAudioFile (const File& f, AudioBuffer<float>& out, double& sampleRate, String& error);

    /** Writes a 32-bit float WAV – lossless for whatever the DAW sent. */
    bool writeFloatWav (const File& f, const AudioBuffer<float>& buffer, double sampleRate);

    String keyName (int root, bool minor);
}
