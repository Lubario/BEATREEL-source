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

    int64 triggerStart (const float* src, int64 total, int64 centre, int searchLen)
    {
        for (int64 i = centre; i > jmax ((int64) 1, centre - searchLen); --i)
            if (i < total && src[i - 1] < 0.0f && src[i] >= 0.0f) return i;
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
            mono[(size_t) i] = si < total ? 0.5f * (L[si] + R[si]) : 0.0f;
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
        for (int64 i = start; i < jmin (total, start + len); ++i) mx = jmax (mx, std::abs (d.bassSignal[(size_t) i]));
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
            default: break;
        }
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

    void watermark()
    {
        Graphics::ScopedSaveState ss (g);
        g.addTransform (AffineTransform::rotation (-0.5f, W * 0.5f, H * 0.5f));
        g.setFont (FontBank::get().font (1, {}, 110 * s));
        g.setColour (Colours::white.withAlpha (0.22f));
        for (int i = -4; i <= 4; ++i)
            g.drawText ("BEATREEL DEMO  BEATREEL DEMO", Rectangle<float> (-W * 0.5f, H * 0.5f + i * 260 * s, W * 2.0f, 140 * s), Justification::centred, false);
        g.setColour (Colours::black.withAlpha (0.18f));
        for (int i = -4; i <= 4; ++i)
            g.drawText ("BEATREEL DEMO  BEATREEL DEMO", Rectangle<float> (-W * 0.5f + 3 * s, H * 0.5f + i * 260 * s + 3 * s, W * 2.0f, 140 * s), Justification::centred, false);
    }

    //==========================================================================
    static float cardWeight (int id) { return id == cardMidi ? 1.5f : id == cardBeat ? 1.35f : id == cardWave ? 0.65f : 1.0f; }

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
        if (st.showProgress) { progress (area.removeFromBottom (36 * s)); area.removeFromBottom (gap); }
        if (st.showKeys)     { keys (area.removeFromBottom (68 * s));     area.removeFromBottom (gap); }
        if (st.showHeader)   { header (area.removeFromTop (landscape ? 120 * s : 165 * s)); area.removeFromTop (gap); }

        std::vector<int> wide, small;
        for (int id : { cardBeat, cardMidi, cardWave }) if (st.hasCard (id)) wide.push_back (id);
        for (int id = 0; id < kNumCards; ++id) if (st.hasCard (id) && ! cardIsWide (id)) small.push_back (id);

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

        if (rc.watermark) watermark();
    }
};
} // namespace

void renderFrame (Graphics& g, int w, int h, const RenderContext& rc)
{
    if (rc.data == nullptr || rc.style == nullptr || rc.data->numFrames == 0) return;
    Frame f (g, w, h, rc);
    f.render();
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
