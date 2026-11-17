// brl_render - make a reel without a DAW.
//
//   brl_render <in.wav | --demo> <out.mp4> [options]
//   brl_render <in.wav | --demo> --still <out.png> <seconds> [options]
//
// options: --mid file.mid  --look 0-11  --random <seed>  --aspect 9:16|4:5|1:1|16:9
//          --bpm 140  --title "NAME"  --fps 30|60  --watermark  --lossless  --ffmpeg path
#include "Exporter.h"
#include "Demo.h"

using namespace juce;
using namespace brl;

int main (int argc, char* argv[])
{
    ScopedJuceInitialiser_GUI gui;   // fonts need the GUI subsystem; no window is opened
    StringArray a;
    for (int i = 1; i < argc; ++i) a.add (argv[i]);
    if (a.size() < 2) { std::cout << "usage: brl_render <in.wav|--demo> <out.mp4> [--still out.png sec] [options]\n"; return 1; }

    auto opt = [&] (const String& k) { auto i = a.indexOf (k); return i >= 0 && i + 1 < a.size() ? a[i + 1] : String(); };
    auto cwd = File::getCurrentWorkingDirectory();

    CaptureInfo info;
    std::shared_ptr<MidiData> midi;
    AudioBuffer<float> audio;
    if (a[0] == "--demo") audio = makeDemoBeat (48000.0, info, midi);
    else
    {
        String err;
        if (! loadAudioFile (cwd.getChildFile (a[0]), audio, info.sampleRate, err)) { std::cerr << err << "\n"; return 1; }
        info.title = cwd.getChildFile (a[0]).getFileNameWithoutExtension().toUpperCase();
    }
    if (auto m = opt ("--mid"); m.isNotEmpty())
    {
        String err;
        midi = MidiData::load (cwd.getChildFile (m), 4, err);
        if (midi == nullptr) std::cerr << "MIDI: " << err << "\n";
    }

    auto t0 = Time::getMillisecondCounterHiRes();
    auto data = analyse (std::move (audio), info, midi.get());
    if (auto b = opt ("--bpm"); b.isNotEmpty()) data->info.bpm = b.getDoubleValue();
    else if (a[0] != "--demo") data->info.bpm = estimateBpm (*data);
    std::cout << "analysed " << String (data->duration, 2) << " s in " << String ((Time::getMillisecondCounterHiRes() - t0) / 1000.0, 2)
              << " s  bpm " << data->info.bpm << "  key " << keyName (data->keyRoot, data->keyMinor)
              << "  int " << String (data->integratedLufs, 1) << " LUFS  kicks " << (int) data->kicks.size() << "  hits " << (int) data->hits.size() << "\n";

    Style st = makeLook (opt ("--look").getIntValue());
    if (auto r = opt ("--random"); r.isNotEmpty()) { Random rng (r.getLargeIntValue()); randomise (st, rng); }
    st.title = opt ("--title").isNotEmpty() ? opt ("--title") : (info.title.isNotEmpty() ? info.title : String ("UNTITLED"));
    if (auto as = opt ("--aspect"); as.isNotEmpty())
        for (int i = 0; i < kNumAspects; ++i) if (as == aspectName (i)) st.aspect = i;
    if (opt ("--fps") == "60") st.fps = 60;
    st.lossless = a.contains ("--lossless");
    if (auto v = opt ("--cards"); v.isNotEmpty())
    {
        st.cards = 0;
        for (auto& n : StringArray::fromTokens (v.toUpperCase(), ",", {}))
            for (int i = 0; i < kNumCards; ++i) if (n.trim() == cardName (i)) st.setCard (i, true);
    }
    if (auto v = opt ("--intro"); v.isNotEmpty()) st.intro = jlimit (0, (int) kNumIntros - 1, v.getIntValue());
    if (auto v = opt ("--intro-len"); v.isNotEmpty()) st.introLength = (float) v.getDoubleValue();
    if (auto v = opt ("--intro-fade"); v.isNotEmpty()) st.introTransition = (float) v.getDoubleValue();
    if (a.contains ("--intro-before")) st.introMode = introBeforeBeat;

    if (a[1] == "--sweep")
    {
        // stress test: every card, every look, every aspect, many times incl. the exact start and end
        const int n = jmax (2, a[2].getIntValue());
        int frames = 0;
        for (int look = 0; look < kNumLooks; ++look)
            for (int asp = 0; asp < kNumAspects; ++asp)
            {
                Style sw = makeLook (look);
                sw.cards = (1u << kNumCards) - 1;
                sw.intro = (look + asp) % kNumIntros;
                sw.introMode = look % 2;
                sw.aspect = asp;
                auto size = aspectSize (asp) / 4;
                Image img (Image::ARGB, size.x, size.y, true, SoftwareImageType());
                for (int i = 0; i < n; ++i)
                {
                    const double times[] = { 0.0, 1e-4, 0.01, 0.03, data->duration - 0.01, data->duration, data->duration + 1.0, -1.0 };
                    const double vd = videoDuration (sw, *data);
                    const double tt = i < 8 ? times[i] : vd * (i - 8) / (n - 8);
                    Graphics g (img);
                    RenderContext rc; rc.data = data.get(); rc.midi = (i % 2) ? midi.get() : nullptr; rc.style = &sw; rc.t = tt;
                    renderFrame (g, size.x, size.y, rc);
                    ++frames;
                }
            }
        std::cout << "sweep ok: " << frames << " frames\n";
        return 0;
    }

    if (a[1] == "--still")
    {
        auto size = aspectSize (st.aspect);
        Image img (Image::RGB, size.x, size.y, true);
        {
            Graphics g (img);
            RenderContext rc; rc.data = data.get(); rc.midi = midi.get(); rc.style = &st; rc.t = a[3].getDoubleValue();
            rc.watermark = a.contains ("--watermark");
            auto r0 = Time::getMillisecondCounterHiRes();
            renderFrame (g, size.x, size.y, rc);
            std::cout << "frame rendered in " << String (Time::getMillisecondCounterHiRes() - r0, 1) << " ms\n";
        }
        auto f = cwd.getChildFile (a[2]);
        f.deleteFile();
        FileOutputStream os (f);
        PNGImageFormat png;
        return png.writeImageToStream (img, os) ? 0 : 1;
    }

    ExportJob job;
    job.data = data; job.midi = midi; job.style = st;
    job.output = cwd.getChildFile (a[1]);
    job.ffmpeg = Exporter::findFfmpeg (opt ("--ffmpeg"));
    job.watermark = a.contains ("--watermark");
    String err;
    int lastPct = -1;
    t0 = Time::getMillisecondCounterHiRes();
    bool ok = Exporter::runJob (job, [&] (float p, const String& s)
    {
        int pct = (int) (p * 100);
        if (pct / 10 != lastPct / 10) { std::cout << pct << "%  " << s << "\n"; lastPct = pct; }
        return true;
    }, err);
    std::cout << (ok ? "wrote " + job.output.getFullPathName() : "ERROR: " + err)
              << "  (" << String ((Time::getMillisecondCounterHiRes() - t0) / 1000.0, 1) << " s)\n";
    return ok ? 0 : 1;
}
