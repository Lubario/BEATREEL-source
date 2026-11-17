#pragma once

#include "Common.h"

namespace brl
{
    enum CardId
    {
        cardCover, cardWave, cardBeat, cardMidi, cardScope, cardBass, cardLevels,
        cardVector, cardRadial, cardSpectrum, cardOrbit, cardTrack, cardDots,
        cardPiano, cardTempo, cardVinyl, cardSpectrogram, cardSteps,
        kNumCards
    };
    const char* cardName (int id);
    bool cardIsWide (int id);

    enum Aspect { aspect9x16, aspect4x5, aspect1x1, aspect16x9, kNumAspects };
    const char* aspectName (int a);
    Point<int> aspectSize (int a);            // export resolution in pixels

    enum BackgroundType { bgSolid, bgGradient, bgMesh, bgDots, bgGrid, bgStripes, bgPattern, bgImage, kNumBackgrounds };
    const char* backgroundName (int b);

    enum PatternType { patStar, patHeart, patSparkle, patNote, patSmile, kNumPatterns };
    const char* patternName (int p);

    enum MidiAlign { midiFromLoopStart, midiFromSongPosition };

    enum IntroType { introNone, introSlam, introTypewriter, introGlitch, introCountdown, introDeal, introIris,
                     introFade, introFadeWhite, introZoomIn, introZoomOut, introSlideUp, introWipe, introSpin, kNumIntros };
    /** Simple intros just animate the reel itself in (no title card). */
    inline bool introIsSimple (int i) { return i >= introFade; }
    const char* introName (int i);
    const char* introDescription (int i);
    enum IntroMode { introOverBeat, introBeforeBeat };

    struct Style
    {
        // --- look (what the 12 looks / randomize change) ---
        String lookName = "PAPER";
        Colour bg1, bg2, cardFill, cardBorder, text, dim, accent, accent2;
        int    background = bgSolid;
        int    pattern    = patStar;
        float  radius = 10.0f, border = 3.0f, shadow = 10.0f, cardAlpha = 1.0f;
        float  react = 0.6f;                // how hard cards/background react to kicks
        bool   glow = false;
        int    titleFont = 1, bodyFont = 0;  // indices into FontBank::builtInNames()
        String titleFontPath, bodyFontPath;  // custom font files (override indices when set)
        String bgImagePath;
        float  bgImageDim = 0.35f;

        // --- content ---
        String title = "UNTITLED", subtitle = "prod. you";
        uint32 cards = 0;                    // bitmask of CardId
        bool   showHeader = true, showKeys = true, showProgress = true, showFooter = true;
        String footerText = "MADE WITH BEATREEL";
        int    aspect = aspect9x16;
        int    fps = 30;
        int    midiAlign = midiFromLoopStart;
        bool   lossless = false;

        // --- intro ---
        int    intro = introNone;
        float  introLength = 2.5f;           // seconds (snapped to whole beats when introSnap)
        float  introTransition = 0.5f;       // seconds the intro takes to hand over to the reel
        int    introMode = introOverBeat;
        bool   introSnap = true;

        bool hasCard (int id) const        { return (cards >> id) & 1u; }
        void setCard (int id, bool on)     { cards = on ? (cards | (1u << id)) : (cards & ~(1u << id)); }

        var  toVar() const;
        void fromVar (const var& v);
        void copyLookFrom (const Style& other);
    };

    constexpr int kNumLooks = 18;
    Style makeLook (int index);              // full style incl. a default card set
    String lookName (int index);
    void randomise (Style& s, Random& rng);

    /** Embedded + user fonts. Thread-safe. */
    class FontBank
    {
    public:
        static FontBank& get();
        static const StringArray& builtInNames();
        Font font (int builtInIndex, const String& customPath, float height);
        Font titleFont (const Style& s, float h) { return font (s.titleFont, s.titleFontPath, h); }
        Font bodyFont  (const Style& s, float h) { return font (s.bodyFont,  s.bodyFontPath,  h); }
        bool canLoad (const File& f);

    private:
        FontBank();
        Typeface::Ptr custom (const String& path);
        std::vector<Typeface::Ptr> builtIn;
        std::map<String, Typeface::Ptr> customs;
        CriticalSection lock;
    };

    /** Presets on disk (JSON) in the user data folder. */
    namespace Presets
    {
        StringArray list();
        bool save (const String& name, const Style& s);
        bool load (const String& name, Style& s);
        bool remove (const String& name);
    }
}
