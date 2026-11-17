#include "Renderer.h"

namespace brl
{
Path patternGlyph (int pattern)
{
    Path p;
    switch (pattern)
    {
        case patHeart:
            p.startNewSubPath (0.5f, 0.92f);
            p.cubicTo (0.05f, 0.62f, 0.0f, 0.3f, 0.2f, 0.16f);
            p.cubicTo (0.36f, 0.05f, 0.48f, 0.18f, 0.5f, 0.3f);
            p.cubicTo (0.52f, 0.18f, 0.64f, 0.05f, 0.8f, 0.16f);
            p.cubicTo (1.0f, 0.3f, 0.95f, 0.62f, 0.5f, 0.92f);
            p.closeSubPath();
            break;
        case patSparkle:
            p.addStar ({ 0.5f, 0.5f }, 4, 0.12f, 0.5f);
            break;
        case patNote:
            p.addEllipse (0.12f, 0.62f, 0.36f, 0.28f);
            p.addRectangle (0.4f, 0.1f, 0.08f, 0.66f);
            p.addRectangle (0.4f, 0.1f, 0.38f, 0.1f);
            p.addRectangle (0.7f, 0.1f, 0.08f, 0.5f);
            p.addEllipse (0.48f, 0.52f, 0.3f, 0.24f);
            break;
        case patSmile:
        {
            p.addEllipse (0.05f, 0.05f, 0.9f, 0.9f);
            Path face;
            face.addEllipse (0.3f, 0.3f, 0.1f, 0.14f);
            face.addEllipse (0.6f, 0.3f, 0.1f, 0.14f);
            Path mouth; mouth.addCentredArc (0.5f, 0.52f, 0.25f, 0.22f, 0, MathConstants<float>::pi * 0.6f, MathConstants<float>::pi * 1.4f, true);
            PathStrokeType (0.07f, PathStrokeType::curved, PathStrokeType::rounded).createStrokedPath (mouth, mouth);
            face.addPath (mouth);
            p.setUsingNonZeroWinding (false);
            p.addPath (face);
            break;
        }
        default:
            p.addStar ({ 0.5f, 0.52f }, 5, 0.2f, 0.5f);
            break;
    }
    return p;
}

namespace
{
inline float easeOutCubic (float x) { x = jlimit (0.0f, 1.0f, x); return 1.0f - std::pow (1.0f - x, 3.0f); }
inline float easeInCubic (float x)  { x = jlimit (0.0f, 1.0f, x); return x * x * x; }
inline float easeInOut (float x)    { x = jlimit (0.0f, 1.0f, x); return x < 0.5f ? 4 * x * x * x : 1 - std::pow (-2 * x + 2, 3.0f) / 2; }
inline float easeOutBack (float x)
{
    x = jlimit (0.0f, 1.0f, x);
    const float c1 = 1.70158f, c3 = c1 + 1.0f;
    return 1.0f + c3 * std::pow (x - 1.0f, 3.0f) + c1 * std::pow (x - 1.0f, 2.0f);
}

//==============================================================================
struct Frame
{
    Graphics& g;
    const RenderContext& rc;
    const Style& st;
    const AnalysisData& d;
    const int W, H;
    const float s;            // design unit -> pixels (1080 reference on the short side)
    const double t;
    const float kick, hit;    // 0..1 decaying pulses
    const float react;
    int cardIndex = 0;

    // CARD DEAL intro: elements fly in one after another (deal < 0 = off)
    float deal = -1.0f;
    int dealIndex = 0, dealCount = 1;

    struct Dealt
    {
        Dealt (Frame& fr, Rectangle<float> r, bool fromTop = false) : f (fr), active (fr.deal >= 0.0f)
        {
            if (! active) return;
            const int k = f.dealIndex++;
            const float stagger = jmin (0.08f, 0.6f / (float) jmax (1, f.dealCount));
            const float span = jmax (0.3f, 1.0f - stagger * (float) (f.dealCount - 1));
            const float local = jlimit (0.0f, 1.0f, (f.deal - (float) k * stagger) / span);
            const float e = easeOutBack (local);
            f.g.saveState();
            const float dy = (1.0f - e) * (fromTop ? -1.0f : 1.0f) * (float) f.H * 0.75f;
            const float rot = (1.0f - e) * 0.3f * (k % 2 ? 1.0f : -1.0f);
            f.g.addTransform (AffineTransform::rotation (rot, r.getCentreX(), r.getCentreY()).translated (0.0f, dy));
            const float a = jlimit (0.0f, 1.0f, local * 3.0f);
            if (a < 0.999f) { f.g.beginTransparencyLayer (a); layer = true; }
        }
        ~Dealt()
        {
            if (! active) return;
            if (layer) f.g.endTransparencyLayer();
            f.g.restoreState();
        }
        Frame& f; bool active, layer = false;
    };

    Frame (Graphics& gr, int w, int h, const RenderContext& r)
        : g (gr), rc (r), st (*r.style), d (*r.data), W (w), H (h),
          s ((float) jmin (w, h) / 1080.0f), t (jlimit (0.0, jmax (0.0, r.data->duration), r.t)),
          kick (r.data->pulse (r.data->kicks, t, 6.0)), hit (r.data->pulse (r.data->hits, t, 9.0)),
          react (r.style->react)
    {}

    Font title (float h) const { return FontBank::get().titleFont (st, h * s); }
    Font body  (float h) const { return FontBank::get().bodyFont  (st, h * s); }

    double beatPos() const { return t / d.secondsPerBeat(); }
    double songQuarter() const { return d.info.startPpq + beatPos(); }
    int songBar (double tt) const
    {
        return (int) std::floor ((d.info.startPpq + tt / d.secondsPerBeat()) / d.info.beatsPerBar + 1e-6) + 1;
    }

    //==========================================================================
    void background()
    {
        auto full = Rectangle<float> (0, 0, (float) W, (float) H);
        g.setColour (st.bg1);
        g.fillRect (full);
        const float big = (float) jmax (W, H);
        const float p = kick * react;

        switch (st.background)
        {
            case bgGradient:
            {
                g.setGradientFill (ColourGradient (st.bg1, 0, 0, st.bg2, (float) W * 0.3f, (float) H, false));
                g.fillRect (full);
                g.setColour (st.accent.withAlpha (0.08f * p));
                g.fillRect (full);
                break;
            }
            case bgMesh:
            {
                struct Blob { Colour c; float sx, sy, ph, r; };
                const Blob blobs[] = {
                    { st.bg2,                      0.13f, 0.11f, 0.0f, 0.75f },
                    { st.accent.withAlpha (0.38f), 0.09f, 0.15f, 2.1f, 0.55f },
                    { st.accent2.withAlpha (0.28f),0.16f, 0.07f, 4.0f, 0.5f },
                    { st.bg2.brighter (0.2f),      0.07f, 0.12f, 5.3f, 0.45f } };
                for (auto& b : blobs)
                {
                    auto cx = (0.5f + 0.42f * std::sin ((float) t * b.sx * 6.2832f + b.ph)) * W;
                    auto cy = (0.5f + 0.42f * std::cos ((float) t * b.sy * 6.2832f + b.ph * 1.3f)) * H;
                    auto r = big * b.r * (1.0f + 0.06f * p);
                    ColourGradient gr (b.c, cx, cy, b.c.withAlpha (0.0f), cx + r, cy, true);
                    g.setGradientFill (gr);
                    g.fillEllipse (cx - r, cy - r, r * 2, r * 2);
                }
                break;
            }
            case bgDots:
            {
                const float sp = 36 * s, rad = 2.2f * s * (1.0f + 0.8f * p);
                g.setColour (st.dim.withAlpha (0.35f + 0.25f * p));
                for (float y = sp * 0.5f; y < H; y += sp)
                    for (float x = sp * 0.5f; x < W; x += sp)
                        g.fillEllipse (x - rad, y - rad, rad * 2, rad * 2);
                break;
            }
            case bgGrid:
            {
                const float sp = 54 * s;
                const float off = (float) std::fmod (beatPos() * sp * 0.25, (double) sp);
                g.setColour (st.dim.withAlpha (0.16f + 0.12f * p));
                for (float x = -off; x < W; x += sp) g.fillRect (x, 0.0f, jmax (1.0f, s), (float) H);
                for (float y = -off; y < H; y += sp) g.fillRect (0.0f, y, (float) W, jmax (1.0f, s));
                break;
            }
            case bgStripes:
            {
                const float sp = 90 * s;
                const float off = (float) std::fmod (t * 20.0 * s, (double) sp);
                g.setColour (st.bg2);
                for (float x = -H - sp + off; x < W + sp; x += sp)
                {
                    Path pth;
                    pth.startNewSubPath (x, (float) H); pth.lineTo (x + H, 0); pth.lineTo (x + H + sp * 0.45f, 0); pth.lineTo (x + sp * 0.45f, (float) H);
                    pth.closeSubPath();
                    g.fillPath (pth);
                }
                break;
            }
            case bgPattern:
            {
                g.setGradientFill (ColourGradient (st.bg1, 0, 0, st.bg2, 0, (float) H, false));
                g.fillRect (full);
                const float sz = 46 * s, sp = 110 * s;
                const float drift = (float) std::fmod (t * 14.0 * s, (double) sp);
                auto glyph = patternGlyph (st.pattern);
                int row = 0;
                for (float y = -sp + drift; y < H + sp; y += sp * 0.75f, ++row)
                    for (float x = -sp + drift + (row % 2) * sp * 0.5f; x < W + sp; x += sp)
                    {
                        auto tr = AffineTransform::scale (sz * (1.0f + 0.15f * p)).translated (x, y);
                        g.setColour ((row % 2 ? st.accent : st.dim).withAlpha (0.13f));
                        g.fillPath (glyph, tr);
                    }
                break;
            }
            case bgImage:
            {
                if (rc.backgroundImage != nullptr && rc.backgroundImage->isValid())
                {
                    auto zoom = 1.0f + 0.015f * p;
                    auto r = full.withSizeKeepingCentre (W * zoom, H * zoom);
                    g.drawImage (*rc.backgroundImage, r, RectanglePlacement::centred | RectanglePlacement::fillDestination);
                    g.setColour (Colours::black.withAlpha (jlimit (0.0f, 0.95f, st.bgImageDim)));
                    g.fillRect (full);
                }
                break;
            }
            default: break;
        }
    }

    //==========================================================================
    Rectangle<float> cardFrame (Rectangle<float> r, const String& label, const String& right = {})
    {
        ++cardIndex;
        const float p = kick * react;
        r = r.withSizeKeepingCentre (r.getWidth() * (1.0f + 0.02f * p), r.getHeight() * (1.0f + 0.02f * p));
        const float rad = st.radius * s, bw = jmax (1.0f, st.border * s);

        if (st.shadow > 0.0f)
        {
            g.setColour (st.cardBorder);
            g.fillRoundedRectangle (r.translated (st.shadow * s, st.shadow * s), rad);
        }
        if (st.glow)
            for (int i = 3; i >= 1; --i)
            {
                g.setColour (st.accent.withAlpha ((0.05f + 0.10f * p) / (float) i));
                g.drawRoundedRectangle (r.expanded (i * 3.0f * s), rad + i * 3.0f * s, 3.0f * s);
            }

        g.setColour (st.cardFill.withMultipliedAlpha (st.cardAlpha));
        g.fillRoundedRectangle (r, rad);
        g.setColour (st.glow ? st.cardBorder.interpolatedWith (st.accent, 0.5f * p) : st.cardBorder);
        g.drawRoundedRectangle (r.reduced (bw * 0.5f), rad, bw);

        auto inner = r.reduced (18 * s, 14 * s);
        auto head = inner.removeFromTop (28 * s);
        g.setColour (st.accent);
        g.fillEllipse (head.getX(), head.getCentreY() - 5 * s, 10 * s, 10 * s);
        g.setFont (body (19));
        g.setColour (st.dim);
        g.drawText (String (cardIndex).paddedLeft ('0', 2) + "  " + label, head.withTrimmedLeft (18 * s), Justification::centredLeft, false);
        if (right.isNotEmpty())
            g.drawText (right, head, Justification::centredRight, false);
        inner.removeFromTop (8 * s);
        return inner;
    }

    //==========================================================================
    void cardCoverBody (Rectangle<float> r)
    {
        auto c = r.getCentre();
        for (int i = 1; i <= 4; ++i)
        {
            float rad = jmin (r.getWidth(), r.getHeight()) * (0.14f * i + 0.04f * kick * react);
            g.setColour (st.dim.withAlpha (0.25f - 0.04f * i));
            g.drawEllipse (c.x - rad, c.y - rad, rad * 2, rad * 2, 1.5f * s);
        }
        auto tr = r.withSizeKeepingCentre (r.getWidth(), r.getHeight() * 0.55f);
        g.setColour (st.accent);
        g.setFont (title (r.getHeight() * 0.32f / s));
        g.drawFittedText (st.title, tr.toNearestInt(), Justification::centred, 2, 0.6f);
        g.setColour (st.dim);
        g.setFont (body (17));
        g.drawText (String (roundToInt (d.info.bpm)) + " BPM  " + keyName (d.keyRoot, d.keyMinor),
                    r.removeFromBottom (24 * s), Justification::centred, false);
    }

    void waveformLane (Rectangle<float> r, const std::vector<float>& mn, const std::vector<float>* mx,
                       double t0, double t1, Colour played, Colour upcoming, float gain = 1.0f)
    {
        const int cols = jmax (2, (int) (r.getWidth() / (3.0f * s)));
        const float bw = r.getWidth() / cols;
        const float mid = r.getCentreY(), half = r.getHeight() * 0.5f;
        for (int i = 0; i < cols; ++i)
        {
            double tt = t0 + (t1 - t0) * (i + 0.5) / cols;
            if (tt < 0 || tt > d.duration) continue;
            int b = jlimit (0, AnalysisData::kOverview - 1, (int) (tt / d.duration * AnalysisData::kOverview));
            float top, bot;
            if (mx != nullptr) { top = jmax (0.0f, (*mx)[(size_t) b]) * gain; bot = jmax (0.0f, -mn[(size_t) b]) * gain; }
            else               { top = bot = mn[(size_t) b] * gain * 0.5f; }
            top = jmin (1.0f, top); bot = jmin (1.0f, bot);
            g.setColour (tt <= t ? played : upcoming);
            g.fillRect (r.getX() + i * bw, mid - top * half, jmax (1.0f, bw * 0.7f), jmax (1.0f, (top + bot) * half));
        }
    }

    void cardWaveBody (Rectangle<float> r)
    {
        waveformLane (r, d.ovMin, &d.ovMax, 0.0, d.duration, st.accent, st.dim.withAlpha (0.55f), 1.1f);
        float x = r.getX() + (float) (t / jmax (1e-6, d.duration)) * r.getWidth();
        g.setColour (st.accent2);
        g.fillRect (x - 1.5f * s, r.getY(), 3 * s, r.getHeight());
    }

    void cardBeatBody (Rectangle<float> r)
    {
        const double win = d.secondsPerBar() * 2.0;
        const double t0 = t - win * 0.5, t1 = t + win * 0.5;
        auto ruler = r.removeFromTop (22 * s);
        auto xOf = [&] (double tt) { return r.getX() + (float) ((tt - t0) / (t1 - t0)) * r.getWidth(); };

        // bar lines + numbers
        g.setFont (body (16));
        const double spb = d.secondsPerBeat();
        const double firstBeat = std::ceil ((t0 / spb) - 1e-9);
        for (double b = firstBeat; b * spb <= t1; b += 1.0)
        {
            const double tt = b * spb;
            const double songBeat = d.info.startPpq + b;
            const bool isBar = std::abs (std::fmod (songBeat, (double) d.info.beatsPerBar)) < 1e-3;
            float x = xOf (tt);
            g.setColour (st.dim.withAlpha (isBar ? 0.55f : 0.18f));
            g.fillRect (x, r.getY(), jmax (1.0f, s), r.getHeight());
            if (isBar)
            {
                g.setColour (st.dim);
                g.drawText (String (songBar (tt + 1e-6)), Rectangle<float> (x + 4 * s, ruler.getY(), 60 * s, ruler.getHeight()), Justification::centredLeft, false);
            }
        }

        struct Lane { const char* name; const std::vector<float>* a; const std::vector<float>* b; float gain; };
        const Lane lanes[] = { { "MASTER", &d.ovMin, &d.ovMax, 1.1f }, { "LOW", &d.ovLow, nullptr, 1.8f },
                               { "MID", &d.ovMid, nullptr, 1.8f }, { "HIGH", &d.ovHigh, nullptr, 1.8f } };
        const int nLanes = 5;
        const float lh = r.getHeight() / nLanes;
        for (int i = 0; i < 4; ++i)
        {
            auto lr = Rectangle<float> (r.getX(), r.getY() + i * lh, r.getWidth(), lh).reduced (0, 3 * s);
            g.setColour (st.dim.withAlpha (0.08f));
            g.fillRect (lr);
            auto col = i == 0 ? st.accent : st.accent.interpolatedWith (st.accent2, i / 3.0f);
            waveformLane (lr, *lanes[i].a, lanes[i].b, t0, t1, col, st.text.withAlpha (0.35f), lanes[i].gain);
            g.setColour (st.dim);
            g.setFont (body (14));
            g.drawText (lanes[i].name, lr.reduced (6 * s, 2 * s), Justification::topLeft, false);
        }
        // hits lane
        auto hr = Rectangle<float> (r.getX(), r.getY() + 4 * lh, r.getWidth(), lh).reduced (0, 3 * s);
        g.setColour (st.dim.withAlpha (0.08f));
        g.fillRect (hr);
        auto ticks = [&] (const std::vector<double>& list, float hFrac, Colour c)
        {
            auto from = std::lower_bound (list.begin(), list.end(), t0);
            for (auto it = from; it != list.end() && *it <= t1; ++it)
            {
                float x = xOf (*it), hh = hr.getHeight() * hFrac;
                g.setColour (*it <= t ? c : st.text.withAlpha (0.35f));
                g.fillRect (x - 1.5f * s, hr.getBottom() - hh, 3 * s, hh);
            }
        };
        ticks (d.hits, 0.5f, st.accent2);
        ticks (d.kicks, 0.95f, st.accent);
        g.setColour (st.dim);
        g.setFont (body (14));
        g.drawText ("HITS", hr.reduced (6 * s, 2 * s), Justification::topLeft, false);

        // playhead
        float px = xOf (t);
        g.setColour (st.accent2);
        g.fillRect (px - 1.5f * s, r.getY() - 22 * s, 3 * s, r.getHeight() + 22 * s);
    }

    void cardMidiBody (Rectangle<float> r)
    {
        const auto* md = rc.midi;
        auto vel = r.removeFromBottom (r.getHeight() * 0.16f);
        auto gutter = r.removeFromLeft (44 * s);

        if (md == nullptr || md->notes.empty())
        {
            // fallback: audio chroma scrolling grid
            const double win = d.secondsPerBar() * 2.0, step = d.secondsPerBeat() / 4.0;
            const double t0 = t - win * 0.5;
            const float rowH = r.getHeight() / 12.0f;
            const int cols = (int) std::round (win / step);
            const float cw = r.getWidth() / cols;
            for (int pc = 0; pc < 12; ++pc)
            {
                float y = r.getBottom() - (pc + 1) * rowH;
                g.setColour (st.dim);
                g.setFont (body (14));
                g.drawText (noteNamesSharp()[pc], Rectangle<float> (gutter.getX(), y, gutter.getWidth(), rowH), Justification::centredLeft, false);
                for (int c = 0; c < cols; ++c)
                {
                    double tt = std::floor (t0 / step + c) * step;
                    if (tt < 0 || tt > d.duration) continue;
                    float v = d.chroma[(size_t) d.frameIndex (tt)][(size_t) pc];
                    v = v * v * v;
                    if (v < 0.15f) continue;
                    g.setColour ((tt <= t ? st.accent : st.text).withAlpha (0.2f + 0.8f * v));
                    float x = r.getX() + (float) ((tt - t0) / win) * r.getWidth();
                    g.fillRoundedRectangle (x + 1, y + 1, cw - 2, rowH - 2, 2 * s);
                }
            }
            float px = r.getCentreX();
            g.setColour (st.accent2);
            g.fillRect (px - 1.5f * s, r.getY(), 3 * s, r.getHeight() + vel.getHeight());
            // energy lollipops
            for (int c = 0; c < cols; ++c)
            {
                double tt = std::floor (t0 / step + c) * step;
                if (tt < 0 || tt > d.duration) continue;
                float v = jmin (1.0f, d.frameValue (d.mid, tt) * 6.0f);
                float x = r.getX() + (float) ((tt - t0) / win) * r.getWidth() + cw * 0.5f;
                float y = vel.getBottom() - v * vel.getHeight() * 0.85f;
                g.setColour ((tt <= t ? st.accent : st.dim).withAlpha (0.8f));
                g.fillRect (x - 0.75f * s, y, 1.5f * s, vel.getBottom() - y);
                g.fillEllipse (x - 3 * s, y - 3 * s, 6 * s, 6 * s);
            }
            return;
        }

        const double bpb = d.info.beatsPerBar;
        double q = st.midiAlign == midiFromSongPosition ? songQuarter() : beatPos();
        q = std::fmod (q, md->lengthQuarters);
        if (q < 0) q += md->lengthQuarters;
        const double page = jmin (md->lengthQuarters, bpb * 2.0);
        const double p0 = std::floor (q / page) * page;

        int lo = md->lowest, hi = md->highest;
        while (hi - lo < 11) { if (lo > 0) --lo; if (hi - lo < 11) ++hi; }
        const int rows = hi - lo + 1;
        const float rowH = r.getHeight() / rows;
        auto xOf = [&] (double qq) { return r.getX() + (float) ((qq - p0) / page) * r.getWidth(); };

        for (int n = lo; n <= hi; ++n)
        {
            float y = r.getBottom() - (n - lo + 1) * rowH;
            bool black = String (noteNamesSharp()[n % 12]).contains ("#");
            g.setColour (st.dim.withAlpha (black ? 0.12f : 0.05f));
            g.fillRect (r.getX(), y, r.getWidth(), rowH);
            if (rowH * 0.8f > 9 * s || n % 12 == 0)
            {
                g.setColour (st.dim);
                g.setFont (body (jmin (15.0f, rowH * 0.8f / s)));
                g.drawText (noteNamesSharp()[n % 12], Rectangle<float> (gutter.getX(), y, gutter.getWidth(), rowH), Justification::centredLeft, false);
            }
        }
        for (double b = 0; b <= page + 1e-6; b += 1.0)
        {
            g.setColour (st.dim.withAlpha (std::fmod (b, bpb) < 1e-6 ? 0.45f : 0.15f));
            g.fillRect (xOf (p0 + b), r.getY(), jmax (1.0f, s), r.getHeight());
        }

        for (auto& nt : md->notes)
        {
            if (nt.end <= p0 || nt.start >= p0 + page) continue;
            float x0 = jmax (r.getX(), xOf (nt.start)), x1 = jmin (r.getRight(), xOf (nt.end));
            float y = r.getBottom() - (nt.note - lo + 1) * rowH;
            const bool active = nt.start <= q && q < nt.end, played = nt.end <= q;
            auto box = Rectangle<float> (x0, y + 1, jmax (2.0f, x1 - x0 - 1), jmax (2.0f, rowH - 2));
            if (active)
            {
                g.setColour (st.accent2.withAlpha (0.35f));
                g.fillRoundedRectangle (box.expanded (3 * s), 3 * s);
            }
            g.setColour (active ? st.accent2 : st.accent.withAlpha (played ? 1.0f : 0.45f));
            g.fillRoundedRectangle (box, jmin (3 * s, rowH * 0.3f));
            if (box.getWidth() > 40 * s && rowH > 16 * s)
            {
                g.setColour (st.cardFill.withAlpha (0.9f));
                g.setFont (body (jmin (14.0f, rowH * 0.7f / s)));
                g.drawText (midiNoteName (nt.note), box.reduced (4 * s, 0), Justification::centredLeft, false);
            }
            // velocity lollipop
            float vx = xOf (nt.start) + 2 * s;
            if (vx >= r.getX() && vx <= r.getRight())
            {
                float vy = vel.getBottom() - nt.velocity * vel.getHeight() * 0.85f;
                g.setColour ((played || active) ? st.accent : st.dim);
                g.fillRect (vx - 0.75f * s, vy, 1.5f * s, vel.getBottom() - vy);
                g.fillEllipse (vx - 3 * s, vy - 3 * s, 6 * s, 6 * s);
            }
        }
        float px = xOf (q);
        g.setColour (st.text);
        g.fillRect (px - 1.5f * s, r.getY(), 3 * s, r.getHeight() + vel.getHeight());
    }

    Path signalPath (Rectangle<float> r, const float* src, int64 total, int64 start, int len, float gain)
    {
        Path p;
        const int pts = jmin (len, (int) (r.getWidth() / (1.5f * s)) + 1);
        for (int i = 0; i < pts; ++i)
        {
            int64 si = start + (int64) i * len / pts;
            float v = (si >= 0 && si < total) ? src[si] * gain : 0.0f;
            float x = r.getX() + r.getWidth() * i / (pts - 1);
            float y = r.getCentreY() - jlimit (-1.0f, 1.0f, v) * r.getHeight() * 0.46f;
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        return p;
    }

    /** Rising zero crossing at or before `centre`. Always returns an index inside [0, total). */
    int64 triggerStart (const float* src, int64 total, int64 centre, int searchLen)
    {
        if (total <= 1) return 0;
        centre = jlimit ((int64) 1, total - 1, centre);
        for (int64 i = centre; i > jmax ((int64) 1, centre - searchLen); --i)
            if (src[i - 1] < 0.0f && src[i] >= 0.0f) return i;
        return centre;
    }

    void cardScopeBody (Rectangle<float> r)
    {
        const auto sr = d.info.sampleRate;
        const int len = (int) (0.040 * sr);
        const int64 total = d.audio.getNumSamples();
        std::vector<float> mono ((size_t) len);
        const int64 start = triggerStart (d.bassSignal.data(), total, d.sampleIndex (t) - len, (int) (0.02 * sr));
        float mx = 0.05f;
        auto* L = d.audio.getReadPointer (0);
        auto* R = d.audio.getReadPointer (jmin (1, d.audio.getNumChannels() - 1));
        for (int i = 0; i < len; ++i)
        {
            int64 si = start + i;
            mono[(size_t) i] = (si >= 0 && si < total) ? 0.5f * (L[si] + R[si]) : 0.0f;
            mx = jmax (mx, std::abs (mono[(size_t) i]));
        }
        g.setColour (st.dim.withAlpha (0.25f));
        g.fillRect (r.getX(), r.getCentreY(), r.getWidth(), jmax (1.0f, s));
        g.setColour (st.dim.withAlpha (0.7f));
        g.strokePath (signalPath (r, mono.data(), len, 0, len, 0.9f / mx), PathStrokeType (1.5f * s));
        g.setColour (st.accent);
        g.strokePath (signalPath (r, d.bassSignal.data(), total, start, len, 0.9f / mx),
                      PathStrokeType (3.5f * s, PathStrokeType::curved, PathStrokeType::rounded));
    }

    void cardBassBody (Rectangle<float> r)
    {
        auto head = r.removeFromTop (r.getHeight() * 0.32f);
        const float hz = d.bassHz[(size_t) d.frameIndex (t)];
        String note = "--";
        if (hz > 0) note = midiNoteName ((int) std::round (69 + 12 * std::log2 (hz / 440.0)));
        g.setColour (st.dim);
        g.setFont (body (16));
        g.drawText ("SUB", head.removeFromLeft (head.getWidth() * 0.25f), Justification::centredLeft, false);
        g.setColour (st.accent);
        g.setFont (title (head.getHeight() * 0.8f / s));
        g.drawText (note, head, Justification::centredLeft, false);

        const auto sr = d.info.sampleRate;
        const int len = (int) (0.06 * sr);
        const int64 total = (int64) d.bassSignal.size();
        const int64 start = triggerStart (d.bassSignal.data(), total, d.sampleIndex (t) - len, (int) (0.03 * sr));
        float mx = 0.02f;
        for (int64 i = jmax ((int64) 0, start); i < jmin (total, start + len); ++i) mx = jmax (mx, std::abs (d.bassSignal[(size_t) i]));
        g.setColour (st.accent.withAlpha (0.25f));
        g.strokePath (signalPath (r, d.bassSignal.data(), total, start, len, 0.9f / mx), PathStrokeType (8 * s));
        g.setColour (st.accent);
        g.strokePath (signalPath (r, d.bassSignal.data(), total, start, len, 0.9f / mx),
                      PathStrokeType (3 * s, PathStrokeType::curved, PathStrokeType::rounded));
    }

    void cardLevelsBody (Rectangle<float> r)
    {
        auto meters = r.removeFromLeft (r.getWidth() * 0.26f);
        auto toY = [] (float gain) { return jlimit (0.0f, 1.0f, (Decibels::gainToDecibels (gain, -60.0f) + 60.0f) / 60.0f); };
        auto mw = meters.getWidth() / 2.0f;
        for (int c = 0; c < 2; ++c)
        {
            auto m = Rectangle<float> (meters.getX() + c * mw, meters.getY(), mw, meters.getHeight() - 20 * s).reduced (4 * s, 0);
            g.setColour (st.dim.withAlpha (0.15f));
            g.fillRect (m);
            float rms = d.frameValue (c == 0 ? d.rmsL : d.rmsR, t), pk = d.frameValue (c == 0 ? d.peakL : d.peakR, t);
            float h = toY (rms * 1.41f) * m.getHeight();
            g.setGradientFill (ColourGradient (st.accent, 0, m.getBottom(), st.accent2, 0, m.getY(), false));
            g.fillRect (m.withTop (m.getBottom() - h));
            g.setColour (st.text);
            g.fillRect (m.getX(), m.getBottom() - toY (pk) * m.getHeight(), m.getWidth(), 2 * s);
            g.setColour (st.dim);
            g.setFont (body (14));
            g.drawText (c == 0 ? "L" : "R", Rectangle<float> (m.getX(), m.getBottom() + 2 * s, m.getWidth(), 18 * s), Justification::centred, false);
        }
        r.removeFromLeft (12 * s);
        auto top = r.removeFromTop (r.getHeight() * 0.45f);
        g.setColour (st.text);
        {
            const String v (d.frameValue (d.lufsS, t), 1);
            auto box = top.withTrimmedRight (34 * s);
            auto f = title (box.getHeight() * 0.8f / s);
            const float tw = GlyphArrangement::getStringWidth (f, v);
            if (tw > box.getWidth()) f = f.withHeight (f.getHeight() * box.getWidth() / tw);
            g.setFont (f);
            g.drawText (v, box, Justification::centredLeft, false);
        }
        g.setColour (st.dim);
        g.setFont (body (14));
        g.drawText ("LUFS", top, Justification::topRight, false);
        g.drawText ("INT " + String (d.integratedLufs, 1), top, Justification::bottomRight, false);
        // history
        Path p;
        const int pts = 80;
        for (int i = 0; i <= pts; ++i)
        {
            double tt = d.duration * i / pts;
            if (tt > t) break;
            float v = jlimit (0.0f, 1.0f, (d.frameValue (d.lufsS, tt) + 30.0f) / 30.0f);
            float x = r.getX() + r.getWidth() * i / pts, y = r.getBottom() - v * r.getHeight();
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        g.setColour (st.accent);
        g.strokePath (p, PathStrokeType (2 * s));
    }

    void cardVectorBody (Rectangle<float> r)
    {
        auto sq = r.withSizeKeepingCentre (jmin (r.getWidth(), r.getHeight()), jmin (r.getWidth(), r.getHeight()));
        auto c = sq.getCentre();
        const float rad = sq.getWidth() * 0.5f;
        g.setColour (st.dim.withAlpha (0.3f));
        g.drawEllipse (sq.reduced (2 * s), 1.2f * s);
        g.drawLine (c.x - rad, c.y, c.x + rad, c.y, s);
        g.drawLine (c.x, c.y - rad, c.x, c.y + rad, s);
        g.setFont (body (14));
        g.setColour (st.dim);
        g.drawText ("L", Rectangle<float> (sq.getX(), sq.getY(), 20 * s, 20 * s), Justification::centred, false);
        g.drawText ("R", Rectangle<float> (sq.getRight() - 20 * s, sq.getY(), 20 * s, 20 * s), Justification::centred, false);

        const int64 total = d.audio.getNumSamples();
        const int64 end = jmin (total, d.sampleIndex (t));
        const int len = 2048;
        auto* L = d.audio.getReadPointer (0);
        auto* R = d.audio.getReadPointer (jmin (1, d.audio.getNumChannels() - 1));
        float mx = 0.2f;
        for (int64 i = jmax ((int64) 0, end - len); i < end; ++i) mx = jmax (mx, std::abs (L[i]), std::abs (R[i]));
        const float k = rad * 0.7f / mx;
        const float dot = 2.4f * s;
        g.setColour (st.accent.withAlpha (0.35f));
        for (int64 i = jmax ((int64) 0, end - len); i < end; i += 2)
        {
            float x = c.x + (R[i] - L[i]) * 0.7071f * k, y = c.y - (L[i] + R[i]) * 0.7071f * k;
            g.fillRect (x, y, dot, dot);
        }
    }

    void cardRadialBody (Rectangle<float> r)
    {
        auto c = r.getCentre();
        const float maxR = jmin (r.getWidth(), r.getHeight()) * 0.5f;
        const float inner = maxR * 0.38f;
        const auto& sp = d.spectrum[(size_t) d.frameIndex (t)];
        const int bars = AnalysisData::kBands * 2;
        for (int i = 0; i < bars; ++i)
        {
            int b = i < AnalysisData::kBands ? i : bars - 1 - i;
            float v = sp[(size_t) b];
            float a = MathConstants<float>::twoPi * i / bars - MathConstants<float>::halfPi;
            float len = (maxR - inner) * v;
            g.setColour (st.accent.interpolatedWith (st.accent2, (float) b / AnalysisData::kBands).withAlpha (0.4f + 0.6f * v));
            g.drawLine (c.x + std::cos (a) * inner, c.y + std::sin (a) * inner,
                        c.x + std::cos (a) * (inner + len), c.y + std::sin (a) * (inner + len), 2.4f * s);
        }
        const float dsz = inner * (0.55f + 0.25f * kick * react);
        Path dm;
        dm.addStar (c, 4, dsz * 0.55f, dsz, 0);
        g.setColour (st.accent);
        g.fillPath (dm);
        g.setColour (st.cardFill);
        g.fillEllipse (c.x - dsz * 0.18f, c.y - dsz * 0.18f, dsz * 0.36f, dsz * 0.36f);
    }

    void cardSpectrumBody (Rectangle<float> r)
    {
        const auto& sp = d.spectrum[(size_t) d.frameIndex (t)];
        const int n = 32;
        const float bw = r.getWidth() / n;
        for (int i = 0; i < n; ++i)
        {
            float v = jmax (sp[(size_t) i * 2], sp[(size_t) i * 2 + 1]);
            auto bar = Rectangle<float> (r.getX() + i * bw + bw * 0.12f, r.getBottom() - v * r.getHeight(), bw * 0.76f, v * r.getHeight());
            g.setGradientFill (ColourGradient (st.accent, 0, r.getBottom(), st.accent2, 0, r.getY(), false));
            g.fillRect (bar);
            g.setColour (st.dim.withAlpha (0.15f));
            g.fillRect (bar.withTop (r.getY()).withBottom (bar.getY()));
        }
    }

    void cardOrbitBody (Rectangle<float> r)
    {
        auto c = r.getCentre();
        const float maxR = jmin (r.getWidth(), r.getHeight()) * 0.48f;
        const double beats = beatPos();
        const float energy[] = { d.frameValue (d.low, t), d.frameValue (d.mid, t), d.frameValue (d.high, t) };
        for (int ring = 0; ring < 3; ++ring)
        {
            float rr = maxR * (0.45f + 0.27f * ring);
            g.setColour (st.dim.withAlpha (0.3f));
            g.drawEllipse (c.x - rr, c.y - rr, rr * 2, rr * 2, 1.2f * s);
            const double period = d.info.beatsPerBar * (1 << ring);
            const float a = (float) (MathConstants<double>::twoPi * std::fmod (beats / period, 1.0)) * (ring == 1 ? -1.0f : 1.0f);
            Path trail;
            trail.addCentredArc (c.x, c.y, rr, rr, 0, a + (ring == 1 ? 0.9f : -0.9f), a, true);
            g.setColour (st.accent.withAlpha (0.5f));
            g.strokePath (trail, PathStrokeType (3 * s, PathStrokeType::curved, PathStrokeType::rounded));
            float ps = (7 + jmin (1.0f, energy[ring] * 8.0f) * 9) * s;
            float px = c.x + std::sin (a) * rr, py = c.y - std::cos (a) * rr;
            g.setColour (ring == 0 ? st.accent : st.accent2);
            g.fillEllipse (px - ps * 0.5f, py - ps * 0.5f, ps, ps);
        }
        Path star;
        float sr = maxR * (0.22f + 0.1f * kick * react);
        star.addStar (c, 5, sr * 0.45f, sr, (float) (beats * 0.25));
        g.setColour (st.accent);
        g.fillPath (star);
    }

    void cardTrackBody (Rectangle<float> r)
    {
        const int passed = d.countBefore (d.hits, t);
        int score = 0;
        for (int i = 0; i < passed; ++i) score += 100 + 10 * jmin (i, 50);
        auto head = r.removeFromTop (22 * s);
        g.setFont (body (15));
        g.setColour (st.dim);
        g.drawText ("SCORE " + String (score).paddedLeft ('0', 7), head, Justification::centredLeft, false);
        g.setColour (st.accent);
        g.drawText (String (passed) + " COMBO x" + String (1 + passed / 25), head, Justification::centredRight, false);

        const int lanes = 4;
        const float vanishY = r.getY(), hitY = r.getBottom() - 14 * s;
        auto laneX = [&] (int i, float y)
        {
            float f = (y - vanishY) / (hitY - vanishY);
            float topW = r.getWidth() * 0.18f, botW = r.getWidth() * 0.96f;
            float w = topW + (botW - topW) * f;
            return r.getCentreX() - w * 0.5f + w * i / lanes;
        };
        g.setColour (st.dim.withAlpha (0.4f));
        for (int i = 0; i <= lanes; ++i) g.drawLine (laneX (i, vanishY), vanishY, laneX (i, hitY), hitY, 1.2f * s);
        g.setColour (st.accent.withAlpha (0.6f + 0.4f * hit));
        g.drawLine (laneX (0, hitY), hitY, laneX (lanes, hitY), hitY, 3 * s);

        const double ahead = 1.4;
        auto from = std::lower_bound (d.hits.begin(), d.hits.end(), t);
        for (auto it = from; it != d.hits.end() && *it < t + ahead; ++it)
        {
            const double ht = *it;
            const int fi = d.frameIndex (ht);
            const float lo = d.low[(size_t) fi], mi = d.mid[(size_t) fi], hi = d.high[(size_t) fi];
            int lane = lo > mi && lo > hi * 2 ? 0 : (hi > mi ? 3 : (fi % 2 ? 1 : 2));
            float f = 1.0f - (float) ((ht - t) / ahead);
            f = f * f;
            float y = vanishY + (hitY - vanishY) * f;
            float x0 = laneX (lane, y), x1 = laneX (lane + 1, y);
            float h = (4 + 10 * f) * s;
            g.setColour (lane == 0 ? st.accent : st.accent2);
            g.fillRoundedRectangle (x0 + (x1 - x0) * 0.1f, y - h * 0.5f, (x1 - x0) * 0.8f, h, 2 * s);
        }
        if (hit > 0.3f)
        {
            g.setColour (st.text.withAlpha (hit));
            g.setFont (title (24));
            g.drawText ("PERFECT", r.withTrimmedBottom (r.getHeight() * 0.45f), Justification::centred, false);
        }
    }

    void cardDotsBody (Rectangle<float> r)
    {
        const auto& sp = d.spectrum[(size_t) d.frameIndex (t)];
        const int cols = 16, rows = 10;
        const float cw = r.getWidth() / cols, rh = r.getHeight() / rows;
        const float dot = jmin (cw, rh) * 0.62f;
        for (int c = 0; c < cols; ++c)
        {
            float v = 0;
            for (int k = c * 4; k < c * 4 + 4; ++k) v = jmax (v, sp[(size_t) k]);
            int lit = (int) std::round (v * rows);
            for (int rr = 0; rr < rows; ++rr)
            {
                float x = r.getX() + c * cw + (cw - dot) * 0.5f, y = r.getBottom() - (rr + 1) * rh + (rh - dot) * 0.5f;
                g.setColour (rr < lit ? (rr == lit - 1 ? st.accent2 : st.accent) : st.dim.withAlpha (0.15f));
                g.fillEllipse (x, y, dot, dot);
            }
        }
    }

    //==========================================================================
    /** Notes sounding right now: from the .mid when loaded, otherwise the strongest audio pitch classes + the bass note. */
    std::vector<int> activeNotes() const
    {
        std::vector<int> out;
        if (rc.midi != nullptr && ! rc.midi->notes.empty())
        {
            double q = st.midiAlign == midiFromSongPosition ? songQuarter() : beatPos();
            q = std::fmod (jmax (0.0, q), rc.midi->lengthQuarters);
            for (auto& nt : rc.midi->notes)
                if (nt.start <= q && q < nt.end) out.push_back (nt.note);
            return out;
        }
        const auto& ch = d.chroma[(size_t) d.frameIndex (t)];
        for (int pc = 0; pc < 12; ++pc)
            if (ch[(size_t) pc] > 0.75f) out.push_back (60 + pc);
        const float hz = d.bassHz[(size_t) d.frameIndex (t)];
        if (hz > 0) out.push_back ((int) std::round (69 + 12 * std::log2 (hz / 440.0)));
        return out;
    }

    void cardPianoBody (Rectangle<float> r)
    {
        auto notes = activeNotes();
        // as many octaves as fit with readable keys (1-4), starting at the lowest note in use
        const int octaves = jlimit (1, 4, (int) (r.getWidth() / (18 * s)) / 7);
        int lo = 48;
        if (rc.midi != nullptr && ! rc.midi->notes.empty()) lo = (rc.midi->lowest / 12) * 12;
        else if (! notes.empty()) lo = (*std::min_element (notes.begin(), notes.end()) / 12) * 12;
        // keep the notes playing right now on screen
        if (! notes.empty())
        {
            const int top = *std::max_element (notes.begin(), notes.end());
            while (top > lo + octaves * 12 - 1) lo += 12;
        }
        const int hi = lo + octaves * 12 - 1;
        auto isBlack = [] (int n) { const int pc = n % 12; return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10; };
        auto on = [&] (int n) { return std::find (notes.begin(), notes.end(), n) != notes.end(); };

        auto label = r.removeFromTop (26 * s);
        String names;
        for (auto n : notes) names << midiNoteName (n) << " ";
        g.setColour (st.accent);
        g.setFont (body (18));
        g.drawText (names.isEmpty() ? String ("-") : names.trim(), label, Justification::centredLeft, true);
        r.removeFromTop (6 * s);

        int whites = 0;
        for (int n = lo; n <= hi; ++n) if (! isBlack (n)) ++whites;
        const float ww = r.getWidth() / (float) jmax (1, whites);
        const float rad = jmin (4 * s, ww * 0.2f);
        int wi = 0;
        std::vector<std::pair<int, float>> blacks;
        for (int n = lo; n <= hi; ++n)
        {
            if (isBlack (n)) { blacks.push_back ({ n, r.getX() + wi * ww }); continue; }
            auto k = Rectangle<float> (r.getX() + wi * ww, r.getY(), ww, r.getHeight()).reduced (s, 0);
            g.setColour (on (n) ? st.accent : Colour (0xfff3f1ec).interpolatedWith (st.cardFill, 0.12f));
            g.fillRoundedRectangle (k, rad);
            g.setColour (st.cardBorder.withAlpha (0.6f));
            g.drawRoundedRectangle (k, rad, s);
            if (n % 12 == 0 && ww > 14 * s)
            {
                g.setColour (on (n) ? st.accent.contrasting() : st.dim);
                g.setFont (body (jmin (14.0f, ww * 0.5f / s)));
                g.drawText ("C" + String (n / 12 - 1), k.removeFromBottom (20 * s), Justification::centred, false);
            }
            ++wi;
        }
        for (auto& [n, x] : blacks)
        {
            auto k = Rectangle<float> (x - ww * 0.32f, r.getY(), ww * 0.64f, r.getHeight() * 0.6f);
            g.setColour (on (n) ? st.accent2 : Colour (0xff17161a).interpolatedWith (st.cardBorder, 0.15f));
            g.fillRoundedRectangle (k, rad);
        }
    }

    void cardTempoBody (Rectangle<float> r)
    {
        const int bpb = jmax (1, d.info.beatsPerBar);
        const double songBeat = songQuarter();
        const int beatInBar = ((int) std::floor (songBeat + 1e-6)) % bpb;
        const float beatFrac = (float) (songBeat - std::floor (songBeat));

        auto top = r.removeFromTop (r.getHeight() * 0.5f);
        {
            const String bpmText (roundToInt (d.info.bpm));
            auto box = top.removeFromLeft (top.getWidth() * 0.6f);
            auto f = title (box.getHeight() * 0.85f / s);
            const float tw = GlyphArrangement::getStringWidth (f, bpmText);
            if (tw > box.getWidth() * 0.95f) f = f.withHeight (f.getHeight() * box.getWidth() * 0.95f / tw);
            g.setColour (st.text);
            g.setFont (f);
            g.drawText (bpmText, box, Justification::centredLeft, false);
        }
        g.setColour (st.dim);
        g.setFont (body (15));
        g.drawText ("BPM", top.removeFromTop (top.getHeight() * 0.5f), Justification::bottomLeft, false);
        g.setColour (st.accent);
        g.drawText ("BAR " + String (songBar (t)) + "." + String (beatInBar + 1), top, Justification::topLeft, false);

        // pendulum
        auto pend = r.removeFromBottom (r.getHeight() * 0.45f);
        const float swing = std::sin ((float) (songBeat * MathConstants<double>::pi));
        const Point<float> pivot (pend.getCentreX(), pend.getY());
        const float len = pend.getHeight() * 0.95f;
        const Point<float> bob (pivot.x + swing * len * 0.6f, pivot.y + len * std::sqrt (jmax (0.0f, 1.0f - swing * swing * 0.36f)));
        g.setColour (st.dim);
        g.drawLine ({ pivot, bob }, 2.5f * s);
        g.setColour (st.accent);
        g.fillEllipse (Rectangle<float> (14 * s, 14 * s).withCentre (bob));

        // beat squares
        r = r.reduced (0, 6 * s);
        const float gap = 6 * s, w = (r.getWidth() - gap * (bpb - 1)) / bpb;
        for (int i = 0; i < bpb; ++i)
        {
            auto b = Rectangle<float> (r.getX() + i * (w + gap), r.getY(), w, r.getHeight());
            const bool cur = i == beatInBar;
            g.setColour (cur ? (i == 0 ? st.accent2 : st.accent).withAlpha (1.0f - 0.5f * beatFrac) : st.dim.withAlpha (i < beatInBar ? 0.4f : 0.15f));
            g.fillRoundedRectangle (b, st.radius * s * 0.3f);
        }
    }

    void cardVinylBody (Rectangle<float> r)
    {
        const float k = kick * react;
        auto disc = r.withSizeKeepingCentre (jmin (r.getWidth(), r.getHeight()), jmin (r.getWidth(), r.getHeight())).reduced (4 * s);
        const auto c = disc.getCentre();
        const float rad = disc.getWidth() * 0.5f * (1.0f + 0.02f * k);
        const float angle = (float) (t * MathConstants<double>::twoPi * 33.333 / 60.0);
        g.setColour (Colour (0xff0d0d0f));
        g.fillEllipse (c.x - rad, c.y - rad, rad * 2, rad * 2);
        for (float gr = rad * 0.95f; gr > rad * 0.4f; gr -= rad * 0.045f)
        {
            g.setColour (Colours::white.withAlpha (0.05f));
            g.drawEllipse (c.x - gr, c.y - gr, gr * 2, gr * 2, 1.0f * s);
        }
        // sheen that rotates
        Path sheen;
        sheen.addPieSegment (c.x - rad, c.y - rad, rad * 2, rad * 2, angle, angle + 0.5f, 0.4f);
        g.setColour (Colours::white.withAlpha (0.08f));
        g.fillPath (sheen);
        sheen.applyTransform (AffineTransform::rotation (MathConstants<float>::pi, c.x, c.y));
        g.fillPath (sheen);
        // label
        const float lr = rad * 0.36f;
        g.setColour (st.accent);
        g.fillEllipse (c.x - lr, c.y - lr, lr * 2, lr * 2);
        {
            Graphics::ScopedSaveState ss (g);
            g.addTransform (AffineTransform::rotation (angle, c.x, c.y));
            g.setColour (st.accent.contrasting());
            g.setFont (title (jmax (6.0f, lr * 0.42f / s)));
            g.drawFittedText (st.title, Rectangle<float> (lr * 1.6f, lr * 0.7f).withCentre (c).toNearestInt(), Justification::centred, 1, 0.5f);
        }
        g.setColour (st.cardFill);
        g.fillEllipse (c.x - 4 * s, c.y - 4 * s, 8 * s, 8 * s);
    }

    void cardSpectrogramBody (Rectangle<float> r)
    {
        const double win = 4.0;                       // seconds of history, "now" at the right edge
        const int cols = jmax (8, (int) (r.getWidth() / (5.0f * s))), rows = 40;
        const float cw = r.getWidth() / cols, rh = r.getHeight() / rows;
        for (int c = 0; c < cols; ++c)
        {
            const double tt = t - win + win * (c + 1) / cols;
            if (tt < 0) continue;
            const auto& sp = d.spectrum[(size_t) d.frameIndex (tt)];
            for (int row = 0; row < rows; ++row)
            {
                const float v = sp[(size_t) (row * AnalysisData::kBands / rows)];
                if (v < 0.08f) continue;
                const Colour col = v < 0.6f ? st.cardFill.interpolatedWith (st.accent, v / 0.6f)
                                            : st.accent.interpolatedWith (st.accent2, (v - 0.6f) / 0.4f);
                g.setColour (col);
                g.fillRect (r.getX() + c * cw, r.getBottom() - (row + 1) * rh, cw + 0.5f, rh + 0.5f);
            }
        }
        g.setColour (st.text);
        g.fillRect (r.getRight() - 2 * s, r.getY(), 2 * s, r.getHeight());
        g.setColour (st.dim);
        g.setFont (body (13));
        g.drawText ("16k", r.reduced (4 * s), Justification::topLeft, false);
        g.drawText ("30", r.reduced (4 * s), Justification::bottomLeft, false);
    }

    void cardStepsBody (Rectangle<float> r)
    {
        const double spb = d.secondsPerBeat(), s16 = spb / 4.0;
        const int steps = d.info.beatsPerBar * 4;
        const double songBeat = songQuarter();
        const double barStartBeat = std::floor (songBeat / d.info.beatsPerBar) * d.info.beatsPerBar;
        const double barStartT = (barStartBeat - d.info.startPpq) * spb;
        const int cur = jlimit (0, steps - 1, (int) std::floor ((t - barStartT) / s16));

        auto hasOnset = [&] (const std::vector<double>& list, double a, double b)
        {
            auto it = std::lower_bound (list.begin(), list.end(), a - s16 * 0.3);
            return it != list.end() && *it < b - s16 * 0.3;
        };
        struct Row { const char* name; int kind; };
        const Row rows[] = { { "KICK", 0 }, { "HIT", 1 }, { "HIGH", 2 }, { "LOW", 3 } };
        const int nRows = 4;
        auto labels = r.removeFromLeft (44 * s);
        const float gap = 3 * s;
        const float cw = (r.getWidth() - gap * (steps - 1)) / steps, rh = (r.getHeight() - gap * (nRows - 1)) / nRows;
        for (int ri = 0; ri < nRows; ++ri)
        {
            const float y = r.getY() + ri * (rh + gap);
            g.setColour (st.dim);
            g.setFont (body (13));
            g.drawText (rows[ri].name, Rectangle<float> (labels.getX(), y, labels.getWidth(), rh), Justification::centredLeft, false);
            for (int i = 0; i < steps; ++i)
            {
                const double a = barStartT + i * s16, b = a + s16;
                bool lit = false;
                switch (rows[ri].kind)
                {
                    case 0: lit = hasOnset (d.kicks, a, b); break;
                    case 1: lit = hasOnset (d.hits, a, b) && ! hasOnset (d.kicks, a, b); break;
                    case 2: lit = a >= 0 && d.frameValue (d.high, a + s16 * 0.25) > d.frameValue (d.high, jmax (0.0, a - s16)) * 1.4f; break;
                    default: lit = a >= 0 && d.frameValue (d.low, a + s16 * 0.25) > 0.6f * *std::max_element (d.low.begin(), d.low.end()); break;
                }
                auto cell = Rectangle<float> (r.getX() + i * (cw + gap), y, cw, rh);
                const bool isCur = i == cur;
                Colour col = lit ? (i <= cur ? (ri == 0 ? st.accent : st.accent2) : st.text.withAlpha (0.45f))
                                 : st.dim.withAlpha (i % 4 == 0 ? 0.22f : 0.12f);
                if (isCur && lit) col = col.brighter (0.3f);
                g.setColour (col);
                g.fillRoundedRectangle (cell, jmin (4 * s, cw * 0.25f));
                if (isCur)
                {
                    g.setColour (st.text);
                    g.drawRoundedRectangle (cell.expanded (s), jmin (4 * s, cw * 0.25f), 1.5f * s);
                }
            }
        }
    }

    void drawCard (int id, Rectangle<float> r)
    {
        String right;
        switch (id)
        {
            case cardBeat:   right = String (roundToInt (d.info.bpm)) + " BPM"; break;
            case cardScope:  right = "40 ms"; break;
            case cardLevels: right = "SHORT-TERM"; break;
            case cardMidi:
                if (rc.midi != nullptr && rc.midi->lengthQuarters > 0)
                {
                    double q = st.midiAlign == midiFromSongPosition ? songQuarter() : beatPos();
                    const double page = jmin (rc.midi->lengthQuarters, d.info.beatsPerBar * 2.0);
                    int pages = jmax (1, (int) std::ceil (rc.midi->lengthQuarters / page - 1e-6));
                    int cur = (int) std::floor (std::fmod (jmax (0.0, q), rc.midi->lengthQuarters) / page) + 1;
                    right = "PAGE " + String (cur) + "/" + String (pages);
                }
                else right = "AUDIO KEYS";
                break;
            case cardBass:   right = "TRIG"; break;
            case cardPiano:  right = rc.midi != nullptr ? "MIDI" : "AUDIO"; break;
            case cardTempo:  right = String (d.info.beatsPerBar) + "/4"; break;
            case cardVinyl:  right = "33 RPM"; break;
            case cardSpectrogram: right = "4 S"; break;
            case cardSteps:  right = "16 STEPS"; break;
            default: break;
        }
        Dealt dealt (*this, r);
        auto inner = cardFrame (r, cardName (id), right);
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (inner.expanded (2 * s).toNearestInt());
        switch (id)
        {
            case cardCover:    cardCoverBody (inner); break;
            case cardWave:     cardWaveBody (inner); break;
            case cardBeat:     cardBeatBody (inner); break;
            case cardMidi:     cardMidiBody (inner); break;
            case cardScope:    cardScopeBody (inner); break;
            case cardBass:     cardBassBody (inner); break;
            case cardLevels:   cardLevelsBody (inner); break;
            case cardVector:   cardVectorBody (inner); break;
            case cardRadial:   cardRadialBody (inner); break;
            case cardSpectrum: cardSpectrumBody (inner); break;
            case cardOrbit:    cardOrbitBody (inner); break;
            case cardTrack:    cardTrackBody (inner); break;
            case cardDots:     cardDotsBody (inner); break;
            case cardPiano:    cardPianoBody (inner); break;
            case cardTempo:    cardTempoBody (inner); break;
            case cardVinyl:    cardVinylBody (inner); break;
            case cardSpectrogram: cardSpectrogramBody (inner); break;
            case cardSteps:    cardStepsBody (inner); break;
            default: break;
        }
    }

    //==========================================================================
    void header (Rectangle<float> r)
    {
        auto meta = r.removeFromBottom (r.getHeight() * 0.28f);
        g.setColour (st.text);
        g.setFont (title (r.getHeight() * 0.92f / s));
        g.drawFittedText (st.title, r.toNearestInt(), Justification::centred, 1, 0.5f);

        AttributedString as;
        as.setJustification (Justification::centred);
        auto f = body (22);
        as.append (String (roundToInt (d.info.bpm)) + " BPM", f, st.accent);
        as.append ("   " + keyName (d.keyRoot, d.keyMinor), f, st.dim);
        if (st.subtitle.isNotEmpty()) as.append ("   " + st.subtitle, f, st.dim);
        as.draw (g, meta);
    }

    int highlightPitchClass() const
    {
        const float hz = d.bassHz[(size_t) d.frameIndex (t)];
        if (rc.midi != nullptr && ! rc.midi->notes.empty())
        {
            double q = st.midiAlign == midiFromSongPosition ? songQuarter() : beatPos();
            q = std::fmod (jmax (0.0, q), rc.midi->lengthQuarters);
            int best = -1;
            for (auto& nt : rc.midi->notes)
                if (nt.start <= q && q < nt.end && (best < 0 || nt.note > best)) best = nt.note;
            if (best >= 0) return best % 12;
        }
        if (hz > 0) return ((int) std::round (69 + 12 * std::log2 (hz / 440.0))) % 12;
        const auto& ch = d.chroma[(size_t) d.frameIndex (t)];
        return (int) (std::max_element (ch.begin(), ch.end()) - ch.begin());
    }

    void keys (Rectangle<float> r)
    {
        static const int majS[] = { 0, 2, 4, 5, 7, 9, 11 }, minS[] = { 0, 2, 3, 5, 7, 8, 10 };
        const int* sc = d.keyMinor ? minS : majS;
        const int hl = highlightPitchClass();
        const float gap = 8 * s;
        const float w = (r.getWidth() - gap * 7) / 8.0f;
        auto box = [&] (int i) { return Rectangle<float> (r.getX() + i * (w + gap), r.getY(), w, r.getHeight()); };

        g.setColour (st.text);
        g.fillRoundedRectangle (box (0), st.radius * s * 0.5f);
        g.setColour (st.bg1.getPerceivedBrightness() > 0.5f ? st.bg1 : st.cardFill);
        g.setColour (st.text.contrasting());
        g.setFont (body (20));
        g.drawText (keyName (d.keyRoot, d.keyMinor), box (0), Justification::centred, false);
        for (int i = 0; i < 7; ++i)
        {
            const int pc = (d.keyRoot + sc[i]) % 12;
            auto b = box (i + 1);
            const bool on = pc == hl;
            g.setColour (on ? st.accent : st.cardFill.withMultipliedAlpha (st.cardAlpha));
            g.fillRoundedRectangle (b, st.radius * s * 0.5f);
            g.setColour (on ? st.accent : st.cardBorder);
            g.drawRoundedRectangle (b.reduced (s), st.radius * s * 0.5f, jmax (1.0f, st.border * s * 0.7f));
            g.setColour (on ? st.accent.contrasting() : st.text);
            g.drawText (noteNamesSharp()[pc], b, Justification::centred, false);
        }
    }

    void progress (Rectangle<float> r)
    {
        auto label = r.removeFromRight (150 * s);
        auto line = r.withSizeKeepingCentre (r.getWidth(), 4 * s);
        g.setColour (st.dim.withAlpha (0.35f));
        g.fillRoundedRectangle (line, 2 * s);
        const float f = (float) (t / jmax (1e-6, d.duration));
        g.setColour (st.accent);
        g.fillRoundedRectangle (line.withWidth (line.getWidth() * f), 2 * s);
        g.fillEllipse (line.getX() + line.getWidth() * f - 7 * s, line.getCentreY() - 7 * s, 14 * s, 14 * s);
        g.setColour (st.text);
        g.setFont (body (20));
        g.drawText (formatTime (t) + " / " + formatTime (d.duration), label, Justification::centredRight, false);
    }

    //==========================================================================
    static float cardWeight (int id) { return id == cardMidi ? 1.5f : id == cardBeat ? 1.35f : id == cardWave ? 0.65f : id == cardSpectrogram ? 0.9f : 1.0f; }

    void layoutColumn (Rectangle<float> area, const std::vector<int>& wide, const std::vector<int>& small, int cols, float gap)
    {
        struct Row { std::vector<int> ids; float weight; };
        std::vector<Row> rows;
        for (int id : wide) rows.push_back ({ { id }, cardWeight (id) });
        for (size_t i = 0; i < small.size(); i += (size_t) cols)
        {
            Row row { {}, 1.0f };
            for (size_t k = i; k < jmin (small.size(), i + (size_t) cols); ++k) row.ids.push_back (small[k]);
            rows.push_back (row);
        }
        if (rows.empty()) return;
        float total = 0; for (auto& row : rows) total += row.weight;
        const float avail = area.getHeight() - gap * (float) (rows.size() - 1);
        float y = area.getY();
        for (auto& row : rows)
        {
            const float h = avail * row.weight / total;
            const float cw = (area.getWidth() - gap * (float) (row.ids.size() - 1)) / (float) row.ids.size();
            for (size_t i = 0; i < row.ids.size(); ++i)
                drawCard (row.ids[i], { area.getX() + (float) i * (cw + gap), y, cw, h });
            y += h + gap;
        }
    }

    void render()
    {
        background();

        const float m = 44 * s, gap = 20 * s;
        auto area = Rectangle<float> (0, 0, (float) W, (float) H).reduced (m);
        const bool landscape = W > H;

        if (st.showFooter && st.footerText.isNotEmpty())
        {
            auto f = area.removeFromBottom (30 * s);
            g.setColour (st.dim);
            g.setFont (body (17));
            g.drawText (st.footerText, f, Justification::centred, false);
            area.removeFromBottom (8 * s);
        }
        std::vector<int> wide, small;
        for (int id : { cardBeat, cardMidi, cardSpectrogram, cardWave }) if (st.hasCard (id)) wide.push_back (id);
        for (int id = 0; id < kNumCards; ++id) if (st.hasCard (id) && ! cardIsWide (id)) small.push_back (id);
        dealCount = (int) (wide.size() + small.size()) + (st.showHeader ? 1 : 0) + (st.showKeys ? 1 : 0) + (st.showProgress ? 1 : 0);

        Rectangle<float> headerR, keysR, progR;
        if (st.showProgress) { progR = area.removeFromBottom (36 * s); area.removeFromBottom (gap); }
        if (st.showKeys)     { keysR = area.removeFromBottom (68 * s); area.removeFromBottom (gap); }
        if (st.showHeader)   { headerR = area.removeFromTop (landscape ? 120 * s : 165 * s); area.removeFromTop (gap); }
        if (st.showHeader)   { Dealt dl (*this, headerR, true); header (headerR); }

        if (landscape && ! wide.empty() && ! small.empty())
        {
            auto left = area.removeFromLeft (area.getWidth() * 0.58f);
            area.removeFromLeft (gap);
            layoutColumn (left, wide, {}, 1, gap);
            layoutColumn (area, {}, small, 2, gap);
        }
        else
        {
            const int cols = landscape ? 4 : (small.size() <= 4 && st.aspect != aspect9x16 ? 2 : 3);
            layoutColumn (area, wide, small, small.size() == 4 && ! landscape ? 2 : cols, gap);
        }
        if (st.showKeys)     { Dealt dl (*this, keysR); keys (keysR); }
        if (st.showProgress) { Dealt dl (*this, progR); progress (progR); }
    }
};
} // namespace

IntroTiming introTiming (const Style& st, const AnalysisData& d)
{
    IntroTiming it;
    it.active = st.intro != introNone;
    if (! it.active) return it;
    double len = jlimit (0.25, 8.0, (double) st.introLength);
    if (st.introSnap)
    {
        const double spb = d.secondsPerBeat();
        len = jmax (1.0, std::round (len / spb)) * spb;
    }
    it.length = len;
    it.transition = jlimit (0.0, 2.0, (double) st.introTransition);
    it.preroll = st.introMode == introBeforeBeat;
    return it;
}

double videoDuration (const Style& st, const AnalysisData& d)
{
    return d.duration + introTiming (st, d).audioOffset();
}

namespace
{
void drawWatermark (Graphics& g, int W, int H)
{
    const float s = (float) jmin (W, H) / 1080.0f;
    Graphics::ScopedSaveState ss (g);
    g.addTransform (AffineTransform::rotation (-0.5f, W * 0.5f, H * 0.5f));
    g.setFont (FontBank::get().font (1, {}, 110 * s));
    for (int pass = 0; pass < 2; ++pass)
    {
        g.setColour (pass == 0 ? Colours::black.withAlpha (0.18f) : Colours::white.withAlpha (0.22f));
        const float o = pass == 0 ? 3 * s : 0.0f;
        for (int i = -4; i <= 4; ++i)
            g.drawText ("BEATREEL DEMO  BEATREEL DEMO", Rectangle<float> (-W * 0.5f + o, H * 0.5f + i * 260 * s + o, W * 2.0f, 140 * s),
                        Justification::centred, false);
    }
}

//==============================================================================
/** The six intro animations. T = video time, inner = context at the matching beat time. */
struct Intro
{
    Graphics& g;
    const int W, H;
    const float s;
    const RenderContext& inner;
    const Style& st;
    const AnalysisData& d;
    const IntroTiming it;
    const double T;

    Intro (Graphics& gr, int w, int h, const RenderContext& in, const IntroTiming& timing, double videoT)
        : g (gr), W (w), H (h), s ((float) jmin (w, h) / 1080.0f), inner (in), st (*in.style), d (*in.data), it (timing), T (videoT) {}

    Rectangle<float> full() const { return { 0, 0, (float) W, (float) H }; }
    Font titleFont (float h) const { return FontBank::get().titleFont (st, h); }
    Font bodyFont (float h) const  { return FontBank::get().bodyFont (st, h); }
    float kick() const             { return it.preroll ? 0.0f : d.pulse (d.kicks, inner.t, 6.0) * st.react; }
    String meta() const
    {
        String m = String (roundToInt (d.info.bpm)) + " BPM   " + keyName (d.keyRoot, d.keyMinor);
        if (st.subtitle.isNotEmpty()) m << "   " << st.subtitle;
        return m;
    }
    /** Biggest font (up to maxH) that fits `text` into `width`. */
    Font fitted (Font f, const String& text, float width, float maxH) const
    {
        f = f.withHeight (maxH);
        const float tw = GlyphArrangement::getStringWidth (f, text);
        return tw > width ? f.withHeight (maxH * width / tw) : f;
    }

    void reel (Graphics& gg, float deal = -1.0f)
    {
        Frame f (gg, W, H, inner);
        f.deal = deal;
        f.render();
    }

    void gradientBg()
    {
        g.setGradientFill (ColourGradient (st.bg1, 0, 0, st.bg2, (float) W * 0.4f, (float) H, false));
        g.fillRect (full());
    }

    //==========================================================================
    void slamScene (float p)
    {
        gradientBg();
        const auto c = full().getCentre();
        const float big = (float) jmax (W, H);
        // rotating rays
        {
            Path rays;
            const int n = 18;
            const float rot = (float) T * 0.35f;
            for (int i = 0; i < n; ++i)
            {
                const float a0 = rot + MathConstants<float>::twoPi * i / n, a1 = a0 + MathConstants<float>::twoPi / n * 0.45f;
                rays.startNewSubPath (c);
                rays.lineTo (c.x + std::cos (a0) * big, c.y + std::sin (a0) * big);
                rays.lineTo (c.x + std::cos (a1) * big, c.y + std::sin (a1) * big);
                rays.closeSubPath();
            }
            g.setColour (st.accent.withAlpha (0.10f + 0.08f * kick()));
            g.fillPath (rays);
        }
        const float impact = 0.3f;
        const float dt = (p - impact) * (float) it.length;   // seconds since impact
        float scale, alpha = 1.0f;
        Point<float> shake;
        if (p < impact)
        {
            scale = 3.4f - 2.4f * easeInCubic (p / impact);
            alpha = jmin (1.0f, p / impact * 1.6f);
        }
        else
        {
            scale = 1.0f + 0.07f * std::exp (-dt * 8.0f) * std::cos (dt * 32.0f);
            const float amp = 28.0f * s * std::exp (-dt * 7.0f);
            shake = { std::sin (dt * 93.0f) * amp, std::cos (dt * 71.0f) * amp };
        }
        const String title = st.title.toUpperCase();
        auto f = fitted (titleFont (1), title, W * 0.84f, 300 * s);
        const float th = f.getHeight();
        auto box = Rectangle<float> (0, c.y - th * 0.6f, (float) W, th * 1.2f);
        {
            Graphics::ScopedSaveState ss (g);
            g.addTransform (AffineTransform::scale (scale, scale, c.x, c.y).translated (shake));
            g.setFont (f);
            g.setColour (st.accent.withMultipliedAlpha (alpha));
            g.drawText (title, box.translated (10 * s, 10 * s), Justification::centred, false);
            g.setColour (st.text.withMultipliedAlpha (alpha));
            g.drawText (title, box, Justification::centred, false);
        }
        if (dt > 0)
        {
            const float tw = jmin ((float) W * 0.84f, GlyphArrangement::getStringWidth (f, title));
            const float grow = easeOutCubic (dt * 3.0f);
            g.setColour (st.accent);
            g.fillRect (Rectangle<float> (tw * grow, 12 * s).withCentre ({ c.x, box.getBottom() + 18 * s }).translated (shake.x, shake.y));
            const float ma = jlimit (0.0f, 1.0f, (dt - 0.12f) * 4.0f);
            g.setColour (st.dim.withAlpha (ma));
            g.setFont (bodyFont (34 * s));
            g.drawText (meta(), Rectangle<float> (0, box.getBottom() + 44 * s, (float) W, 50 * s), Justification::centred, false);
            g.setColour (Colours::white.withAlpha (0.85f * std::exp (-dt * 11.0f)));
            g.fillRect (full());
        }
    }

    //==========================================================================
    void typewriterScene (float p)
    {
        g.setColour (st.bg1);
        g.fillRect (full());
        g.setColour (st.text.withAlpha (0.05f));
        for (float y = 0; y < H; y += 6 * s) g.fillRect (0.0f, y, (float) W, jmax (1.0f, s));

        const Font mono = FontBank::get().font (6, {}, 10);
        const float margin = W * 0.1f;
        const float width = W - margin * 2;
        const String title = st.title;
        auto big = fitted (mono, "> " + title + "_", width, 120 * s);
        const float lh = big.getHeight() * 1.25f;
        const float y0 = H * 0.5f - lh;
        const bool blink = std::fmod (T, 0.6) < 0.3;

        auto typed = [&] (const String& txt, float from, float to) { return txt.substring (0, (int) std::floor (txt.length() * jlimit (0.0f, 1.0f, (p - from) / (to - from)))); };

        // line 1: the title
        const String t1 = typed (title, 0.02f, 0.5f);
        g.setFont (big);
        g.setColour (st.accent);
        g.drawText (">", Rectangle<float> (margin, y0, width, lh), Justification::centredLeft, false);
        const float px = GlyphArrangement::getStringWidth (big, "> ");
        g.setColour (st.text);
        g.drawText (t1, Rectangle<float> (margin + px, y0, width - px, lh), Justification::centredLeft, false);
        const bool typing1 = p < 0.55f;
        if (typing1 && (blink || p < 0.5f))
        {
            const float cx = margin + px + GlyphArrangement::getStringWidth (big, t1);
            g.setColour (st.accent);
            g.fillRect (cx + 4 * s, y0 + lh * 0.18f, big.getHeight() * 0.5f, lh * 0.64f);
        }
        // line 2: tempo + key
        auto small = mono.withHeight (jmin (54 * s, big.getHeight() * 0.5f));
        const float y1 = y0 + lh + 10 * s, lh2 = small.getHeight() * 1.4f;
        if (p >= 0.55f)
        {
            g.setFont (small);
            g.setColour (st.accent);
            g.drawText (">", Rectangle<float> (margin, y1, width, lh2), Justification::centredLeft, false);
            g.setColour (st.dim);
            const float sp = GlyphArrangement::getStringWidth (small, "> ");
            g.drawText (typed (meta(), 0.55f, 0.75f), Rectangle<float> (margin + sp, y1, width - sp, lh2), Justification::centredLeft, false);
        }
        // line 3: loading bar
        if (p >= 0.78f)
        {
            const float lp = jlimit (0.0f, 1.0f, (p - 0.78f) / 0.2f);
            const float y2 = y1 + lh2 + 6 * s;
            g.setFont (small);
            g.setColour (st.accent);
            g.drawText ("> LOADING BEAT " + String (roundToInt (lp * 100)) + "%", Rectangle<float> (margin, y2, width, lh2), Justification::centredLeft, false);
            auto bar = Rectangle<float> (margin, y2 + lh2 + 8 * s, width, 14 * s);
            g.setColour (st.dim.withAlpha (0.3f));
            g.fillRect (bar);
            g.setColour (st.accent);
            g.fillRect (bar.withWidth (bar.getWidth() * lp));
        }
    }

    //==========================================================================
    void countdownScene (float p)
    {
        gradientBg();
        const auto c = full().getCentre();
        const float k = kick();
        for (int i = 1; i <= 3; ++i)
        {
            const float r = jmin (W, H) * (0.18f * i + 0.02f * k);
            g.setColour (st.dim.withAlpha (0.18f));
            g.drawEllipse (c.x - r, c.y - r, r * 2, r * 2, 2 * s);
        }
        const float seg = 0.25f;
        if (p < seg * 3)
        {
            const int idx = jmin (2, (int) (p / seg));
            const float local = (p - idx * seg) / seg;
            const float sc = 1.9f - 0.9f * easeOutBack (jmin (1.0f, local * 2.5f));
            const float a = local < 0.82f ? 1.0f : jmax (0.0f, (1.0f - local) / 0.18f);
            // ring burst
            const float rr = jmin (W, H) * (0.12f + 0.4f * easeOutCubic (local));
            g.setColour (st.accent.withAlpha (0.8f * (1.0f - local)));
            g.drawEllipse (c.x - rr, c.y - rr, rr * 2, rr * 2, 8 * s * (1.0f - local) + s);
            Graphics::ScopedSaveState ss (g);
            g.addTransform (AffineTransform::scale (sc, sc, c.x, c.y));
            g.setFont (titleFont (420 * s));
            g.setColour (st.accent.withMultipliedAlpha (a));
            g.drawText (String (3 - idx), full().translated (12 * s, 12 * s), Justification::centred, false);
            g.setColour (st.text.withMultipliedAlpha (a));
            g.drawText (String (3 - idx), full(), Justification::centred, false);
        }
        else
        {
            const float local = (p - seg * 3) / (1.0f - seg * 3);
            const float sc = easeOutBack (jmin (1.0f, local * 2.0f));
            const String title = st.title.toUpperCase();
            auto f = fitted (titleFont (1), title, W * 0.84f, 240 * s);
            Graphics::ScopedSaveState ss (g);
            g.addTransform (AffineTransform::scale (jmax (0.01f, sc), jmax (0.01f, sc), c.x, c.y));
            g.setFont (f);
            g.setColour (st.text);
            g.drawText (title, full(), Justification::centred, false);
            g.setFont (bodyFont (34 * s));
            g.setColour (st.accent);
            g.drawText (meta(), full().withTrimmedTop (f.getHeight() * 1.5f), Justification::centred, false);
        }
        // three progress dots
        for (int i = 0; i < 3; ++i)
        {
            const bool on = p >= i * seg;
            const float r = 9 * s;
            const float x = c.x + (i - 1) * 40 * s;
            g.setColour (on ? st.accent : st.dim.withAlpha (0.4f));
            g.fillEllipse (x - r, H * 0.86f - r, r * 2, r * 2);
        }
    }

    //==========================================================================
    void irisScene (float p)
    {
        g.setColour (st.bg1);
        g.fillRect (full());
        const auto c = full().getCentre();
        const float in = easeOutCubic (p / 0.4f);
        const float k = kick();
        // dashed rotating ring
        const float rr = jmin (W, H) * (0.36f + 0.015f * k);
        Path ring;
        ring.addCentredArc (c.x, c.y, rr, rr, (float) T * 0.8f, 0, MathConstants<float>::twoPi * in, true);
        Path dashed;
        const float dashes[] = { 26 * s, 16 * s };
        PathStrokeType (5 * s).createDashedStroke (dashed, ring, dashes, 2);
        g.setColour (st.accent);
        g.fillPath (dashed);

        const String title = st.title.toUpperCase();
        auto f = fitted (titleFont (1), title, rr * 1.7f, 200 * s);
        const float sc = 0.85f + 0.15f * in;
        Graphics::ScopedSaveState ss (g);
        g.addTransform (AffineTransform::scale (sc, sc, c.x, c.y));
        g.setFont (f);
        g.setColour (st.text.withAlpha (in));
        g.drawText (title, full().translated (0, -f.getHeight() * 0.15f), Justification::centred, false);
        g.setFont (bodyFont (30 * s));
        g.setColour (st.dim.withAlpha (jlimit (0.0f, 1.0f, (p - 0.3f) * 4.0f)));
        g.drawText (meta(), full().withTrimmedTop (f.getHeight() * 1.2f), Justification::centred, false);
    }

    //==========================================================================
    void glitch (float amount, float titleAlpha)
    {
        Image reelImg (Image::RGB, W, H, false, SoftwareImageType());
        { Graphics gi (reelImg); reel (gi); }
        Random rng ((int64) std::floor (T * 24.0) * 7919 + 17);

        g.drawImageAt (reelImg, 0, 0);
        if (amount > 0.01f)
        {
            // horizontal tears
            const int slices = 6 + (int) (14 * amount);
            for (int i = 0; i < slices; ++i)
            {
                const int y = rng.nextInt (H), hh = jmax (2, (int) (rng.nextFloat() * H * 0.08f * amount) + 2);
                const int dx = (int) ((rng.nextFloat() - 0.5f) * W * 0.25f * amount);
                g.setOpacity (1.0f);
                g.drawImage (reelImg, dx, y, W, hh, 0, y, W, hh);
            }
            // colour ghosts
            g.setOpacity (0.35f * amount);
            g.drawImageAt (reelImg, (int) (14 * s * amount), 0);
            g.setOpacity (1.0f);
            for (int i = 0; i < (int) (10 * amount); ++i)
            {
                g.setColour ((i % 2 ? st.accent : st.accent2).withAlpha (0.55f));
                g.fillRect (rng.nextFloat() * W, rng.nextFloat() * H, rng.nextFloat() * W * 0.3f * amount, rng.nextFloat() * 18 * s + 3 * s);
            }
            g.setColour (Colours::black.withAlpha (0.25f * amount));
            for (float y = 0; y < H; y += 4 * s) g.fillRect (0.0f, y, (float) W, jmax (1.0f, s));
            g.setColour (Colours::black.withAlpha (0.4f * amount));
            g.fillRect (full());
        }
        // flickering title with RGB split
        const bool visible = rng.nextFloat() > 0.18f * amount;
        if (titleAlpha > 0.01f && visible)
        {
            const String title = st.title.toUpperCase();
            auto f = fitted (titleFont (1), title, W * 0.86f, 260 * s);
            const float off = (6 + 22 * amount) * s;
            const float jx = (rng.nextFloat() - 0.5f) * 20 * s * amount;
            g.setFont (f);
            g.setColour (st.accent.withAlpha (0.85f * titleAlpha));
            g.drawText (title, full().translated (-off + jx, 0), Justification::centred, false);
            g.setColour (st.accent2.withAlpha (0.85f * titleAlpha));
            g.drawText (title, full().translated (off + jx, 0), Justification::centred, false);
            g.setColour (st.text.withAlpha (titleAlpha));
            g.drawText (title, full().translated (jx, 0), Justification::centred, false);
        }
    }

    //==========================================================================
    /** FADE / ZOOM / SLIDE / WIPE / SPIN: the reel itself animates in over the intro + hand-over time. */
    void simpleIntro (float q)
    {
        const auto c = full().getCentre();
        switch (st.intro)
        {
            case introFade:
            case introFadeWhite:
            {
                reel (g);
                g.setColour ((st.intro == introFade ? Colours::black : Colours::white).withAlpha (1.0f - easeInOut (q)));
                g.fillRect (full());
                break;
            }
            case introZoomIn:
            case introZoomOut:
            case introSpin:
            {
                g.setColour (st.intro == introSpin ? st.bg1 : Colours::black);
                g.fillRect (full());
                const float e = st.intro == introSpin ? easeOutBack (q) : easeOutCubic (q);
                const float sc = st.intro == introZoomIn ? 0.35f + 0.65f * e
                               : st.intro == introZoomOut ? 2.2f - 1.2f * e
                               : jmax (0.02f, e);
                const float rot = st.intro == introSpin ? (1.0f - easeOutCubic (q)) * -MathConstants<float>::pi : 0.0f;
                Graphics::ScopedSaveState ss (g);
                // scale + rotate around the centre of the frame
                g.addTransform (AffineTransform::translation (-c.x, -c.y).rotated (rot).scaled (sc).translated (c.x, c.y));
                const float a = jlimit (0.0f, 1.0f, q * 2.5f);
                if (a < 0.999f) g.beginTransparencyLayer (a);
                reel (g);
                if (a < 0.999f) g.endTransparencyLayer();
                break;
            }
            case introSlideUp:
            {
                g.setColour (st.bg1);
                g.fillRect (full());
                Graphics::ScopedSaveState ss (g);
                g.addTransform (AffineTransform::translation (0, (1.0f - easeOutCubic (q)) * H));
                reel (g);
                break;
            }
            case introWipe:
            {
                g.setColour (st.bg1);
                g.fillRect (full());
                const float x = easeInOut (q) * W;
                {
                    Graphics::ScopedSaveState ss (g);
                    g.reduceClipRegion (Rectangle<float> (0, 0, x, (float) H).toNearestInt());
                    reel (g);
                }
                if (q < 0.999f)
                {
                    g.setColour (st.accent);
                    g.fillRect (x - 4 * s, 0.0f, 8 * s, (float) H);
                }
                break;
            }
            default: reel (g); break;
        }
    }

    //==========================================================================
    void scene (float p)
    {
        switch (st.intro)
        {
            case introSlam:       slamScene (p); break;
            case introTypewriter: typewriterScene (p); break;
            case introCountdown:  countdownScene (p); break;
            case introIris:       irisScene (p); break;
            default: break;
        }
    }

    void draw()
    {
        const float p = it.length > 0 ? (float) jlimit (0.0, 1.0, T / it.length) : 1.0f;
        const bool out = T >= it.length;
        const float x = ! out ? 0.0f : (it.transition > 0 ? (float) jlimit (0.0, 1.0, (T - it.length) / it.transition) : 1.0f);

        if (introIsSimple (st.intro))
        {
            simpleIntro ((float) jlimit (0.0, 1.0, T / jmax (0.05, it.end())));
            return;
        }
        if (st.intro == introDeal)
        {
            reel (g, (float) jlimit (0.0, 1.0, T / jmax (0.05, it.end())));
            return;
        }
        if (st.intro == introGlitch)
        {
            glitch (out ? 0.0f : std::pow (1.0f - p, 1.3f) * 0.85f + 0.15f, out ? 1.0f - easeInCubic (x) : 1.0f);
            return;
        }
        if (! out) { scene (p); return; }

        // hand over to the reel
        reel (g);
        const auto c = full().getCentre();
        switch (st.intro)
        {
            case introSlam:
            case introCountdown:
            {
                const float e = easeInCubic (x);
                const float sc = st.intro == introSlam ? 1.0f + 0.4f * e : 1.0f + 4.0f * e * e;
                Graphics::ScopedSaveState ss (g);
                g.addTransform (AffineTransform::scale (sc, sc, c.x, c.y));
                g.beginTransparencyLayer (1.0f - easeOutCubic (x));
                scene (1.0f);
                g.endTransparencyLayer();
                break;
            }
            case introTypewriter:
            {
                const float dy = -easeInOut (x) * H;
                Graphics::ScopedSaveState ss (g);
                g.addTransform (AffineTransform::translation (0, dy));
                scene (1.0f);
                g.setColour (st.accent);
                g.fillRect (0.0f, (float) H - 10 * s, (float) W, 10 * s);
                break;
            }
            case introIris:
            {
                const float r = easeInOut (x) * full().getCentre().getDistanceFrom ({}) * 1.05f;
                Path outside;
                outside.addRectangle (full());
                outside.addEllipse (c.x - r, c.y - r, r * 2, r * 2);
                outside.setUsingNonZeroWinding (false);
                {
                    Graphics::ScopedSaveState ss (g);
                    g.reduceClipRegion (outside);
                    scene (1.0f);
                }
                g.setColour (st.accent);
                g.drawEllipse (c.x - r, c.y - r, r * 2, r * 2, 8 * s);
                break;
            }
            default: break;
        }
    }
};
} // namespace

void renderFrame (Graphics& g, int w, int h, const RenderContext& rc)
{
    if (rc.data == nullptr || rc.style == nullptr || rc.data->numFrames == 0) return;
    const auto& d = *rc.data;
    const auto it = introTiming (*rc.style, d);
    const double T = rc.t;

    RenderContext inner = rc;
    inner.t = jlimit (0.0, d.duration, T - it.audioOffset());

    if (! it.active || T >= it.end() || T < 0.0)
    {
        Frame f (g, w, h, inner);
        f.render();
    }
    else
    {
        Intro intro (g, w, h, inner, it, T);
        intro.draw();
    }
    if (rc.watermark) drawWatermark (g, w, h);
}


void drawLookSwatch (Graphics& g, Rectangle<float> r, const Style& s)
{
    g.setGradientFill (ColourGradient (s.bg1, r.getX(), r.getY(), s.bg2, r.getRight(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 6);
    auto card = r.reduced (r.getWidth() * 0.16f, r.getHeight() * 0.22f);
    if (s.shadow > 0) { g.setColour (s.cardBorder); g.fillRoundedRectangle (card.translated (3, 3), s.radius * 0.3f); }
    g.setColour (s.cardFill.withMultipliedAlpha (s.cardAlpha));
    g.fillRoundedRectangle (card, s.radius * 0.3f);
    g.setColour (s.cardBorder);
    g.drawRoundedRectangle (card, s.radius * 0.3f, 1.2f);
    auto bars = card.reduced (card.getWidth() * 0.12f, card.getHeight() * 0.22f);
    for (int i = 0; i < 6; ++i)
    {
        float hgt = bars.getHeight() * (0.3f + 0.7f * std::abs (std::sin (i * 1.7f + 0.4f)));
        g.setColour (i % 2 ? s.accent2 : s.accent);
        g.fillRect (bars.getX() + i * bars.getWidth() / 6, bars.getBottom() - hgt, bars.getWidth() / 9, hgt);
    }
}
}
