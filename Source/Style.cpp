#include "Style.h"
#include "BinaryData.h"

namespace brl
{
const char* cardName (int id)
{
    static const char* n[] = { "COVER", "WAVE", "BEAT", "MIDI", "SCOPE", "BASS", "LEVELS",
                               "VECTOR", "RADIAL", "SPECTRUM", "ORBIT", "TRACK", "DOTS",
                               "PIANO", "TEMPO", "VINYL", "SPECTROGRAM", "STEPS" };
    return isPositiveAndBelow (id, (int) kNumCards) ? n[id] : "?";
}
bool cardIsWide (int id) { return id == cardBeat || id == cardMidi || id == cardWave || id == cardSpectrogram; }

const char* aspectName (int a)
{
    static const char* n[] = { "9:16", "4:5", "1:1", "16:9" };
    return n[jlimit (0, (int) kNumAspects - 1, a)];
}
Point<int> aspectSize (int a)
{
    switch (a)
    {
        case aspect4x5:  return { 1080, 1350 };
        case aspect1x1:  return { 1080, 1080 };
        case aspect16x9: return { 1920, 1080 };
        default:         return { 1080, 1920 };
    }
}
const char* backgroundName (int b)
{
    static const char* n[] = { "Solid", "Gradient", "Mesh", "Dots", "Grid", "Stripes", "Pattern", "Image" };
    return n[jlimit (0, (int) kNumBackgrounds - 1, b)];
}
const char* patternName (int p)
{
    static const char* n[] = { "Stars", "Hearts", "Sparkles", "Notes", "Smileys" };
    return n[jlimit (0, (int) kNumPatterns - 1, p)];
}

const char* introName (int i)
{
    static const char* n[] = { "NONE", "SLAM", "TYPEWRITER", "GLITCH", "COUNTDOWN", "CARD DEAL", "IRIS",
                               "FADE IN", "FADE FROM WHITE", "ZOOM IN", "ZOOM OUT", "SLIDE UP", "WIPE", "SPIN" };
    return n[jlimit (0, (int) kNumIntros - 1, i)];
}
const char* introDescription (int i)
{
    static const char* n[] = { "Straight into the reel",
                               "Title slams in with a flash and screen shake",
                               "Title types itself out, terminal style",
                               "The reel glitches in under a flickering title",
                               "3 - 2 - 1 countdown, then the title pops",
                               "Every card flies in and lands, one by one",
                               "Title card, then an iris opens onto the reel",
                               "The reel fades up from black",
                               "The reel fades in out of a white flash",
                               "The reel grows in from the middle",
                               "The reel settles in from a close-up",
                               "The reel slides up from the bottom",
                               "The reel is wiped on from left to right",
                               "The reel spins and pops into place" };
    return n[jlimit (0, (int) kNumIntros - 1, i)];
}

//==============================================================================
namespace
{
    Colour c (uint32 rgb) { return Colour (0xff000000 | rgb); }

    const char* names[kNumLooks] = { "PAPER", "NIGHT BLOOM", "STUDIO", "TERMINAL", "BUBBLEGUM", "DEEP SEA",
                                     "SUNSET", "ICE", "GOLD RUSH", "ACID", "LAVENDER", "GOTHIC",
                                     "VAPORWAVE", "MINT", "NEON CITY", "SAKURA", "CARBON", "ARCADE" };

    uint32 defaultCards()
    {
        uint32 m = 0;
        for (int id : { cardBeat, cardMidi, cardLevels, cardScope, cardRadial, cardVector, cardBass, cardOrbit })
            m |= 1u << id;
        return m;
    }

    void colourToVar (DynamicObject& o, const char* k, Colour col) { o.setProperty (k, col.toDisplayString (true)); }
    Colour colourFromVar (const var& v, const char* k, Colour fallback)
    {
        auto s = v.getProperty (k, {}).toString();
        return s.isEmpty() ? fallback : Colour::fromString (s);
    }
}

String lookName (int i) { return names[jlimit (0, kNumLooks - 1, i)]; }

Style makeLook (int i)
{
    Style s;
    s.cards = defaultCards();
    s.lookName = lookName (i);
    auto set = [&] (uint32 b1, uint32 b2, uint32 card, uint32 border, uint32 text, uint32 dim, uint32 acc, uint32 acc2,
                    int bg, float radius, float borderW, float shadow, float alpha, bool glow, int tf, int bf)
    {
        s.bg1 = c (b1); s.bg2 = c (b2); s.cardFill = c (card); s.cardBorder = c (border);
        s.text = c (text); s.dim = c (dim); s.accent = c (acc); s.accent2 = c (acc2);
        s.background = bg; s.radius = radius; s.border = borderW; s.shadow = shadow;
        s.cardAlpha = alpha; s.glow = glow; s.titleFont = tf; s.bodyFont = bf;
    };
    switch (jlimit (0, kNumLooks - 1, i))
    {
        case 0:  set (0xEFEBDD, 0xE4DFCC, 0xF8F5EC, 0x1B1B3A, 0x1B1B3A, 0x8A8AA0, 0x2E2BD9, 0x8C88FF, bgPattern,  6, 2.0f, 0,  1.0f,  false, 3, 0); s.pattern = patStar; break;
        case 1:  set (0x1A0F1A, 0x5A1A4A, 0x1E141C, 0x4A2E44, 0xF5E8F0, 0x9C7A90, 0xFF4FC3, 0xFFB3E6, bgMesh,    14, 1.5f, 0,  0.82f, true,  1, 0); break;
        case 2:  set (0xF4F4F4, 0xE6E6E6, 0xFFFFFF, 0x111111, 0x111111, 0x8A8A8A, 0xFF2A2A, 0xFFB000, bgGrid,     0, 2.0f, 8,  1.0f,  false, 2, 6); break;
        case 3:  set (0x050806, 0x0B1A10, 0x08100B, 0x1F5F3A, 0xB8FFD0, 0x3F7F5A, 0x38FF7A, 0xE8FF5A, bgDots,     2, 1.5f, 0,  0.92f, true,  7, 6); break;
        case 4:  set (0xFFD6EC, 0xFFF0F7, 0xFFFFFF, 0xFF3D9A, 0x4A1030, 0xC07A9C, 0xFF3D9A, 0x7A5CFF, bgPattern, 22, 3.0f, 8,  1.0f,  false, 4, 0); s.pattern = patHeart; break;
        case 5:  set (0x031426, 0x0A4A6F, 0x072238, 0x0F4C75, 0xDFF6FF, 0x5C8CA8, 0x00E5FF, 0x3D7BFF, bgMesh,    12, 1.0f, 0,  0.8f,  true,  5, 0); break;
        case 6:  set (0x2B0F3A, 0xFF6B3D, 0x1F0B2A, 0xFF9A5A, 0xFFF1E6, 0xD99C8A, 0xFFB23D, 0xFF4D8D, bgGradient,16, 1.5f, 0,  0.75f, true,  2, 0); break;
        case 7:  set (0xE8F4FF, 0xB9D6F5, 0xFFFFFF, 0xA9C8E8, 0x0E2A47, 0x6F8DAD, 0x2F8CFF, 0x7FD4FF, bgMesh,    18, 1.0f, 0,  0.7f,  false, 0, 0); break;
        case 8:  set (0x0B0A08, 0x17140E, 0x14120E, 0x6B5A2E, 0xF3E7C4, 0x8E7F5C, 0xE8B64A, 0xFFF0B8, bgStripes,  4, 1.5f, 0,  0.95f, false, 8, 5); break;
        case 9:  set (0x1C1C1C, 0x111111, 0x262626, 0xD7FF3A, 0xF2F2F2, 0x8C8C8C, 0xD7FF3A, 0xFF3AD7, bgGrid,     0, 3.0f, 10, 1.0f,  false, 4, 6); break;
        case 10: set (0xE9E2FF, 0xF8F5FF, 0xFFFFFF, 0x6E56CF, 0x2A1F5C, 0x9A8FC4, 0x6E56CF, 0xFF8AD8, bgPattern, 14, 2.0f, 6,  1.0f,  false, 1, 0); s.pattern = patSparkle; break;
        case 11: set (0x080606, 0x4A0A10, 0x0E0909, 0x5A0F14, 0xF2E6E6, 0x8C5A5E, 0xE0142C, 0xFF6B7A, bgMesh,     2, 1.5f, 0,  0.9f,  true,  9, 5); break;
        case 12: set (0x2B1055, 0x7597DE, 0x1B0B3A, 0xFF71CE, 0xFDF6FF, 0xB9A3E3, 0xFF71CE, 0x01CDFE, bgGrid,     4, 2.0f, 6,  0.82f, true,  4, 0); break;
        case 13: set (0xE6F7F1, 0xCFF0E4, 0xFFFFFF, 0x1E5E4B, 0x10372C, 0x6E9C8D, 0x10B981, 0xF59E0B, bgDots,    12, 2.0f, 6,  1.0f,  false, 3, 0); break;
        case 14: set (0x05010F, 0x2A0A55, 0x0B0618, 0x3B1E6E, 0xF5F3FF, 0x8B7FB8, 0x00F5D4, 0xF15BB5, bgMesh,    10, 1.5f, 0,  0.85f, true,  1, 0); break;
        case 15: set (0xFFF1F2, 0xFFE4E6, 0xFFFFFF, 0xF9A8D4, 0x4C1D3D, 0xB4839F, 0xEC4899, 0xA78BFA, bgPattern, 20, 1.5f, 0,  0.92f, false, 8, 0); s.pattern = patSparkle; break;
        case 16: set (0x0A0A0A, 0x121212, 0x141414, 0x2A2A2A, 0xEDEDED, 0x7A7A7A, 0xFFFFFF, 0xFF3B30, bgStripes,  6, 1.0f, 0,  1.0f,  false, 2, 6); break;
        case 17: set (0x0B0B2B, 0x15154A, 0x10103A, 0xFFD400, 0xFFFFFF, 0x8C8CC8, 0xFFD400, 0xFF2E63, bgDots,     0, 3.0f, 8,  1.0f,  false, 7, 6); break;
        default: break;
    }
    s.footerText = "MADE WITH BEATREEL";
    return s;
}

void Style::copyLookFrom (const Style& o)
{
    lookName = o.lookName;
    bg1 = o.bg1; bg2 = o.bg2; cardFill = o.cardFill; cardBorder = o.cardBorder;
    text = o.text; dim = o.dim; accent = o.accent; accent2 = o.accent2;
    background = o.background == bgImage && bgImagePath.isEmpty() ? bgSolid : o.background;
    pattern = o.pattern; radius = o.radius; border = o.border; shadow = o.shadow;
    cardAlpha = o.cardAlpha; glow = o.glow; titleFont = o.titleFont; bodyFont = o.bodyFont;
    titleFontPath = {}; bodyFontPath = {};
}

void randomise (Style& s, Random& rng)
{
    auto look = makeLook (rng.nextInt (kNumLooks));
    s.copyLookFrom (look);
    s.lookName = "RANDOM";

    // shift the accent pair around the colour wheel, keep contrast with the card
    const float shift = rng.nextFloat();
    auto rotate = [&] (Colour col) { return col.withHue (std::fmod (col.getHue() + shift, 1.0f)); };
    s.accent = rotate (s.accent); s.accent2 = rotate (s.accent2);
    if (rng.nextFloat() < 0.5f) { s.bg2 = rotate (s.bg2); }

    static const int bgs[] = { bgSolid, bgGradient, bgMesh, bgDots, bgGrid, bgStripes, bgPattern };
    s.background = bgs[rng.nextInt (7)];
    s.pattern = rng.nextInt (kNumPatterns);
    s.radius = (float) (rng.nextInt (5) * 5);
    s.shadow = rng.nextFloat() < 0.4f ? 8.0f : 0.0f;
    s.react  = 0.3f + rng.nextFloat() * 0.7f;
    static const int titleFonts[] = { 1, 2, 3, 4, 5, 7, 8, 9 };
    s.titleFont = titleFonts[rng.nextInt (8)];
    s.bodyFont  = rng.nextBool() ? 0 : (rng.nextBool() ? 5 : 6);

    // cards: 1–2 wide, 3–6 small
    s.cards = 0;
    std::vector<int> wide { cardBeat, cardMidi, cardWave, cardSpectrogram }, small;
    for (int i = 0; i < kNumCards; ++i) if (! cardIsWide (i)) small.push_back (i);
    auto shuffle = [&] (std::vector<int>& v) { for (int i = (int) v.size() - 1; i > 0; --i) std::swap (v[(size_t) i], v[(size_t) rng.nextInt (i + 1)]); };
    shuffle (wide); shuffle (small);
    const int nw = 1 + rng.nextInt (2), ns = 3 + rng.nextInt (4);
    for (int i = 0; i < nw; ++i) s.setCard (wide[(size_t) i], true);
    for (int i = 0; i < ns; ++i) s.setCard (small[(size_t) i], true);
}

//==============================================================================
var Style::toVar() const
{
    auto* o = new DynamicObject();
    o->setProperty ("version", 1);
    o->setProperty ("lookName", lookName);
    colourToVar (*o, "bg1", bg1); colourToVar (*o, "bg2", bg2);
    colourToVar (*o, "cardFill", cardFill); colourToVar (*o, "cardBorder", cardBorder);
    colourToVar (*o, "text", text); colourToVar (*o, "dim", dim);
    colourToVar (*o, "accent", accent); colourToVar (*o, "accent2", accent2);
    o->setProperty ("background", background); o->setProperty ("pattern", pattern);
    o->setProperty ("radius", radius); o->setProperty ("border", border); o->setProperty ("shadow", shadow);
    o->setProperty ("cardAlpha", cardAlpha); o->setProperty ("react", react); o->setProperty ("glow", glow);
    o->setProperty ("titleFont", titleFont); o->setProperty ("bodyFont", bodyFont);
    o->setProperty ("titleFontPath", titleFontPath); o->setProperty ("bodyFontPath", bodyFontPath);
    o->setProperty ("bgImagePath", bgImagePath); o->setProperty ("bgImageDim", bgImageDim);
    o->setProperty ("title", title); o->setProperty ("subtitle", subtitle);
    o->setProperty ("cards", (int64) cards);
    o->setProperty ("showHeader", showHeader); o->setProperty ("showKeys", showKeys);
    o->setProperty ("showProgress", showProgress); o->setProperty ("showFooter", showFooter);
    o->setProperty ("footerText", footerText);
    o->setProperty ("aspect", aspect); o->setProperty ("fps", fps);
    o->setProperty ("midiAlign", midiAlign); o->setProperty ("lossless", lossless);
    o->setProperty ("intro", intro); o->setProperty ("introLength", introLength);
    o->setProperty ("introTransition", introTransition); o->setProperty ("introMode", introMode);
    o->setProperty ("introSnap", introSnap);
    return var (o);
}

void Style::fromVar (const var& v)
{
    if (! v.isObject()) return;
    auto get = [&] (const char* k, var def) { return v.getProperty (k, def); };
    lookName = get ("lookName", lookName).toString();
    bg1 = colourFromVar (v, "bg1", bg1); bg2 = colourFromVar (v, "bg2", bg2);
    cardFill = colourFromVar (v, "cardFill", cardFill); cardBorder = colourFromVar (v, "cardBorder", cardBorder);
    text = colourFromVar (v, "text", text); dim = colourFromVar (v, "dim", dim);
    accent = colourFromVar (v, "accent", accent); accent2 = colourFromVar (v, "accent2", accent2);
    background = jlimit (0, (int) kNumBackgrounds - 1, (int) get ("background", background));
    pattern = jlimit (0, (int) kNumPatterns - 1, (int) get ("pattern", pattern));
    radius = get ("radius", radius); border = get ("border", border); shadow = get ("shadow", shadow);
    cardAlpha = get ("cardAlpha", cardAlpha); react = get ("react", react); glow = get ("glow", glow);
    titleFont = get ("titleFont", titleFont); bodyFont = get ("bodyFont", bodyFont);
    titleFontPath = get ("titleFontPath", titleFontPath).toString(); bodyFontPath = get ("bodyFontPath", bodyFontPath).toString();
    bgImagePath = get ("bgImagePath", bgImagePath).toString(); bgImageDim = get ("bgImageDim", bgImageDim);
    title = get ("title", title).toString(); subtitle = get ("subtitle", subtitle).toString();
    cards = (uint32) (int64) get ("cards", (int64) cards);
    showHeader = get ("showHeader", showHeader); showKeys = get ("showKeys", showKeys);
    showProgress = get ("showProgress", showProgress); showFooter = get ("showFooter", showFooter);
    footerText = get ("footerText", footerText).toString();
    aspect = jlimit (0, (int) kNumAspects - 1, (int) get ("aspect", aspect));
    fps = get ("fps", fps) == var (60) ? 60 : 30;
    midiAlign = get ("midiAlign", midiAlign); lossless = get ("lossless", lossless);
    intro = jlimit (0, (int) kNumIntros - 1, (int) get ("intro", intro));
    introLength = jlimit (0.25f, 8.0f, (float) get ("introLength", introLength));
    introTransition = jlimit (0.0f, 2.0f, (float) get ("introTransition", introTransition));
    introMode = jlimit (0, 1, (int) get ("introMode", introMode));
    introSnap = get ("introSnap", introSnap);
}

//==============================================================================
FontBank& FontBank::get() { static FontBank fb; return fb; }

const StringArray& FontBank::builtInNames()
{
    static StringArray n { "Space Grotesk", "Bebas Neue", "Anton", "Archivo Black", "Rubik Mono One",
                           "Barlow Condensed", "JetBrains Mono", "Press Start 2P", "Playfair Display", "Unifraktur" };
    return n;
}

FontBank::FontBank()
{
    using namespace BinaryData;
    const std::pair<const char*, int> data[] = {
        { SpaceGrotesk_ttf, SpaceGrotesk_ttfSize }, { BebasNeueRegular_ttf, BebasNeueRegular_ttfSize },
        { AntonRegular_ttf, AntonRegular_ttfSize }, { ArchivoBlackRegular_ttf, ArchivoBlackRegular_ttfSize },
        { RubikMonoOneRegular_ttf, RubikMonoOneRegular_ttfSize }, { BarlowCondensedSemiBold_ttf, BarlowCondensedSemiBold_ttfSize },
        { JetBrainsMono_ttf, JetBrainsMono_ttfSize }, { PressStart2PRegular_ttf, PressStart2PRegular_ttfSize },
        { PlayfairDisplay_ttf, PlayfairDisplay_ttfSize }, { UnifrakturMaguntiaBook_ttf, UnifrakturMaguntiaBook_ttfSize } };
    for (auto& [ptr, size] : data)
        builtIn.push_back (Typeface::createSystemTypefaceFor (ptr, (size_t) size));
}

Typeface::Ptr FontBank::custom (const String& path)
{
    const ScopedLock sl (lock);
    if (auto it = customs.find (path); it != customs.end()) return it->second;
    Typeface::Ptr tf;
    MemoryBlock mb;
    if (File (path).loadFileAsData (mb) && mb.getSize() > 0)
        tf = Typeface::createSystemTypefaceFor (mb.getData(), mb.getSize());
    customs[path] = tf;
    return tf;
}

bool FontBank::canLoad (const File& f) { return custom (f.getFullPathName()) != nullptr; }

Font FontBank::font (int idx, const String& customPath, float height)
{
    Typeface::Ptr tf;
    if (customPath.isNotEmpty()) tf = custom (customPath);
    if (tf == nullptr) tf = builtIn[(size_t) jlimit (0, (int) builtIn.size() - 1, idx)];
    return Font (FontOptions (tf).withHeight (height));
}

//==============================================================================
namespace Presets
{
    static File fileFor (const String& name) { return getPresetDir().getChildFile (File::createLegalFileName (name) + ".json"); }

    StringArray list()
    {
        StringArray out;
        for (auto& f : getPresetDir().findChildFiles (File::findFiles, false, "*.json"))
            out.add (f.getFileNameWithoutExtension());
        out.sortNatural();
        return out;
    }
    bool save (const String& name, const Style& s)
    {
        auto v = s.toVar();
        if (auto* o = v.getDynamicObject()) o->setProperty ("lookName", name);
        return fileFor (name).replaceWithText (JSON::toString (v));
    }
    bool load (const String& name, Style& s)
    {
        auto v = JSON::parse (fileFor (name));
        if (! v.isObject()) return false;
        // presets carry the look + layout, but keep the current title/subtitle
        auto title = s.title, subtitle = s.subtitle;
        s.fromVar (v);
        s.title = title; s.subtitle = subtitle;
        return true;
    }
    bool remove (const String& name) { return fileFor (name).deleteFile(); }
}
}
