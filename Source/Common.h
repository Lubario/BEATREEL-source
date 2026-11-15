#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_cryptography/juce_cryptography.h>

#include <algorithm>
#include <array>
#include <map>
#include <thread>
#include <atomic>
#include <memory>
#include <vector>

namespace brl
{
    using namespace juce;

    inline constexpr const char* kProductName = "BEATREEL";

    /** Per-user data folder: presets, captures, license, ffmpeg. */
    inline File getUserDataDir()
    {
        auto dir = File::getSpecialLocation (File::userApplicationDataDirectory)
                   #if JUCE_MAC
                       .getChildFile ("Application Support")
                   #endif
                       .getChildFile (kProductName);
        dir.createDirectory();
        return dir;
    }

    inline File getPresetDir()   { auto d = getUserDataDir().getChildFile ("Presets");  d.createDirectory(); return d; }
    inline File getCaptureDir()  { auto d = getUserDataDir().getChildFile ("Captures"); d.createDirectory(); return d; }

    inline String formatTime (double seconds)
    {
        auto s = jmax (0, (int) std::floor (seconds));
        return String (s / 60) + ":" + String (s % 60).paddedLeft ('0', 2);
    }

    inline const char* const* noteNamesSharp()
    {
        static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        return names;
    }

    inline String midiNoteName (int note)
    {
        return String (noteNamesSharp()[((note % 12) + 12) % 12]) + String (note / 12 - 1);
    }
}
