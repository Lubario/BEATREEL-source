#include "Exporter.h"

namespace brl
{
bool Exporter::start (ExportJob job)
{
    if (isThreadRunning()) return false;
    currentJob = std::move (job);
    progress = 0.0f;
    { const ScopedLock sl (lock); status = "Starting"; result = {}; }
    startThread();
    return true;
}

void Exporter::run()
{
    String err;
    runJob (currentJob, [this] (float p, const String& st)
    {
        progress = p;
        { const ScopedLock sl (lock); status = st; }
        return ! threadShouldExit();
    }, err);
    const ScopedLock sl (lock);
    result = threadShouldExit() && err.isEmpty() ? String ("Cancelled") : err;
    status = result.isEmpty() ? "Done" : result;
}

File Exporter::findFfmpeg (const String& userPath)
{
    #if JUCE_WINDOWS
     const String exe = "ffmpeg.exe";
    #else
     const String exe = "ffmpeg";
    #endif
    Array<File> candidates;
    if (userPath.isNotEmpty()) candidates.add (File (userPath));
    candidates.add (getUserDataDir().getChildFile (exe));
    candidates.add (getUserDataDir().getChildFile ("ffmpeg").getChildFile (exe));

    // next to the plugin binary / inside the bundle's Resources folder
    auto self = File::getSpecialLocation (File::currentExecutableFile);
    for (auto dir = self.getParentDirectory(); dir.exists() && dir != dir.getParentDirectory(); dir = dir.getParentDirectory())
    {
        candidates.add (dir.getChildFile (exe));
        candidates.add (dir.getChildFile ("Resources").getChildFile (exe));
        if (dir.getFileName() == "Contents") break;
    }

    #if ! JUCE_WINDOWS
     for (auto p : { "/opt/homebrew/bin/ffmpeg", "/usr/local/bin/ffmpeg", "/usr/bin/ffmpeg" })
         candidates.add (File (p));
    #endif
    for (auto& dir : StringArray::fromTokens (SystemStats::getEnvironmentVariable ("PATH", {}),
                                              File::getSeparatorChar() == '\\' ? ";" : ":", {}))
        if (dir.isNotEmpty() && File::isAbsolutePath (dir)) candidates.add (File (dir).getChildFile (exe));

    for (auto& f : candidates)
        if (f.existsAsFile()) return f;
    return {};
}

namespace
{
    /** Picks the best H.264 encoder this ffmpeg build has (x264 isn't in every build). */
    StringArray videoEncoderArgs (const File& ffmpeg, int pixels)
    {
        static CriticalSection cacheLock;
        static std::map<String, String> cache;
        String enc;
        {
            const ScopedLock sl (cacheLock);
            if (auto it = cache.find (ffmpeg.getFullPathName()); it != cache.end()) enc = it->second;
        }
        if (enc.isEmpty())
        {
            ChildProcess p;
            String list;
            if (p.start (StringArray { ffmpeg.getFullPathName(), "-hide_banner", "-encoders" })) list = p.readAllProcessOutput();
            for (auto e : { "libx264", "h264_videotoolbox", "h264_mf", "libopenh264", "h264_nvenc", "h264_qsv", "h264_amf" })
                if (list.contains (String (" ") + e + " ")) { enc = e; break; }
            if (enc.isEmpty()) enc = "mpeg4";
            const ScopedLock sl (cacheLock);
            cache[ffmpeg.getFullPathName()] = enc;
        }
        const String bitrate = String (pixels >= 1920 * 1080 ? 16 : 12) + "M";
        if (enc == "libx264") return { "-c:v", "libx264", "-preset", "medium", "-crf", "16", "-profile:v", "high" };
        if (enc == "h264_videotoolbox") return { "-c:v", enc, "-b:v", bitrate, "-profile:v", "high" };
        if (enc == "mpeg4") return { "-c:v", "mpeg4", "-q:v", "2" };
        return { "-c:v", enc, "-b:v", bitrate };
    }
}

bool Exporter::runJob (const ExportJob& job, std::function<bool (float, const String&)> cb, String& error)
{
    if (job.data == nullptr || job.data->numFrames == 0) { error = "Nothing captured yet"; return false; }
    if (! job.ffmpeg.existsAsFile()) { error = "ffmpeg not found - see the install guide"; return false; }

    const auto size = aspectSize (job.style.aspect);
    const int w = size.x, h = size.y, fps = job.style.fps;
    const int frames = (int) std::ceil (job.data->duration * fps);

    auto tmp = File::getSpecialLocation (File::tempDirectory).getChildFile ("beatreel_" + String::toHexString (Random::getSystemRandom().nextInt64()));
    tmp.createDirectory();
    struct Cleanup { File d; ~Cleanup() { d.deleteRecursively(); } } cleanup { tmp };

    // 1) the captured audio, untouched, as 32-bit float WAV
    auto wav = tmp.getChildFile ("audio.wav");
    if (! cb (0.0f, "Writing audio") || ! writeFloatWav (wav, job.data->audio, job.data->info.sampleRate))
    {
        error = "Couldn't write the audio"; return false;
    }
    if (job.alsoSaveWav)
        wav.copyFileTo (job.output.withFileExtension ("wav"));

    Image bg;
    if (job.style.background == bgImage && job.style.bgImagePath.isNotEmpty())
        bg = ImageFileFormat::loadFrom (File (job.style.bgImagePath));

    // 2) frames, rendered in parallel
    std::atomic<int> next { 0 }, done { 0 };
    std::atomic<bool> failed { false };
    const int nThreads = job.threads > 0 ? job.threads : jlimit (1, 8, SystemStats::getNumCpus() - 1);

    auto worker = [&]
    {
        Image img (Image::RGB, w, h, false, SoftwareImageType());
        JPEGImageFormat jpg;
        jpg.setQuality (0.97f);
        for (int i; (i = next++) < frames && ! failed;)
        {
            {
                Graphics g (img);
                RenderContext rc;
                rc.data = job.data.get(); rc.midi = job.midi.get(); rc.style = &job.style;
                rc.backgroundImage = bg.isValid() ? &bg : nullptr;
                rc.t = (double) i / fps;
                rc.watermark = job.watermark;
                renderFrame (g, w, h, rc);
            }
            auto f = tmp.getChildFile ("f" + String (i).paddedLeft ('0', 6) + ".jpg");
            FileOutputStream os (f);
            if (! os.openedOk() || ! jpg.writeImageToStream (img, os)) failed = true;
            ++done;
        }
    };

    {
        std::vector<std::unique_ptr<std::thread>> pool;
        for (int k = 0; k < nThreads - 1; ++k) pool.push_back (std::make_unique<std::thread> (worker));
        // the calling thread also renders, and reports progress in between
        std::thread own (worker);
        while (done.load() < frames && ! failed)
        {
            if (! cb (0.02f + 0.83f * (float) done.load() / frames, "Rendering frame " + String (done.load()) + " / " + String (frames)))
                failed = true;
            Thread::sleep (100);
        }
        own.join();
        for (auto& t : pool) t->join();
    }
    if (failed) { if (error.isEmpty()) error = "Rendering stopped"; return false; }

    // 3) encode with the original audio
    if (! cb (0.86f, "Encoding video")) return false;
    auto out = job.output;
    out.getParentDirectory().createDirectory();
    out.deleteFile();

    StringArray args { job.ffmpeg.getFullPathName(), "-y", "-hide_banner", "-loglevel", "error",
                       "-framerate", String (fps), "-i", tmp.getChildFile ("f%06d.jpg").getFullPathName(),
                       "-i", wav.getFullPathName(),
                       "-map", "0:v", "-map", "1:a" };
    args.addArray (videoEncoderArgs (job.ffmpeg, w * h));
    args.addArray ({ "-pix_fmt", "yuv420p", "-r", String (fps) });
    if (job.style.lossless) args.addArray ({ "-c:a", "alac" });
    else                    args.addArray ({ "-c:a", "aac", "-b:a", "320k" });
    args.addArray ({ "-shortest", "-movflags", "+faststart", out.getFullPathName() });

    ChildProcess proc;
    if (! proc.start (args, ChildProcess::wantStdOut | ChildProcess::wantStdErr)) { error = "Couldn't start ffmpeg"; return false; }
    String log;
    char buf[1024];
    while (proc.isRunning())
    {
        if (! cb (0.9f, "Encoding video")) { proc.kill(); return false; }
        if (auto n = proc.readProcessOutput (buf, sizeof (buf)); n > 0) log += String::fromUTF8 (buf, n);
        else Thread::sleep (50);
    }
    log += proc.readAllProcessOutput();
    if (proc.getExitCode() != 0 || ! out.existsAsFile())
    {
        error = "ffmpeg failed: " + log.trim().substring (0, 300);
        return false;
    }
    cb (1.0f, "Done");
    return true;
}
}
