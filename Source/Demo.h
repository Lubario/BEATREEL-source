#pragma once

#include "Analysis.h"

namespace brl
{
    /** Synthesises an 8-bar demo beat (150 BPM, F major) plus its melody as MIDI,
        so looks can be previewed before the first capture. */
    AudioBuffer<float> makeDemoBeat (double sampleRate, CaptureInfo& info, std::shared_ptr<MidiData>& midi);
}
