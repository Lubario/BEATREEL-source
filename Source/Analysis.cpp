#include "Analysis.h"

namespace brl
{
namespace
{
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

        void set (double nb0, double nb1, double nb2, double a0, double na1, double na2)
        {
            b0 = nb0 / a0; b1 = nb1 / a0; b2 = nb2 / a0; a1 = na1 / a0; a2 = na2 / a0;
        }
        static Biquad lowPass (double sr, double f, double q = 0.7071)
        {
            Biquad b; auto w = MathConstants<double>::twoPi * f / sr, c = std::cos (w), al = std::sin (w) / (2 * q);
            b.set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al); return b;
        }
        static Biquad highPass (double sr, double f, double q = 0.7071)
        {
            Biquad b; auto w = MathConstants<double>::twoPi * f / sr, c = std::cos (w), al = std::sin (w) / (2 * q);
            b.set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al); return b;
        }
        static Biquad highShelf (double sr, double f, double gainDb, double q)
        {
            Biquad b; auto A = std::pow (10.0, gainDb / 40.0), w = MathConstants<double>::twoPi * f / sr;
            auto c = std::cos (w), al = std::sin (w) / (2 * q), sa = 2 * std::sqrt (A) * al;
            b.set (A * ((A + 1) + (A - 1) * c + sa), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sa),
                   (A + 1) - (A - 1) * c + sa, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sa);
            return b;
        }
        inline float process (float x)
        {
            double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return (float) y;
        }
    };

    float rmsOf (const float* d, int n)
    {
        double s = 0; for (int i = 0; i < n; ++i) s += (double) d[i] * d[i];
        return n > 0 ? (float) std::sqrt (s / n) : 0.0f;
    }

    std::vector<double> pickOnsets (const std::vector<float>& env, double fps, float sensitivity, double minGap)
    {
        // positive spectral-ish flux on an envelope, adaptive threshold, peak picking
        std::vector<float> flux (env.size(), 0.0f);
        for (size_t i = 1; i < env.size(); ++i) flux[i] = jmax (0.0f, env[i] - env[i - 1]);

        float globalMax = 1e-9f;
        for (auto f : flux) globalMax = jmax (globalMax, f);

        std::vector<double> out;
        const int w = (int) (fps * 0.25);
        double last = -1e9;
        for (int i = 1; i + 1 < (int) flux.size(); ++i)
        {
            double mean = 0; int cnt = 0;
            for (int k = jmax (0, i - w); k <= jmin ((int) flux.size() - 1, i + w); ++k) { mean += flux[k]; ++cnt; }
            mean /= jmax (1, cnt);
            const float thr = (float) (mean * 1.6 + globalMax * sensitivity);
            if (flux[i] > thr && flux[i] >= flux[i - 1] && flux[i] >= flux[i + 1])
            {
                double t = i / fps;
                if (t - last >= minGap) { out.push_back (t); last = t; }
            }
        }
        return out;
    }
}

//==============================================================================
float AnalysisData::frameValue (const std::vector<float>& v, double t) const
{
    if (v.empty()) return 0.0f;
    auto x = jlimit (0.0, (double) v.size() - 1.0, t * kFps);
    auto i = (int) x; auto fr = (float) (x - i);
    auto j = jmin ((int) v.size() - 1, i + 1);
    return v[(size_t) i] + (v[(size_t) j] - v[(size_t) i]) * fr;
}

int AnalysisData::countBefore (const std::vector<double>& list, double t) const
{
    return (int) (std::upper_bound (list.begin(), list.end(), t) - list.begin());
}

float AnalysisData::pulse (const std::vector<double>& list, double t, double decay) const
{
    auto n = countBefore (list, t);
    if (n == 0) return 0.0f;
    return (float) std::exp (-(t - list[(size_t) n - 1]) * decay);
}

String keyName (int root, bool minor)
{
    return String (noteNamesSharp()[((root % 12) + 12) % 12]) + (minor ? " MIN" : " MAJ");
}

//==============================================================================
std::shared_ptr<AnalysisData> analyse (AudioBuffer<float>&& audio, const CaptureInfo& info, const MidiData* midi)
{
    auto d = std::make_shared<AnalysisData>();
    d->audio = std::move (audio);
    d->info  = info;

    const int numCh = d->audio.getNumChannels();
    const int n     = d->audio.getNumSamples();
    const double sr = info.sampleRate;
    d->duration = n / sr;
    if (n == 0 || numCh == 0) return d;

    const float* L = d->audio.getReadPointer (0);
    const float* R = d->audio.getReadPointer (jmin (1, numCh - 1));

    // ---- band split (mono) -------------------------------------------------
    std::vector<float> mono ((size_t) n), lowSig ((size_t) n), midSig ((size_t) n), highSig ((size_t) n);
    d->bassSignal.resize ((size_t) n);
    {
        auto lp1 = Biquad::lowPass (sr, 150), lp2 = Biquad::lowPass (sr, 150);
        auto hp1 = Biquad::highPass (sr, 4000), hp2 = Biquad::highPass (sr, 4000);
        auto mh = Biquad::highPass (sr, 150), ml = Biquad::lowPass (sr, 4000);
        auto b1 = Biquad::lowPass (sr, 220), b2 = Biquad::lowPass (sr, 220), b3 = Biquad::highPass (sr, 25);
        for (int i = 0; i < n; ++i)
        {
            const float m = 0.5f * (L[i] + R[i]);
            mono[(size_t) i]    = m;
            lowSig[(size_t) i]  = lp2.process (lp1.process (m));
            highSig[(size_t) i] = hp2.process (hp1.process (m));
            midSig[(size_t) i]  = ml.process (mh.process (m));
            d->bassSignal[(size_t) i] = b3.process (b2.process (b1.process (m)));
        }
    }

    // ---- overview ----------------------------------------------------------
    const int N = AnalysisData::kOverview;
    d->ovMin.assign (N, 0); d->ovMax.assign (N, 0); d->ovRms.assign (N, 0);
    d->ovLow.assign (N, 0); d->ovMid.assign (N, 0); d->ovHigh.assign (N, 0);
    for (int b = 0; b < N; ++b)
    {
        int s0 = (int) ((int64) b * n / N), s1 = jmax (s0 + 1, (int) ((int64) (b + 1) * n / N));
        s1 = jmin (s1, n);
        float mn = 0, mx = 0;
        for (int i = s0; i < s1; ++i) { mn = jmin (mn, mono[(size_t) i]); mx = jmax (mx, mono[(size_t) i]); }
        d->ovMin[(size_t) b] = mn; d->ovMax[(size_t) b] = mx;
        d->ovRms[(size_t) b]  = rmsOf (mono.data() + s0, s1 - s0);
        d->ovLow[(size_t) b]  = rmsOf (lowSig.data() + s0, s1 - s0);
        d->ovMid[(size_t) b]  = rmsOf (midSig.data() + s0, s1 - s0);
        d->ovHigh[(size_t) b] = rmsOf (highSig.data() + s0, s1 - s0);
    }
    auto normalise = [] (std::vector<float>& v)
    {
        float m = 1e-6f; for (auto x : v) m = jmax (m, x);
        for (auto& x : v) x /= m;
    };
    normalise (d->ovLow); normalise (d->ovMid); normalise (d->ovHigh);

    // ---- loudness (BS.1770 K-weighting) -------------------------------------
    std::vector<float> kwL ((size_t) n), kwR ((size_t) n);
    {
        auto sL = Biquad::highShelf (sr, 1681.97, 4.0, 0.7072), hL = Biquad::highPass (sr, 38.13, 0.5003);
        auto sR = sL, hR = hL;
        for (int i = 0; i < n; ++i) { kwL[(size_t) i] = hL.process (sL.process (L[i])); kwR[(size_t) i] = hR.process (sR.process (R[i])); }
    }
    std::vector<double> kwSum ((size_t) n + 1, 0.0);
    for (int i = 0; i < n; ++i)
        kwSum[(size_t) i + 1] = kwSum[(size_t) i] + (double) kwL[(size_t) i] * kwL[(size_t) i] + (double) kwR[(size_t) i] * kwR[(size_t) i];
    auto msWindow = [&] (int end, int len)
    {
        end = jlimit (0, n, end);
        int s0 = jmax (0, end - len);
        return end > s0 ? (kwSum[(size_t) end] - kwSum[(size_t) s0]) / (end - s0) : 0.0;
    };
    auto toLufs = [] (double ms) { return (float) jmax (-70.0, -0.691 + 10.0 * std::log10 (ms + 1e-12)); };

    // ---- frames -------------------------------------------------------------
    const double fps = AnalysisData::kFps;
    const int F = (int) std::ceil (d->duration * fps);
    d->numFrames = F;
    for (auto* v : { &d->peakL, &d->peakR, &d->rmsL, &d->rmsR, &d->lufsM, &d->lufsS, &d->low, &d->mid, &d->high, &d->kickEnv, &d->bassHz })
        v->assign ((size_t) F, 0.0f);
    d->spectrum.assign ((size_t) F, {});
    d->chroma.assign ((size_t) F, {});

    constexpr int fftOrder = 12, fftSize = 1 << fftOrder;
    dsp::FFT fft (fftOrder);
    std::vector<float> window ((size_t) fftSize), fftBuf ((size_t) fftSize * 2);
    for (int i = 0; i < fftSize; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos (MathConstants<float>::twoPi * i / (fftSize - 1));

    // log band edges 30 Hz .. 16 kHz
    std::array<int, AnalysisData::kBands + 1> edges {};
    for (int b = 0; b <= AnalysisData::kBands; ++b)
    {
        double f = 30.0 * std::pow (16000.0 / 30.0, (double) b / AnalysisData::kBands);
        edges[(size_t) b] = jlimit (1, fftSize / 2 - 1, (int) std::round (f * fftSize / sr));
    }

    // bass pitch: decimated low-passed signal
    const int dec = jmax (1, (int) std::round (sr / 6000.0));
    const double dsr = sr / dec;
    std::vector<float> bassDec;
    bassDec.reserve ((size_t) (n / dec + 1));
    for (int i = 0; i < n; i += dec) bassDec.push_back (d->bassSignal[(size_t) i]);

    const int hop = (int) (sr / fps);
    for (int f = 0; f < F; ++f)
    {
        const int end = jmin (n, (f + 1) * hop);
        const int start = jmax (0, end - hop);
        const int len = end - start;
        if (len <= 0) continue;

        float pl = 0, pr = 0;
        for (int i = start; i < end; ++i) { pl = jmax (pl, std::abs (L[i])); pr = jmax (pr, std::abs (R[i])); }
        d->peakL[(size_t) f] = pl; d->peakR[(size_t) f] = pr;
        // meters use a 50 ms RMS window
        const int rl = (int) (0.05 * sr), rs = jmax (0, end - rl);
        d->rmsL[(size_t) f] = rmsOf (L + rs, end - rs);
        d->rmsR[(size_t) f] = rmsOf (R + rs, end - rs);
        d->low[(size_t) f]  = rmsOf (lowSig.data() + start, len);
        d->mid[(size_t) f]  = rmsOf (midSig.data() + start, len);
        d->high[(size_t) f] = rmsOf (highSig.data() + start, len);
        d->lufsM[(size_t) f] = toLufs (msWindow (end, (int) (0.4 * sr)));
        d->lufsS[(size_t) f] = toLufs (msWindow (end, (int) (3.0 * sr)));

        // spectrum + chroma
        const int centre = start + len / 2;
        std::fill (fftBuf.begin(), fftBuf.end(), 0.0f);
        for (int i = 0; i < fftSize; ++i)
        {
            int s = centre - fftSize / 2 + i;
            if (s >= 0 && s < n) fftBuf[(size_t) i] = mono[(size_t) s] * window[(size_t) i];
        }
        fft.performFrequencyOnlyForwardTransform (fftBuf.data());
        auto& sp = d->spectrum[(size_t) f];
        for (int b = 0; b < AnalysisData::kBands; ++b)
        {
            float m = 0;
            for (int k = edges[(size_t) b]; k <= jmax (edges[(size_t) b], edges[(size_t) b + 1] - 1); ++k) m = jmax (m, fftBuf[(size_t) k]);
            float db = Decibels::gainToDecibels (m / (fftSize * 0.25f), -90.0f);
            sp[(size_t) b] = jlimit (0.0f, 1.0f, (db + 72.0f) / 66.0f);
        }
        auto& ch = d->chroma[(size_t) f];
        for (int k = jmax (1, (int) (55.0 * fftSize / sr)); k < (int) (2000.0 * fftSize / sr); ++k)
        {
            double freq = k * sr / fftSize;
            int pc = ((int) std::round (12.0 * std::log2 (freq / 440.0)) + 69) % 12;
            ch[(size_t) pc] += fftBuf[(size_t) k] * fftBuf[(size_t) k];
        }
        float cm = 1e-9f; for (auto c : ch) cm = jmax (cm, c);
        for (auto& c : ch) c /= cm;

        // bass pitch by normalised autocorrelation (30–250 Hz)
        const int w = (int) (0.09 * dsr);
        const int de = jmin ((int) bassDec.size(), end / dec), ds = de - w;
        if (ds > 0 && d->low[(size_t) f] > 0.01f)
        {
            const int minLag = (int) (dsr / 250.0), maxLag = jmin (w - 1, (int) (dsr / 30.0));
            double e0 = 0; for (int i = ds; i < de; ++i) e0 += (double) bassDec[(size_t) i] * bassDec[(size_t) i];
            std::vector<double> rs ((size_t) (maxLag + 2), 0.0);
            double best = 0; int bestLag = 0;
            for (int lag = minLag; lag <= maxLag; ++lag)
            {
                double c = 0, e1 = 0;
                for (int i = ds + lag; i < de; ++i) { c += (double) bassDec[(size_t) i] * bassDec[(size_t) (i - lag)]; e1 += (double) bassDec[(size_t) (i - lag)] * bassDec[(size_t) (i - lag)]; }
                double r = c / std::sqrt ((e0 + 1e-12) * (e1 + 1e-12));
                rs[(size_t) lag] = r;
                if (r > best) { best = r; bestLag = lag; }
            }
            // avoid octave errors: prefer the shortest period that is almost as strong
            for (int lag = minLag + 1; lag < bestLag; ++lag)
                if (rs[(size_t) lag] >= best * 0.9 && rs[(size_t) lag] >= rs[(size_t) lag - 1] && rs[(size_t) lag] >= rs[(size_t) lag + 1])
                { bestLag = lag; break; }
            if (best > 0.75 && bestLag > 0) d->bassHz[(size_t) f] = (float) (dsr / bestLag);
        }
    }

    // integrated loudness (gated, 400 ms blocks, 75 % overlap)
    {
        std::vector<double> blocks;
        const int bl = (int) (0.4 * sr), step = bl / 4;
        for (int e = bl; e <= n; e += step) blocks.push_back (msWindow (e, bl));
        auto mean = [&] (double gateLufs)
        {
            double s = 0; int c = 0;
            for (auto b : blocks) if (toLufs (b) > gateLufs) { s += b; ++c; }
            return c ? s / c : 0.0;
        };
        double abs = mean (-70.0);
        double rel = toLufs (abs) - 10.0;
        d->integratedLufs = blocks.empty() ? toLufs (msWindow (n, n)) : toLufs (mean (rel));
    }

    // onsets
    d->kicks = pickOnsets (d->low,  fps, 0.12f, 0.12);
    {
        std::vector<float> full ((size_t) F);
        for (int f = 0; f < F; ++f) full[(size_t) f] = d->mid[(size_t) f] + d->high[(size_t) f] * 1.5f + d->low[(size_t) f] * 0.5f;
        d->hits = pickOnsets (full, fps, 0.08f, 0.07);
    }
    {
        float m = 1e-6f; for (auto v : d->low) m = jmax (m, v);
        for (int f = 0; f < F; ++f) d->kickEnv[(size_t) f] = jmin (1.0f, d->low[(size_t) f] / m);
    }

    detectKey (*d, midi);

    return d;
}

//==============================================================================
void detectKey (AnalysisData& d, const MidiData* midi)
{
    {
        std::array<double, 12> pcs {};
        if (midi != nullptr && ! midi->notes.empty())
            for (auto& nt : midi->notes) pcs[(size_t) (nt.note % 12)] += (nt.end - nt.start) * (0.5 + nt.velocity);
        else
            for (auto& c : d.chroma) for (int i = 0; i < 12; ++i) pcs[(size_t) i] += c[(size_t) i];

        static const double maj[] = { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 };
        static const double mnr[] = { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };
        auto corr = [&] (const double* prof, int root)
        {
            double mx = 0, my = 0;
            for (int i = 0; i < 12; ++i) { mx += pcs[(size_t) ((i + root) % 12)]; my += prof[i]; }
            mx /= 12; my /= 12;
            double sxy = 0, sxx = 0, syy = 0;
            for (int i = 0; i < 12; ++i)
            {
                double x = pcs[(size_t) ((i + root) % 12)] - mx, y = prof[i] - my;
                sxy += x * y; sxx += x * x; syy += y * y;
            }
            return sxy / std::sqrt (sxx * syy + 1e-12);
        };
        double best = -2;
        for (int r = 0; r < 12; ++r)
        {
            if (auto c = corr (maj, r); c > best) { best = c; d.keyRoot = r; d.keyMinor = false; }
            if (auto c = corr (mnr, r); c > best) { best = c; d.keyRoot = r; d.keyMinor = true; }
        }
    }

}

double estimateBpm (const AnalysisData& d)
{
    if (d.numFrames < 120) return 120.0;
    std::vector<float> env ((size_t) d.numFrames, 0.0f);
    for (int f = 1; f < d.numFrames; ++f)
    {
        auto e = [&] (int i) { return d.low[(size_t) i] + d.mid[(size_t) i] + d.high[(size_t) i]; };
        env[(size_t) f] = jmax (0.0f, e (f) - e (f - 1));
    }
    double bestScore = 0, bestBpm = 120;
    for (double bpm = 70; bpm <= 180; bpm += 0.5)
    {
        double lag = AnalysisData::kFps * 60.0 / bpm, s = 0;
        for (int mult = 1; mult <= 4; ++mult)
        {
            int l = (int) std::round (lag * mult);
            for (int f = l; f < d.numFrames; ++f) s += env[(size_t) f] * env[(size_t) (f - l)];
        }
        // gentle preference for the typical beat range
        s *= 1.0 - 0.15 * std::abs (bpm - 135.0) / 65.0;
        if (s > bestScore) { bestScore = s; bestBpm = bpm; }
    }
    return std::round (bestBpm);
}

//==============================================================================
std::shared_ptr<MidiData> MidiData::load (const File& file, int beatsPerBar, String& error)
{
    FileInputStream in (file);
    MidiFile mf;
    if (! in.openedOk() || ! mf.readFrom (in)) { error = "Couldn't read " + file.getFileName(); return {}; }

    const int tpq = mf.getTimeFormat();
    if (tpq <= 0) { error = "SMPTE-timed MIDI files aren't supported"; return {}; }

    auto md = std::make_shared<MidiData>();
    md->fileName = file.getFileNameWithoutExtension();
    double lastEnd = 0;
    for (int t = 0; t < mf.getNumTracks(); ++t)
    {
        MidiMessageSequence seq (*mf.getTrack (t));
        seq.updateMatchedPairs();
        for (int i = 0; i < seq.getNumEvents(); ++i)
        {
            auto* ev = seq.getEventPointer (i);
            if (! ev->message.isNoteOn()) continue;
            double s = ev->message.getTimeStamp() / tpq;
            double e = ev->noteOffObject != nullptr ? ev->noteOffObject->message.getTimeStamp() / tpq : s + 0.25;
            md->notes.push_back ({ s, jmax (s + 1.0 / 32.0, e), ev->message.getNoteNumber(), ev->message.getFloatVelocity() });
            lastEnd = jmax (lastEnd, e);
        }
    }
    if (md->notes.empty()) { error = file.getFileName() + " has no notes"; return {}; }

    std::sort (md->notes.begin(), md->notes.end(), [] (auto& a, auto& b) { return a.start < b.start; });
    md->lowest = 127; md->highest = 0;
    for (auto& nt : md->notes) { md->lowest = jmin (md->lowest, nt.note); md->highest = jmax (md->highest, nt.note); }
    const double bar = jmax (1, beatsPerBar);
    md->lengthQuarters = jmax (bar, std::ceil (lastEnd / bar - 1e-6) * bar);
    return md;
}

//==============================================================================
bool loadAudioFile (const File& f, AudioBuffer<float>& out, double& sampleRate, String& error)
{
    AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<AudioFormatReader> r (fm.createReaderFor (f));
    if (r == nullptr) { error = "Unsupported audio file: " + f.getFileName(); return false; }
    if (r->lengthInSamples > (int64) r->sampleRate * 60 * 20) { error = "Audio is longer than 20 minutes"; return false; }
    const int ch = jlimit (1, 2, (int) r->numChannels);
    out.setSize (2, (int) r->lengthInSamples);
    r->read (&out, 0, (int) r->lengthInSamples, 0, true, true);
    if (ch == 1) out.copyFrom (1, 0, out, 0, 0, out.getNumSamples());
    sampleRate = r->sampleRate;
    return true;
}

bool writeFloatWav (const File& f, const AudioBuffer<float>& buffer, double sampleRate)
{
    f.deleteFile();
    std::unique_ptr<OutputStream> os (f.createOutputStream());
    if (os == nullptr) return false;
    WavAudioFormat wav;
    std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (os.get(), sampleRate, (unsigned) buffer.getNumChannels(), 32, {}, 0));
    if (w == nullptr) return false;
    os.release();
    return w->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
}
}
