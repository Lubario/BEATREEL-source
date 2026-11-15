// Automated check of the capture path: REC must store the master bit-for-bit and stop exactly at the loop end.
#include "PluginProcessor.h"

using namespace juce;
using namespace brl;

struct FakeHost : public AudioPlayHead
{
    double sr = 48000, bpm = 140, ppq = 16;
    bool playing = false, looping = true, reportLoop = true;
    double loopStart = 16, loopEnd = 32;
    int64 time = 0;
    Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setIsPlaying (playing); p.setBpm (bpm); p.setPpqPosition (ppq); p.setTimeInSamples (time);
        p.setTimeSignature (TimeSignature { 4, 4 });
        p.setIsLooping (looping);
        if (reportLoop) p.setLoopPoints (LoopPoints { loopStart, loopEnd });
        return p;
    }
    void advance (int n)
    {
        time += n;
        ppq += n / sr * bpm / 60.0;
        if (looping && ppq >= loopEnd) ppq = loopStart + (ppq - loopEnd);   // wrap at block boundary
    }
};

static int failures = 0;
static void check (bool ok, const String& what) { std::cout << (ok ? "PASS  " : "FAIL  ") << what << "\n"; if (! ok) ++failures; }

static void runCase (const String& name, bool reportLoop, bool offline, bool stopTransportAfter)
{
    BeatReelProcessor p;
    FakeHost host;
    host.reportLoop = reportLoop;
    host.looping = ! stopTransportAfter;
    p.setPlayHead (&host);
    p.setNonRealtime (offline);
    p.prepareToPlay (host.sr, 512);
    MessageManager::getInstance()->runDispatchLoopUntil (1500);   // let the demo analysis finish

    // reference "master" signal: random floats incl. values > 1.0 and tiny denormal-ish values
    const int total = (int) (host.sr * 20);
    AudioBuffer<float> ref (2, total);
    Random rng (7);
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < total; ++i) ref.setSample (c, i, (rng.nextFloat() * 2.4f - 1.2f) * (i % 97 == 0 ? 1e-30f : 1.0f));

    p.toggleRec();
    host.playing = true;
    MidiBuffer midi;
    int pos = 0, block = 0;
    const int expectLoop = (int) std::llround ((host.loopEnd - host.loopStart) * 60.0 / host.bpm * host.sr);
    const int stopAt = stopTransportAfter ? (int) (host.sr * 7.3) : total;
    while (pos < total)
    {
        int n = 256 + (block++ * 97) % 700;          // varying block sizes, like real hosts
        n = jmin (n, total - pos);
        if (pos >= stopAt) host.playing = false;
        AudioBuffer<float> io (2, n);
        for (int c = 0; c < 2; ++c) io.copyFrom (c, 0, ref, c, pos, n);
        p.processBlock (io, midi);
        // pass-through must be untouched
        bool same = true;
        for (int c = 0; c < 2 && same; ++c) same = std::memcmp (io.getReadPointer (c), ref.getReadPointer (c, pos), sizeof (float) * (size_t) n) == 0;
        if (! same) { check (false, name + ": audio passes through unchanged"); break; }
        host.advance (n);
        pos += n;
        if (pos > expectLoop + 48000 && ! stopTransportAfter) break;
        if (stopTransportAfter && pos > stopAt + 4096) break;
        if (block % 40 == 0) MessageManager::getInstance()->runDispatchLoopUntil (1);
    }
    // wait for finish + analysis + WAV write
    for (int i = 0; i < 100 && (p.getRecState() != BeatReelProcessor::recIdle || p.isAnalysing() || p.isDemo()); ++i)
        MessageManager::getInstance()->runDispatchLoopUntil (100);

    auto d = p.getData();
    check (d != nullptr && ! p.isDemo(), name + ": capture finished and analysed");
    if (d == nullptr || p.isDemo()) return;
    const int got = d->audio.getNumSamples();
    int expected = stopTransportAfter ? -1 : expectLoop;
    if (stopTransportAfter)
    {
        // everything until the first block that reported "stopped"
        expected = 0; int b = 0;
        while (expected < stopAt) expected += 256 + (b++ * 97) % 700;
    }
    if (! reportLoop && ! stopTransportAfter)
    {
        // no loop points: stops at the first block after the wrap
        expected = 0; int b = 0;
        double q = 16;
        while (true) { int n = 256 + (b++ * 97) % 700; double nq = q + n / host.sr * host.bpm / 60.0; if (nq >= 32) { expected += n; break; } expected += n; q = nq; }
    }
    check (got == expected, name + ": length " + String (got) + " samples (expected " + String (expected) + ")");
    bool bitExact = got <= total;
    for (int c = 0; c < 2 && bitExact; ++c)
        bitExact = std::memcmp (d->audio.getReadPointer (c), ref.getReadPointer (c), sizeof (float) * (size_t) jmin (got, total)) == 0;
    check (bitExact, name + ": captured audio is bit-identical to the master");
    check (std::abs (d->info.startPpq - 16.0) < 1e-9 && std::abs (d->info.bpm - 140.0) < 1e-9, name + ": tempo + start bar taken from the DAW");

    // the WAV kept on disk is bit-exact too
    AudioBuffer<float> fromDisk; double sr = 0; String err;
    bool diskOk = loadAudioFile (p.getCaptureFile(), fromDisk, sr, err) && fromDisk.getNumSamples() == got;
    for (int c = 0; c < 2 && diskOk; ++c)
        diskOk = std::memcmp (fromDisk.getReadPointer (c), ref.getReadPointer (c), sizeof (float) * (size_t) got) == 0;
    check (diskOk, name + ": saved 32-bit float WAV is bit-identical");
}

int main()
{
    ScopedJuceInitialiser_GUI gui;
    runCase ("loop points reported", true, false, false);
    runCase ("loop wraps, no loop points", false, false, false);
    runCase ("offline export, transport stops", true, true, true);
    std::cout << (failures == 0 ? "ALL PASSED\n" : String (failures) + " FAILED\n");
    return failures == 0 ? 0 : 1;
}
