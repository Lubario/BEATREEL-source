#pragma once

#include "Renderer.h"

namespace brl
{
    struct ExportJob
    {
        std::shared_ptr<const AnalysisData> data;
        std::shared_ptr<const MidiData> midi;
        Style style;
        File output;              // .mp4 (or .mov when lossless)
        File ffmpeg;
        bool watermark = false;
        bool alsoSaveWav = false; // writes the bit-exact capture next to the video
        int  threads = 0;         // 0 = auto
    };

    /** Renders a job to video on a background thread. Poll from the UI. */
    class Exporter : private Thread
    {
    public:
        Exporter() : Thread ("BEATREEL export") {}
        ~Exporter() override { stopThread (10000); }

        bool start (ExportJob job);
        void cancel()                  { signalThreadShouldExit(); }
        bool isBusy() const            { return isThreadRunning(); }
        float getProgress() const      { return progress.load(); }
        String getStatus() const       { const ScopedLock sl (lock); return status; }
        /** After the thread finishes: empty when OK, otherwise an error message. */
        String getResult() const       { const ScopedLock sl (lock); return result; }
        File   getOutput() const       { return currentJob.output; }

        /** Synchronous version (used by the CLI tool). progressCb returns false to cancel. */
        static bool runJob (const ExportJob& job, std::function<bool (float, const String&)> progressCb, String& error);

        /** Finds ffmpeg: user setting, BEATREEL data folder, inside the plugin bundle, then PATH. */
        static File findFfmpeg (const String& userPath = {});

    private:
        void run() override;
        ExportJob currentJob;
        std::atomic<float> progress { 0.0f };
        CriticalSection lock;
        String status, result;
    };
}
