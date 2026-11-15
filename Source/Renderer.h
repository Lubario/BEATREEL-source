#pragma once

#include "Analysis.h"
#include "Style.h"

namespace brl
{
    struct RenderContext
    {
        const AnalysisData* data  = nullptr;   // required
        const MidiData*     midi  = nullptr;   // optional
        const Style*        style = nullptr;   // required
        const Image*        backgroundImage = nullptr;
        double t = 0.0;                        // seconds into the VIDEO (the intro may come before the beat)
        bool   watermark = false;              // "DEMO" overlay for unactivated exports
    };

    /** Effective intro timing for a style + capture (snapping applied). */
    struct IntroTiming
    {
        double length = 0, transition = 0;
        bool   preroll = false;            // intro plays before the beat (silent lead-in)
        bool   active = false;
        double end() const          { return active ? length + transition : 0.0; }
        double audioOffset() const  { return active && preroll ? length : 0.0; }   // video time where the beat starts
    };
    IntroTiming introTiming (const Style& s, const AnalysisData& d);

    /** Length of the exported video, including a lead-in intro. */
    double videoDuration (const Style& s, const AnalysisData& d);

    /** Draws one video frame into a w x h canvas at the graphics origin. Thread-safe (no shared mutable state). */
    void renderFrame (Graphics& g, int w, int h, const RenderContext& rc);

    /** Draws a small swatch of a look (used for the look buttons). */
    void drawLookSwatch (Graphics& g, Rectangle<float> r, const Style& s);

    /** Unit-box (0..1) path for a background pattern glyph. */
    Path patternGlyph (int pattern);
}
