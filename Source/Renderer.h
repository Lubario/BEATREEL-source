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
        double t = 0.0;                        // seconds into the capture
        bool   watermark = false;              // "DEMO" overlay for unactivated exports
    };

    /** Draws one video frame into a w x h canvas at the graphics origin. Thread-safe (no shared mutable state). */
    void renderFrame (Graphics& g, int w, int h, const RenderContext& rc);

    /** Draws a small swatch of a look (used for the look buttons). */
    void drawLookSwatch (Graphics& g, Rectangle<float> r, const Style& s);

    /** Unit-box (0..1) path for a background pattern glyph. */
    Path patternGlyph (int pattern);
}
