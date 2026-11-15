#include "Demo.h"

namespace brl
{
AudioBuffer<float> makeDemoBeat (double sr, CaptureInfo& info, std::shared_ptr<MidiData>& midi)
{
    const double bpm = 150.0, spb = 60.0 / bpm, s16 = spb / 4.0;
    const int bars = 8;
    const int n = (int) (bars * 4 * spb * sr);
    AudioBuffer<float> buf (2, n);
    buf.clear();
    auto* L = buf.getWritePointer (0);
    auto* R = buf.getWritePointer (1);
    Random rng (42);
    const double twoPi = MathConstants<double>::twoPi;
    auto hz = [] (int note) { return 440.0 * std::pow (2.0, (note - 69) / 12.0); };
    auto add = [&] (int64 i, float l, float r) { if (i >= 0 && i < n) { L[i] += l; R[i] += r; } };

    // progression: F, Dm, Bb, C  (two bars each cycle → 8 bars)
    const int roots[] = { 41, 38, 46 - 12, 36 };            // 808 roots: F2, D2, Bb1, C2
    const int chords[][3] = { { 65, 69, 72 }, { 62, 65, 69 }, { 58, 62, 65 }, { 60, 64, 67 } };

    midi = std::make_shared<MidiData>();
    midi->fileName = "demo";

    for (int bar = 0; bar < bars; ++bar)
    {
        const int ci = bar % 4;
        const double barT = bar * 4 * spb;

        // kick
        for (int st : { 0, 6, 10, (bar % 2 ? 14 : 11) })
        {
            int64 s0 = (int64) ((barT + st * s16) * sr);
            double ph = 0;
            for (int i = 0; i < (int) (0.35 * sr); ++i)
            {
                double tt = i / sr, f = 45 + 110 * std::exp (-tt * 30);
                ph += twoPi * f / sr;
                float v = (float) (std::sin (ph) * std::exp (-tt * 9) * 0.9);
                add (s0 + i, v, v);
            }
        }
        // clap on 2 and 4
        for (int st : { 4, 12 })
        {
            int64 s0 = (int64) ((barT + st * s16) * sr);
            float lp = 0;
            for (int i = 0; i < (int) (0.2 * sr); ++i)
            {
                double tt = i / sr;
                float nz = rng.nextFloat() * 2 - 1;
                lp += 0.35f * (nz - lp);
                float v = (nz - lp) * (float) std::exp (-tt * 22) * 0.45f + (float) (std::sin (twoPi * 190 * tt) * std::exp (-tt * 30) * 0.2);
                add (s0 + i, v, v * 0.9f);
            }
        }
        // hats: 8ths with 16th rolls on the last beat of odd bars
        for (int st = 0; st < 16; ++st)
        {
            if (st % 2 && ! (bar % 2 && st >= 12)) continue;
            int64 s0 = (int64) ((barT + st * s16) * sr);
            float prev = 0;
            for (int i = 0; i < (int) (0.05 * sr); ++i)
            {
                float nz = rng.nextFloat() * 2 - 1;
                float hp = nz - prev; prev = nz;
                float v = hp * (float) std::exp (-(i / sr) * 70) * 0.18f;
                add (s0 + i, v * 0.7f, v);
            }
        }
        // 808, two bars per root, slight glide into the next
        {
            int64 s0 = (int64) (barT * sr), len = (int64) (4 * spb * sr);
            double f0 = hz (roots[ci]);
            static double ph808 = 0;
            for (int64 i = 0; i < len; ++i)
            {
                double tt = i / sr;
                ph808 += twoPi * f0 / sr;
                float env = (float) (jmin (1.0, tt * 200) * (0.6 + 0.4 * std::exp (-tt * 2)));
                float v = (float) std::tanh (std::sin (ph808) * 1.6) * env * 0.42f;
                add (s0 + i, v, v);
            }
        }
        // chord stabs + melody (also written as MIDI)
        for (int st : { 0, 3, 6, 10, 12 })
        {
            const double start = barT + st * s16, len = s16 * (st == 12 ? 4 : 2.5);
            for (int k = 0; k < 3; ++k)
            {
                const int note = chords[ci][k];
                midi->notes.push_back ({ start / spb, (start + len) / spb, note, 0.6f + 0.1f * k });
                const double f = hz (note);
                int64 s0 = (int64) (start * sr);
                for (int i = 0; i < (int) (len * sr); ++i)
                {
                    double tt = i / sr;
                    double v = 0;
                    for (int hmc = 1; hmc <= 5; ++hmc) v += std::sin (twoPi * f * hmc * tt * (1 + 0.002 * k)) / hmc;
                    float e = (float) (std::exp (-tt * 6) * jmin (1.0, (len - tt) * 40));
                    add (s0 + i, (float) v * e * 0.05f * (k == 0 ? 1.2f : 0.8f), (float) v * e * 0.05f * (k == 2 ? 1.2f : 0.8f));
                }
            }
        }
        // bell melody: chord tones an octave up
        for (int st = 0; st < 16; st += (bar % 2 ? 2 : 3))
        {
            const int note = chords[ci][(st / 2 + bar) % 3] + 12;
            const double start = barT + st * s16, len = s16 * 1.5;
            midi->notes.push_back ({ start / spb, (start + len) / spb, note, 0.8f });
            const double f = hz (note);
            int64 s0 = (int64) (start * sr);
            for (int i = 0; i < (int) (0.6 * sr); ++i)
            {
                double tt = i / sr;
                float v = (float) ((std::sin (twoPi * f * tt) + 0.3 * std::sin (twoPi * f * 2.01 * tt)) * std::exp (-tt * 5) * 0.07);
                add (s0 + i, v * 0.8f, v);
            }
        }
    }

    // normalise to -1 dBFS
    const float peak = jmax (buf.getMagnitude (0, 0, n), buf.getMagnitude (1, 0, n));
    if (peak > 0) buf.applyGain (Decibels::decibelsToGain (-1.0f) / peak);

    std::sort (midi->notes.begin(), midi->notes.end(), [] (auto& a, auto& b) { return a.start < b.start; });
    midi->lowest = 127; midi->highest = 0;
    for (auto& nt : midi->notes) { midi->lowest = jmin (midi->lowest, nt.note); midi->highest = jmax (midi->highest, nt.note); }
    midi->lengthQuarters = bars * 4.0;

    info.sampleRate = sr;
    info.bpm = bpm;
    info.beatsPerBar = 4;
    info.startPpq = 64.0;   // pretend the loop starts at bar 17
    info.bpmFromHost = true;
    info.title = "HEAVEN";
    return buf;
}
}
