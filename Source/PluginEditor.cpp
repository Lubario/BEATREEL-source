#include "PluginEditor.h"

namespace brl
{
//==============================================================================
UiLookAndFeel::UiLookAndFeel()
{
    setColour (ResizableWindow::backgroundColourId, bg());
    setColour (TextButton::buttonColourId, panel());
    setColour (TextButton::buttonOnColourId, accent());
    setColour (TextButton::textColourOffId, text());
    setColour (TextButton::textColourOnId, Colours::black);
    setColour (ComboBox::backgroundColourId, panel());
    setColour (ComboBox::outlineColourId, line());
    setColour (ComboBox::textColourId, text());
    setColour (ComboBox::arrowColourId, dim());
    setColour (PopupMenu::backgroundColourId, panel());
    setColour (PopupMenu::highlightedBackgroundColourId, accent());
    setColour (PopupMenu::highlightedTextColourId, Colours::black);
    setColour (PopupMenu::textColourId, text());
    setColour (Label::textColourId, text());
    setColour (TextEditor::backgroundColourId, panel());
    setColour (TextEditor::outlineColourId, line());
    setColour (TextEditor::focusedOutlineColourId, accent());
    setColour (TextEditor::textColourId, text());
    setColour (Slider::thumbColourId, accent());
    setColour (Slider::trackColourId, accent().withAlpha (0.6f));
    setColour (Slider::backgroundColourId, line());
    setColour (Slider::textBoxTextColourId, text());
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (ToggleButton::textColourId, text());
    setColour (ToggleButton::tickColourId, accent());
    setColour (ScrollBar::thumbColourId, line());
    setColour (AlertWindow::backgroundColourId, panel());
    setColour (AlertWindow::textColourId, text());
    setColour (AlertWindow::outlineColourId, accent());
}

Font UiLookAndFeel::ui (float h, bool heading)
{
    return FontBank::get().font (heading ? 1 : 5, {}, heading ? h * 1.15f : h);
}
Font UiLookAndFeel::getTextButtonFont (TextButton&, int h) { return ui (jmin (17.0f, h * 0.62f)); }
Font UiLookAndFeel::getLabelFont (Label& l)                { return ui (jmax (14.0f, l.getHeight() * 0.62f)); }
Font UiLookAndFeel::getComboBoxFont (ComboBox&)            { return ui (15.0f); }
Font UiLookAndFeel::getPopupMenuFont()                     { return ui (16.0f); }

void UiLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour& c, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    auto fill = b.getToggleState() ? findColour (TextButton::buttonOnColourId) : c;
    if (over) fill = fill.brighter (0.12f);
    if (down) fill = fill.darker (0.1f);
    g.setColour (fill);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (b.getToggleState() ? fill : line());
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
}

void UiLookAndFeel::drawToggleButton (Graphics& g, ToggleButton& b, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat();
    auto box = r.removeFromLeft (r.getHeight()).reduced (5);
    g.setColour (b.getToggleState() ? accent() : (over ? line().brighter (0.2f) : line()));
    g.fillRoundedRectangle (box, 3.0f);
    if (b.getToggleState())
    {
        Path tick;
        tick.startNewSubPath (box.getX() + box.getWidth() * 0.22f, box.getCentreY());
        tick.lineTo (box.getX() + box.getWidth() * 0.42f, box.getBottom() - box.getHeight() * 0.25f);
        tick.lineTo (box.getRight() - box.getWidth() * 0.2f, box.getY() + box.getHeight() * 0.25f);
        g.setColour (Colours::black);
        g.strokePath (tick, PathStrokeType (2.0f));
    }
    g.setColour (b.findColour (ToggleButton::textColourId));
    g.setFont (ui (15.0f));
    g.drawText (b.getButtonText(), r.withTrimmedLeft (4), Justification::centredLeft, true);
}

//==============================================================================
namespace
{
    void heading (Graphics& g, Rectangle<int> r, const String& t)
    {
        g.setColour (UiLookAndFeel::dim());
        g.setFont (UiLookAndFeel::ui (14.0f));
        g.drawText (t, r, Justification::bottomLeft, false);
    }

    struct Page : public Component
    {
        explicit Page (BeatReelEditor& e) : ed (e) {}
        virtual void refresh() = 0;
        virtual int contentHeight() const = 0;
        Style& st() { return ed.proc.style; }
        BeatReelEditor& ed;
    };

    //--------------------------------------------------------------------------
    struct LooksPage : public Page
    {
        struct Tile : public Component
        {
            Tile (LooksPage& p, int i) : page (p), index (i), look (makeLook (i)) {}
            void paint (Graphics& g) override
            {
                auto r = getLocalBounds().toFloat().reduced (3);
                auto label = r.removeFromBottom (20);
                drawLookSwatch (g, r, look);
                bool sel = page.st().lookName == look.lookName;
                if (sel || isMouseOver())
                {
                    g.setColour (sel ? UiLookAndFeel::accent() : UiLookAndFeel::dim());
                    g.drawRoundedRectangle (r.expanded (1.5f), 7, 2.0f);
                }
                g.setColour (sel ? UiLookAndFeel::accent() : UiLookAndFeel::text());
                g.setFont (UiLookAndFeel::ui (14));
                g.drawText (String (index + 1).paddedLeft ('0', 2) + "  " + look.lookName, label, Justification::centredLeft, true);
            }
            void mouseEnter (const MouseEvent&) override { repaint(); }
            void mouseExit (const MouseEvent&) override  { repaint(); }
            void mouseUp (const MouseEvent& e) override
            {
                if (! e.mouseWasClicked()) return;
                page.st().copyLookFrom (look);
                page.ed.styleChanged();
            }
            LooksPage& page; int index; Style look;
        };

        explicit LooksPage (BeatReelEditor& e) : Page (e)
        {
            for (int i = 0; i < kNumLooks; ++i) addAndMakeVisible (tiles.add (new Tile (*this, i)));
        }
        void refresh() override { for (auto* t : tiles) t->repaint(); }
        int contentHeight() const override { return 30 + (kNumLooks / 2) * 104; }
        void paint (Graphics& g) override { heading (g, { 4, 0, getWidth(), 24 }, String (kNumLooks) + " LOOKS - CLICK TO APPLY, THEN MAKE IT YOURS"); }
        void resized() override
        {
            const int w = (getWidth() - 8) / 2;
            for (int i = 0; i < tiles.size(); ++i)
                tiles[i]->setBounds (4 + (i % 2) * w, 30 + (i / 2) * 104, w, 100);
        }
        OwnedArray<Tile> tiles;
    };

    //--------------------------------------------------------------------------
    struct CardsPage : public Page
    {
        explicit CardsPage (BeatReelEditor& e) : Page (e)
        {
            for (int i = 0; i < kNumCards; ++i)
            {
                auto* b = cardToggles.add (new ToggleButton (cardName (i)));
                addAndMakeVisible (b);
                b->onClick = [this, i, b] { st().setCard (i, b->getToggleState()); ed.styleChanged(); };
            }
            const char* parts[] = { "HEADER", "KEY STRIP", "PROGRESS", "FOOTER" };
            for (int i = 0; i < 4; ++i)
            {
                auto* b = partToggles.add (new ToggleButton (parts[i]));
                addAndMakeVisible (b);
                b->onClick = [this, i, b]
                {
                    bool v = b->getToggleState();
                    if (i == 0) st().showHeader = v; else if (i == 1) st().showKeys = v;
                    else if (i == 2) st().showProgress = v; else st().showFooter = v;
                    ed.styleChanged();
                };
            }
            midiAlign.addItemList ({ "MIDI starts at the loop start", "MIDI follows song position" }, 1);
            midiAlign.onChange = [this] { st().midiAlign = midiAlign.getSelectedItemIndex(); ed.styleChanged(); };
            fps.addItemList ({ "30 fps", "60 fps" }, 1);
            fps.onChange = [this] { st().fps = fps.getSelectedItemIndex() == 1 ? 60 : 30; ed.styleChanged(); };
            audio.addItemList ({ "Audio: AAC 320k (for posting)", "Audio: ALAC lossless (archive)" }, 1);
            audio.onChange = [this] { st().lossless = audio.getSelectedItemIndex() == 1; ed.styleChanged(); };
            bars.addItemList ({ "REC: until the loop ends", "REC: 4 bars", "REC: 8 bars", "REC: 16 bars", "REC: 32 bars" }, 1);
            bars.onChange = [this]
            {
                static const int v[] = { 0, 4, 8, 16, 32 };
                ed.proc.maxBars = v[jmax (0, bars.getSelectedItemIndex())];
                ed.proc.settings().setValue ("maxBars", ed.proc.maxBars);
            };
            for (auto* c : { &midiAlign, &fps, &audio, &bars }) addAndMakeVisible (c);
            addAndMakeVisible (resetOrder);
            resetOrder.onClick = [this] { st().cardOrder = {}; ed.styleChanged(); };
        }
        void refresh() override
        {
            for (int i = 0; i < kNumCards; ++i) cardToggles[i]->setToggleState (st().hasCard (i), dontSendNotification);
            bool v[] = { st().showHeader, st().showKeys, st().showProgress, st().showFooter };
            for (int i = 0; i < 4; ++i) partToggles[i]->setToggleState (v[i], dontSendNotification);
            midiAlign.setSelectedItemIndex (st().midiAlign, dontSendNotification);
            fps.setSelectedItemIndex (st().fps == 60 ? 1 : 0, dontSendNotification);
            audio.setSelectedItemIndex (st().lossless ? 1 : 0, dontSendNotification);
            const int m = ed.proc.maxBars;
            bars.setSelectedItemIndex (m == 4 ? 1 : m == 8 ? 2 : m == 16 ? 3 : m == 32 ? 4 : 0, dontSendNotification);
        }
        int contentHeight() const override { return 30 + ((kNumCards + 1) / 2) * 28 + 40 + 2 * 28 + 40 + 4 * 34 + 80; }
        void paint (Graphics& g) override
        {
            heading (g, { 4, 0, getWidth(), 24 }, "CARDS ON THE VIDEO");
            heading (g, { 4, partsY - 30, getWidth(), 24 }, "LAYOUT");
            heading (g, { 4, optsY - 30, getWidth(), 24 }, "OPTIONS");
            g.setColour (UiLookAndFeel::accent());
            g.setFont (UiLookAndFeel::ui (14));
            g.drawText ("TIP: drag cards in the preview to swap their spots", Rectangle<int> (4, hintY, getWidth() - 8, 24), Justification::centredLeft, true);
        }
        void resized() override
        {
            const int w = (getWidth() - 8) / 2;
            int y = 30;
            for (int i = 0; i < kNumCards; ++i) cardToggles[i]->setBounds (4 + (i % 2) * w, y + (i / 2) * 28, w, 26);
            partsY = y + ((kNumCards + 1) / 2) * 28 + 40;
            for (int i = 0; i < 4; ++i) partToggles[i]->setBounds (4 + (i % 2) * w, partsY + (i / 2) * 28, w, 26);
            optsY = partsY + 2 * 28 + 40;
            int oy = optsY;
            for (auto* c : { &bars, &midiAlign, &fps, &audio }) { c->setBounds (4, oy, getWidth() - 8, 28); oy += 34; }
            hintY = oy + 6;
            resetOrder.setBounds (4, oy + 34, getWidth() - 8, 28);
        }
        OwnedArray<ToggleButton> cardToggles, partToggles;
        ComboBox midiAlign, fps, audio, bars;
        TextButton resetOrder { "RESET CARD ORDER" };
        int partsY = 0, optsY = 0, hintY = 0;
    };

    //--------------------------------------------------------------------------
    struct ColourSwatch : public Component, private ChangeListener
    {
        ColourSwatch (Page& p, String n, Colour Style::* m) : page (p), name (std::move (n)), member (m) {}
        void paint (Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (2);
            auto sw = r.removeFromTop (r.getHeight() - 16);
            g.setColour (page.st().*member);
            g.fillRoundedRectangle (sw, 4);
            g.setColour (UiLookAndFeel::line().brighter (0.3f));
            g.drawRoundedRectangle (sw, 4, 1);
            g.setColour (UiLookAndFeel::dim());
            g.setFont (UiLookAndFeel::ui (12));
            g.drawText (name, r, Justification::centred, true);
        }
        void mouseUp (const MouseEvent& e) override
        {
            if (! e.mouseWasClicked()) return;
            auto sel = std::make_unique<ColourSelector> (ColourSelector::showColourAtTop | ColourSelector::showSliders | ColourSelector::showColourspace);
            sel->setCurrentColour (page.st().*member);
            sel->setSize (300, 320);
            sel->addChangeListener (this);
            sel->setColour (ColourSelector::backgroundColourId, UiLookAndFeel::panel());
            CallOutBox::launchAsynchronously (std::move (sel), getScreenBounds(), nullptr);
        }
        void changeListenerCallback (ChangeBroadcaster* b) override
        {
            if (auto* sel = dynamic_cast<ColourSelector*> (b))
            {
                page.st().*member = sel->getCurrentColour();
                page.st().lookName = "CUSTOM";
                page.ed.styleChanged();
            }
        }
        Page& page; String name; Colour Style::* member;
    };

    //--------------------------------------------------------------------------
    /** 16 steps of one bar: shows when the beat zoom fires, click to edit (switches to a custom pattern). */
    struct ZoomGrid : public Component, private Timer
    {
        explicit ZoomGrid (Page& p) : page (p) { startTimerHz (30); }

        static uint32 impliedSteps (const Style& st, const AnalysisData* d)
        {
            switch (st.zoomMode)
            {
                case zoomEveryBeat:     return 0x1111u;
                case zoomEvery2Beats:   return 0x0101u;
                case zoomEveryBar:      return 0x0001u;
                case zoomEveryHalfBeat: return 0x5555u;
                case zoomPattern:       return st.zoomSteps & 0xffffu;
                case zoomAuto:          return d != nullptr ? patternFromKicks (*d) : 0x1111u;
                default:                return 0;
            }
        }
        void timerCallback() override { repaint(); }
        Rectangle<float> cell (int i) const
        {
            const float gap = 3.0f, groupGap = 8.0f;
            const float w = (getWidth() - gap * 12 - groupGap * 3) / 16.0f;
            const float x = i * (w + gap) + (i / 4) * (groupGap - gap);
            return { x, 0.0f, w, (float) getHeight() };
        }
        void paint (Graphics& g) override
        {
            auto& st = page.st();
            auto d = page.ed.proc.getData();
            const uint32 steps = impliedSteps (st, d.get());
            int cur = -1;
            if (d != nullptr)
            {
                const double songBeat = d->info.startPpq + page.ed.lastPreviewT / d->secondsPerBeat();
                cur = (int) (((int64) std::floor (songBeat / (d->info.beatsPerBar / 16.0)) % 16 + 16) % 16);
            }
            const bool editable = st.zoomMode == zoomPattern;
            const bool off = st.zoomMode == zoomOff;
            for (int i = 0; i < 16; ++i)
            {
                auto r = cell (i);
                const bool on = ! off && ((steps >> i) & 1u);
                g.setColour (on ? UiLookAndFeel::accent().withAlpha (editable ? 1.0f : 0.55f)
                                : UiLookAndFeel::line().withAlpha (i % 4 == 0 ? 1.0f : 0.6f));
                g.fillRoundedRectangle (r, 3);
                if (i == cur)
                {
                    g.setColour (Colours::white);
                    g.drawRoundedRectangle (r.reduced (0.5f), 3, 2);
                }
            }
        }
        void mouseDown (const MouseEvent& e) override
        {
            auto& st = page.st();
            for (int i = 0; i < 16; ++i)
                if (cell (i).contains (e.position))
                {
                    if (st.zoomMode != zoomPattern)
                    {
                        auto d = page.ed.proc.getData();
                        st.zoomSteps = impliedSteps (st, d.get());   // start from what you were seeing
                        st.zoomMode = zoomPattern;
                    }
                    st.zoomSteps ^= 1u << i;
                    page.ed.styleChanged();
                    page.refresh();
                    return;
                }
        }
        Page& page;
    };

    struct StylePage : public Page
    {
        explicit StylePage (BeatReelEditor& e) : Page (e)
        {
            auto addText = [this] (TextEditor& te, String Style::* m)
            {
                addAndMakeVisible (te);
                te.setFont (UiLookAndFeel::ui (16));
                te.onTextChange = [this, &te, m] { st().*m = te.getText(); ed.styleChanged(); };
            };
            addText (title, &Style::title);
            addText (subtitle, &Style::subtitle);
            addText (footer, &Style::footerText);
            addText (cardText, &Style::cardText);
            cardText.setMultiLine (true, false);
            cardText.setReturnKeyStartsNewLine (true);
            title.setInputRestrictions (40);

            const std::pair<const char*, Colour Style::*> cols[] = {
                { "BG", &Style::bg1 }, { "BG 2", &Style::bg2 }, { "CARD", &Style::cardFill }, { "BORDER", &Style::cardBorder },
                { "TEXT", &Style::text }, { "DIM", &Style::dim }, { "ACCENT", &Style::accent }, { "ACCENT 2", &Style::accent2 } };
            for (auto& [n, m] : cols) addAndMakeVisible (swatches.add (new ColourSwatch (*this, n, m)));

            for (int i = 0; i < kNumBackgrounds; ++i) background.addItem (backgroundName (i), i + 1);
            background.onChange = [this]
            {
                st().background = background.getSelectedItemIndex();
                if (st().background == bgImage && st().bgImagePath.isEmpty()) pickImage();
                ed.styleChanged();
            };
            for (int i = 0; i < kNumPatterns; ++i) pattern.addItem (patternName (i), i + 1);
            pattern.onChange = [this] { st().pattern = pattern.getSelectedItemIndex(); ed.styleChanged(); };
            imageBtn.onClick = [this] { pickImage(); };

            auto fillFonts = [] (ComboBox& c)
            {
                c.clear (dontSendNotification);
                auto names = FontBank::builtInNames();
                for (int i = 0; i < names.size(); ++i) c.addItem (names[i], i + 1);
                c.addSeparator();
                c.addItem ("Load a font file (.ttf / .otf)...", 1000);
            };
            fillFonts (titleFont); fillFonts (bodyFont);
            auto fontChange = [this] (ComboBox& c, int Style::* idx, String Style::* path)
            {
                c.onChange = [this, &c, idx, path]
                {
                    const int id = c.getSelectedId();
                    if (id == 1000)
                    {
                        chooser = std::make_unique<FileChooser> ("Choose a font", File(), "*.ttf;*.otf");
                        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [this, path] (const FileChooser& fc)
                        {
                            auto f = fc.getResult();
                            if (f.existsAsFile() && FontBank::get().canLoad (f)) { st().*path = f.getFullPathName(); ed.styleChanged(); }
                            refresh();
                        });
                        return;
                    }
                    if (id > 0) { st().*idx = id - 1; st().*path = {}; ed.styleChanged(); }
                };
            };
            fontChange (titleFont, &Style::titleFont, &Style::titleFontPath);
            fontChange (bodyFont, &Style::bodyFont, &Style::bodyFontPath);

            auto addSlider = [this] (Slider& s, double lo, double hi, float Style::* m)
            {
                addAndMakeVisible (s);
                s.setRange (lo, hi, 0.01);
                s.setSliderStyle (Slider::LinearHorizontal);
                s.setTextBoxStyle (Slider::NoTextBox, true, 0, 0);
                s.onValueChange = [this, &s, m] { st().*m = (float) s.getValue(); ed.styleChanged(); };
            };
            addSlider (radius, 0, 30, &Style::radius);
            addSlider (border, 0, 6, &Style::border);
            addSlider (shadow, 0, 20, &Style::shadow);
            addSlider (alpha, 0.2, 1, &Style::cardAlpha);
            addSlider (react, 0, 1, &Style::react);
            addSlider (zoomLen, 0, 1, &Style::zoomLength);
            addAndMakeVisible (zoomNudge);
            zoomNudge.setRange (-250, 250, 1);
            zoomNudge.setTextBoxStyle (Slider::TextBoxRight, false, 64, 22);
            zoomNudge.setTextValueSuffix (" ms");
            zoomNudge.setDoubleClickReturnValue (true, 0.0);
            zoomNudge.onValueChange = [this] { st().zoomNudgeMs = (float) zoomNudge.getValue(); ed.styleChanged(); };
            for (int i = 0; i < kNumZoomModes; ++i) zoomMode.addItem (zoomModeName (i), i + 1);
            zoomMode.onChange = [this] { st().zoomMode = zoomMode.getSelectedItemIndex(); ed.styleChanged(); refresh(); };
            addAndMakeVisible (zoomMode);
            addAndMakeVisible (zoomGrid);
            addAndMakeVisible (fromKicks);
            fromKicks.onClick = [this]
            {
                if (auto d = ed.proc.getData()) { st().zoomSteps = patternFromKicks (*d); st().zoomMode = zoomPattern; ed.styleChanged(); refresh(); }
            };
            addSlider (imageDim, 0, 0.9, &Style::bgImageDim);
            glow.onClick = [this] { st().glow = glow.getToggleState(); ed.styleChanged(); };
            for (Component* c : { (Component*) &background, (Component*) &pattern, (Component*) &imageBtn, (Component*) &titleFont,
                                  (Component*) &bodyFont, (Component*) &glow })
                addAndMakeVisible (c);
        }

        void pickImage()
        {
            chooser = std::make_unique<FileChooser> ("Background image", File(), "*.png;*.jpg;*.jpeg;*.gif");
            chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [this] (const FileChooser& fc)
            {
                auto f = fc.getResult();
                if (f.existsAsFile()) { st().bgImagePath = f.getFullPathName(); st().background = bgImage; }
                else if (st().bgImagePath.isEmpty()) st().background = bgSolid;
                ed.styleChanged();
            });
        }

        void refresh() override
        {
            if (! title.hasKeyboardFocus (true)) title.setText (st().title, false);
            if (! subtitle.hasKeyboardFocus (true)) subtitle.setText (st().subtitle, false);
            if (! footer.hasKeyboardFocus (true)) footer.setText (st().footerText, false);
            if (! cardText.hasKeyboardFocus (true)) cardText.setText (st().cardText, false);
            for (auto* s : swatches) s->repaint();
            background.setSelectedItemIndex (st().background, dontSendNotification);
            pattern.setSelectedItemIndex (st().pattern, dontSendNotification);
            pattern.setEnabled (st().background == bgPattern);
            imageBtn.setButtonText (st().bgImagePath.isEmpty() ? "CHOOSE IMAGE..." : File (st().bgImagePath).getFileName());
            auto fontSel = [] (ComboBox& c, int idx, const String& path)
            {
                if (path.isNotEmpty())
                {
                    if (c.indexOfItemId (2000) < 0) c.addItem ("Custom: " + File (path).getFileNameWithoutExtension(), 2000);
                    else c.changeItemText (2000, "Custom: " + File (path).getFileNameWithoutExtension());
                    c.setSelectedId (2000, dontSendNotification);
                }
                else c.setSelectedId (idx + 1, dontSendNotification);
            };
            fontSel (titleFont, st().titleFont, st().titleFontPath);
            fontSel (bodyFont, st().bodyFont, st().bodyFontPath);
            radius.setValue (st().radius, dontSendNotification);
            border.setValue (st().border, dontSendNotification);
            shadow.setValue (st().shadow, dontSendNotification);
            alpha.setValue (st().cardAlpha, dontSendNotification);
            react.setValue (st().react, dontSendNotification);
            zoomMode.setSelectedItemIndex (st().zoomMode, dontSendNotification);
            zoomNudge.setValue (st().zoomNudgeMs, dontSendNotification);
            zoomLen.setValue (st().zoomLength, dontSendNotification);
            zoomGrid.repaint();
            imageDim.setValue (st().bgImageDim, dontSendNotification);
            glow.setToggleState (st().glow, dontSendNotification);
        }

        int contentHeight() const override { return totalH; }

        void paint (Graphics& g) override
        {
            for (auto& [y, t] : labels) heading (g, { 4, y - 20, getWidth(), 20 }, t);
        }

        void resized() override
        {
            labels.clear();
            int y = 24;
            const int w = getWidth() - 8;
            auto row = [&] (Component& c, const String& label, int h = 28)
            {
                labels.push_back ({ y, label });
                c.setBounds (4, y, w, h);
                y += h + 24;
            };
            row (title, "TITLE");
            row (subtitle, "SUBTITLE (shown next to BPM and key)");
            labels.push_back ({ y, "COLOURS - CLICK TO EDIT" });
            const int sw = w / 4;
            for (int i = 0; i < swatches.size(); ++i) swatches[i]->setBounds (4 + (i % 4) * sw, y + (i / 4) * 52, sw, 50);
            y += 2 * 52 + 24;
            row (background, "BACKGROUND");
            row (pattern, "PATTERN");
            row (imageBtn, "BACKGROUND IMAGE");
            row (imageDim, "IMAGE DARKEN", 22);
            row (titleFont, "TITLE FONT");
            row (bodyFont, "TEXT FONT");
            row (radius, "CORNER RADIUS", 22);
            row (border, "BORDER", 22);
            row (shadow, "HARD SHADOW", 22);
            row (alpha, "CARD OPACITY", 22);
            row (react, "BEAT ZOOM STRENGTH", 22);
            row (zoomMode, "ZOOM ON");
            row (zoomGrid, "ONE BAR - CLICK STEPS FOR A CUSTOM PATTERN", 30);
            fromKicks.setBounds (4, y - 18, w, 26); y += 32;
            row (zoomNudge, "TIMING NUDGE (- earlier / + later, double-click resets)", 24);
            row (zoomLen, "ZOOM LENGTH (short - long)", 22);
            glow.setBounds (4, y - 16, w, 26); y += 20;
            row (footer, "FOOTER TEXT");
            row (cardText, "TEXT CARD (one line each, e.g. @yourname)", 56);
            totalH = y;
        }

        TextEditor title, subtitle, footer, cardText;
        OwnedArray<ColourSwatch> swatches;
        ComboBox background, pattern, titleFont, bodyFont;
        TextButton imageBtn { "CHOOSE IMAGE..." };
        Slider radius, border, shadow, alpha, react, imageDim, zoomLen, zoomNudge;
        ComboBox zoomMode;
        ZoomGrid zoomGrid { *this };
        TextButton fromKicks { "START FROM THE DETECTED KICKS" };
        ToggleButton glow { "GLOW" };
        std::unique_ptr<FileChooser> chooser;
        std::vector<std::pair<int, String>> labels;
        int totalH = 900;
    };

    //--------------------------------------------------------------------------
    struct InfoPage : public Page
    {
        explicit InfoPage (BeatReelEditor& e) : Page (e)
        {
            addAndMakeVisible (bpm);
            bpm.setRange (40, 300, 0.5);
            bpm.setTextBoxStyle (Slider::TextBoxRight, false, 70, 26);
            bpm.onDragEnd = [this] { ed.proc.setBpm (bpm.getValue()); };
            bpm.onValueChange = [this] { if (! bpm.isMouseButtonDown()) ed.proc.setBpm (bpm.getValue()); };
            previewAudio.onClick = [this]
            {
                ed.proc.previewAudio = previewAudio.getToggleState();
                ed.proc.settings().setValue ("previewAudio", previewAudio.getToggleState());
            };
            clearMidi.onClick = [this] { ed.proc.clearMidi(); };
            ffmpegBtn.onClick = [this]
            {
                chooser = std::make_unique<FileChooser> ("Locate ffmpeg", File(), "*");
                chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [this] (const FileChooser& fc)
                {
                    if (auto f = fc.getResult(); f.existsAsFile()) ed.proc.settings().setValue ("ffmpegPath", f.getFullPathName());
                    refresh();
                });
            };
            licenseBtn.onClick = [this]
            {
                if (ed.proc.license.valid) { License::deactivate(); ed.proc.license = {}; refresh(); }
            };
            openFolder.onClick = [] { getUserDataDir().startAsProcess(); };
            for (Component* c : { (Component*) &previewAudio, (Component*) &clearMidi, (Component*) &ffmpegBtn,
                                  (Component*) &licenseBtn, (Component*) &openFolder })
                addAndMakeVisible (c);
        }
        void refresh() override
        {
            auto d = ed.proc.getData();
            if (d != nullptr && ! bpm.isMouseButtonDown()) bpm.setValue (d->info.bpm, dontSendNotification);
            previewAudio.setToggleState (ed.proc.previewAudio.load(), dontSendNotification);
            auto ff = ed.proc.findFfmpeg();
            ffmpegText = ff.existsAsFile() ? "ffmpeg: " + ff.getFullPathName() : "ffmpeg: NOT FOUND - needed for export";
            licenseBtn.setButtonText (ed.proc.license.valid ? "DEACTIVATE THIS COMPUTER" : "NOT ACTIVATED - use ACTIVATE (top bar)");
            licenseBtn.setEnabled (ed.proc.license.valid);
            auto m = ed.proc.getMidi();
            clearMidi.setEnabled (m != nullptr);
            midiText = m != nullptr ? "MIDI: " + m->fileName + "  (" + String ((int) m->notes.size()) + " notes, "
                                         + String (m->lengthQuarters / jmax (1, d ? d->info.beatsPerBar : 4), 0) + " bars)" : "MIDI: none loaded";
            if (d != nullptr)
                capText = String (ed.proc.isDemo() ? "DEMO BEAT" : "CAPTURE") + "  " + formatTime (d->duration) + String::fromUTF8 ("  ·  ")
                        + String (d->bars(), 1) + String::fromUTF8 (" bars  ·  ") + String (d->info.sampleRate / 1000.0, 1) + " kHz 32-bit float\n"
                        + keyName (d->keyRoot, d->keyMinor) + String::fromUTF8 ("  ·  ") + String (d->integratedLufs, 1) + String::fromUTF8 (" LUFS integrated  ·  starts at bar ")
                        + String ((int) std::floor (d->info.startPpq / d->info.beatsPerBar) + 1);
            repaint();
        }
        int contentHeight() const override { return 640; }
        void paint (Graphics& g) override
        {
            heading (g, { 4, 0, getWidth(), 22 }, "CAPTURE");
            g.setColour (UiLookAndFeel::text());
            g.setFont (UiLookAndFeel::ui (15));
            g.drawFittedText (capText, 4, 26, getWidth() - 8, 44, Justification::topLeft, 3);
            heading (g, { 4, 76, getWidth(), 22 }, "TEMPO (FROM YOUR DAW, OR SET IT FOR IMPORTED AUDIO)");
            g.setColour (UiLookAndFeel::text());
            g.drawFittedText (midiText, 4, 170, getWidth() - 8, 22, Justification::centredLeft, 1);
            g.drawFittedText (ffmpegText, 4, 270, getWidth() - 8, 36, Justification::topLeft, 2);
            heading (g, { 4, 380, getWidth(), 22 }, "HOW TO");
            g.setColour (UiLookAndFeel::dim());
            g.drawFittedText ("1  Put BEATREEL last on your master track.\n"
                              "2  Select the loop / region you want in your DAW.\n"
                              "3  Press REC, then press play. It stops by itself at the end of the loop "
                              "(or press REC again). Exporting your song with REC armed also works.\n"
                              "4  Load the .mid of your melody, pick a look, tweak it, EXPORT.",
                              4, 406, getWidth() - 8, 150, Justification::topLeft, 10);
        }
        void resized() override
        {
            bpm.setBounds (4, 102, getWidth() - 8, 28);
            previewAudio.setBounds (4, 136, getWidth() - 8, 26);
            clearMidi.setBounds (4, 196, getWidth() - 8, 28);
            ffmpegBtn.setBounds (4, 232, getWidth() - 8, 28);
            licenseBtn.setBounds (4, 312, getWidth() - 8, 28);
            openFolder.setBounds (4, 346, getWidth() - 8, 28);
        }
        Slider bpm;
        ToggleButton previewAudio { "Hear the preview when the DAW is stopped" };
        TextButton clearMidi { "REMOVE MIDI" }, ffmpegBtn { "LOCATE FFMPEG..." }, licenseBtn, openFolder { "OPEN PRESETS / CAPTURES FOLDER" };
        String capText, midiText, ffmpegText;
        std::unique_ptr<FileChooser> chooser;
    };
    //--------------------------------------------------------------------------
    struct IntroPage : public Page, private Timer
    {
        /** Timeline: drag the two handles to set the intro length and its hand-over to the reel. */
        struct Timeline : public Component
        {
            explicit Timeline (IntroPage& p) : page (p) {}
            static constexpr double span = 10.0;   // seconds shown

            float xOf (double sec) const { return 10.0f + (float) (sec / span) * (getWidth() - 20.0f); }
            double secAt (float x) const { return jlimit (0.0, span, (double) (x - 10.0f) / (getWidth() - 20.0f) * span); }

            void paint (Graphics& g) override
            {
                auto& st = page.st();
                auto d = page.ed.proc.getData();
                auto r = getLocalBounds().toFloat();
                auto track = r.withTrimmedTop (18).withTrimmedBottom (16).reduced (10, 0);
                g.setColour (UiLookAndFeel::line());
                g.fillRoundedRectangle (track, 4);

                // beat ticks + second labels
                if (d != nullptr)
                {
                    const double spb = d->secondsPerBeat();
                    for (double b = 0; b <= span; b += spb)
                    {
                        const bool bar = std::abs (std::fmod (b / spb, (double) d->info.beatsPerBar)) < 1e-6;
                        g.setColour (UiLookAndFeel::dim().withAlpha (bar ? 0.6f : 0.25f));
                        g.fillRect (xOf (b), track.getY(), 1.0f, track.getHeight());
                    }
                }
                g.setFont (UiLookAndFeel::ui (12));
                g.setColour (UiLookAndFeel::dim());
                for (int sec = 0; sec <= (int) span; sec += 2)
                    g.drawText (String (sec) + "s", Rectangle<float> (xOf (sec) - 15, r.getBottom() - 15, 30, 14), Justification::centred, false);

                if (d == nullptr || st.intro == introNone)
                {
                    g.setColour (UiLookAndFeel::dim());
                    g.drawText ("PICK AN INTRO ABOVE", track, Justification::centred, false);
                    return;
                }
                const auto it = introTiming (st, *d);
                auto introR = Rectangle<float> (xOf (0), track.getY(), xOf (it.length) - xOf (0), track.getHeight());
                auto fadeR  = Rectangle<float> (xOf (it.length), track.getY(), xOf (it.end()) - xOf (it.length), track.getHeight());
                g.setColour (UiLookAndFeel::accent());
                g.fillRoundedRectangle (introR, 4);
                g.setColour (UiLookAndFeel::accent2().withAlpha (0.55f));
                g.fillRect (fadeR);
                g.setColour (Colours::black);
                g.setFont (UiLookAndFeel::ui (13));
                if (introR.getWidth() > 40) g.drawText ("INTRO", introR, Justification::centred, false);

                // where the beat starts
                const float bx = xOf (it.audioOffset());
                g.setColour (UiLookAndFeel::text());
                g.drawText (it.preroll ? "BEAT STARTS" : "BEAT FROM 0:00", Rectangle<float> (bx - 2, 0, 120, 16), Justification::centredLeft, false);
                g.fillRect (bx, 14.0f, 2.0f, track.getBottom() - 14.0f);

                // handles
                for (double hs : { it.length, it.end() })
                {
                    const float hx = xOf (hs);
                    g.setColour (UiLookAndFeel::text());
                    g.fillRoundedRectangle (hx - 4, track.getY() - 3, 8, track.getHeight() + 6, 3);
                }
                // playhead while previewing
                const double T = page.ed.getIntroPreviewTime();
                if (T >= 0)
                {
                    g.setColour (Colours::white);
                    g.fillRect (xOf (T) - 1, track.getY() - 6, 2.0f, track.getHeight() + 12);
                }
            }

            void mouseDown (const MouseEvent& e) override
            {
                auto d = page.ed.proc.getData();
                if (d == nullptr || page.st().intro == introNone) { dragging = 0; return; }
                const auto it = introTiming (page.st(), *d);
                const float a = std::abs (e.position.x - xOf (it.length)), b = std::abs (e.position.x - xOf (it.end()));
                dragging = b < a && b < 14 ? 2 : (a < 14 || b >= 14 ? 1 : 2);
                mouseDrag (e);
            }
            void mouseDrag (const MouseEvent& e) override
            {
                auto d = page.ed.proc.getData();
                if (dragging == 0 || d == nullptr) return;
                auto& st = page.st();
                const double sec = secAt (e.position.x);
                if (dragging == 1)
                {
                    const double fade = st.introTransition;
                    st.introLength = (float) jlimit (0.25, 8.0, sec);
                    st.introTransition = (float) fade;
                }
                else
                {
                    const auto it = introTiming (st, *d);
                    st.introTransition = (float) jlimit (0.0, 2.0, sec - it.length);
                }
                page.ed.styleChanged();
                repaint();
            }
            void mouseUp (const MouseEvent&) override { dragging = 0; page.ed.startIntroPreview(); }

            IntroPage& page;
            int dragging = 0;
        };

        struct Tile : public Component
        {
            Tile (IntroPage& p, int i) : page (p), index (i) {}
            void paint (Graphics& g) override
            {
                auto r = getLocalBounds().toFloat().reduced (2);
                const bool sel = page.st().intro == index;
                g.setColour (sel ? UiLookAndFeel::accent() : (isMouseOver() ? UiLookAndFeel::line().brighter (0.15f) : UiLookAndFeel::panel()));
                g.fillRoundedRectangle (r, 5);
                g.setColour (sel ? UiLookAndFeel::accent() : UiLookAndFeel::line());
                g.drawRoundedRectangle (r, 5, 1);
                auto tr = r.reduced (10, 6);
                g.setColour (sel ? Colours::black : UiLookAndFeel::text());
                g.setFont (UiLookAndFeel::ui (18, true));
                g.drawText (introName (index), tr.removeFromTop (22), Justification::centredLeft, false);
                g.setColour (sel ? Colours::black.withAlpha (0.75f) : UiLookAndFeel::dim());
                g.setFont (UiLookAndFeel::ui (13));
                g.drawFittedText (introDescription (index), tr.toNearestInt(), Justification::topLeft, 2);
            }
            void mouseEnter (const MouseEvent&) override { repaint(); }
            void mouseExit (const MouseEvent&) override  { repaint(); }
            void mouseUp (const MouseEvent& e) override
            {
                if (! e.mouseWasClicked()) return;
                page.st().intro = index;
                page.ed.styleChanged();
                if (index != introNone) page.ed.startIntroPreview(); else page.ed.stopIntroPreview();
                page.refresh();
            }
            IntroPage& page; int index;
        };

        explicit IntroPage (BeatReelEditor& e) : Page (e), timeline (*this)
        {
            for (int i = 0; i < kNumIntros; ++i) addAndMakeVisible (tiles.add (new Tile (*this, i)));
            addAndMakeVisible (previewBtn);
            previewBtn.setColour (TextButton::buttonColourId, UiLookAndFeel::accent());
            previewBtn.setColour (TextButton::textColourOffId, Colours::black);
            previewBtn.onClick = [this] { if (ed.isIntroPreviewing()) ed.stopIntroPreview(); else ed.startIntroPreview(); };
            addAndMakeVisible (loop);
            loop.onClick = [this] { ed.introLoop = loop.getToggleState(); if (ed.introLoop) ed.startIntroPreview(); };
            addAndMakeVisible (timeline);

            auto slider = [this] (Slider& sl, double lo, double hi, float Style::* m)
            {
                addAndMakeVisible (sl);
                sl.setRange (lo, hi, 0.05);
                sl.setTextBoxStyle (Slider::TextBoxRight, false, 64, 24);
                sl.setTextValueSuffix (" s");
                sl.onValueChange = [this, &sl, m] { st().*m = (float) sl.getValue(); ed.styleChanged(); timeline.repaint(); };
                sl.onDragEnd = [this] { ed.startIntroPreview(); };
            };
            slider (length, 0.25, 8.0, &Style::introLength);
            slider (fade, 0.0, 2.0, &Style::introTransition);
            addAndMakeVisible (snap);
            snap.onClick = [this] { st().introSnap = snap.getToggleState(); ed.styleChanged(); refresh(); };
            mode.addItemList ({ "Plays over the start of the beat", "Plays before the beat (silent lead-in)" }, 1);
            mode.onChange = [this] { st().introMode = mode.getSelectedItemIndex(); ed.styleChanged(); refresh(); ed.startIntroPreview(); };
            addAndMakeVisible (mode);
            startTimerHz (30);
        }

        void timerCallback() override
        {
            timeline.repaint();
            previewBtn.setButtonText (ed.isIntroPreviewing() ? "STOP PREVIEW" : String::fromUTF8 ("\xe2\x96\xb6  PREVIEW INTRO"));
        }

        void refresh() override
        {
            for (auto* t : tiles) t->repaint();
            length.setValue (st().introLength, dontSendNotification);
            fade.setValue (st().introTransition, dontSendNotification);
            snap.setToggleState (st().introSnap, dontSendNotification);
            loop.setToggleState (ed.introLoop, dontSendNotification);
            mode.setSelectedItemIndex (st().introMode, dontSendNotification);
            const bool on = st().intro != introNone;
            for (Component* c : { (Component*) &length, (Component*) &fade, (Component*) &snap, (Component*) &mode, (Component*) &previewBtn, (Component*) &loop })
                c->setEnabled (on);
            info = {};
            if (auto d = ed.proc.getData(); d != nullptr && on)
            {
                const auto it = introTiming (st(), *d);
                info << "Intro " << String (it.length, 2) << " s (" << String (it.length / d->secondsPerBeat(), 1) << " beats)"
                     << "  +  " << String (it.transition, 2) << " s hand-over\n"
                     << "Video length " << formatTime (videoDuration (st(), *d)) << "."
                     << (it.preroll ? String (" Your beat starts after the intro.") : String (" Your beat plays from the first frame."));
            }
            timeline.repaint();
            repaint();
        }

        int contentHeight() const override { return 30 + ((kNumIntros + 1) / 2) * 66 + 410; }

        void paint (Graphics& g) override
        {
            heading (g, { 4, 0, getWidth(), 24 }, "INTRO - SHOWS AT THE START OF THE EXPORTED VIDEO");
            for (auto& [y, t] : labels) heading (g, { 4, y - 20, getWidth(), 20 }, t);
            g.setColour (UiLookAndFeel::text());
            g.setFont (UiLookAndFeel::ui (14));
            g.drawFittedText (info, 4, infoY, getWidth() - 8, 44, Justification::topLeft, 3);
        }

        void resized() override
        {
            labels.clear();
            const int w = (getWidth() - 8) / 2;
            int y = 30;
            for (int i = 0; i < tiles.size(); ++i) tiles[i]->setBounds (4 + (i % 2) * w, y + (i / 2) * 66, w, 62);
            y += ((kNumIntros + 1) / 2) * 66 + 8;
            previewBtn.setBounds (4, y, getWidth() - 8, 34); y += 40;
            loop.setBounds (4, y, getWidth() - 8, 26); y += 52;
            labels.push_back ({ y, "TIMING - DRAG THE HANDLES" });
            timeline.setBounds (0, y, getWidth(), 70); y += 96;
            labels.push_back ({ y, "INTRO LENGTH" });
            length.setBounds (4, y, getWidth() - 8, 26); y += 30;
            snap.setBounds (4, y, getWidth() - 8, 26); y += 50;
            labels.push_back ({ y, "HAND-OVER TO THE REEL" });
            fade.setBounds (4, y, getWidth() - 8, 26); y += 50;
            labels.push_back ({ y, "WHEN IT PLAYS" });
            mode.setBounds (4, y, getWidth() - 8, 28); y += 38;
            infoY = y;
        }

        OwnedArray<Tile> tiles;
        Timeline timeline;
        TextButton previewBtn;
        ToggleButton loop { "Loop the preview while I tweak the timing" }, snap { "Snap to whole beats" };
        Slider length, fade;
        ComboBox mode;
        String info;
        std::vector<std::pair<int, String>> labels;
        int infoY = 0;
    };
}

//==============================================================================
class BeatReelEditor::RecButton : public Button
{
public:
    explicit RecButton (BeatReelProcessor& p) : Button ("REC"), proc (p) {}
    void paintButton (Graphics& g, bool over, bool) override
    {
        auto r = getLocalBounds().toFloat().reduced (1);
        const int st = proc.getRecState();
        const bool rec = st == BeatReelProcessor::recRecording, armed = st == BeatReelProcessor::recArmed;
        const bool blink = (Time::getMillisecondCounter() / 400) % 2;
        g.setColour (rec ? UiLookAndFeel::accent() : (over ? UiLookAndFeel::line().brighter (0.15f) : Colour (0xff050407)));
        g.fillRoundedRectangle (r, 5);
        g.setColour (rec ? Colours::black : UiLookAndFeel::accent());
        auto dot = r.removeFromLeft (r.getHeight()).reduced (r.getHeight() * 0.3f);
        if (rec) g.fillRect (dot.reduced (1));
        else if (! armed || blink) g.fillEllipse (dot);
        g.setColour (rec ? Colours::black : UiLookAndFeel::text());
        g.setFont (UiLookAndFeel::ui (r.getHeight() * 0.5f, true));
        g.drawText (rec ? formatTime (proc.getRecordedSeconds()) : armed ? "ARMED" : "REC", r.withTrimmedRight (r.getHeight() * 0.4f), Justification::centred, false);
    }
    BeatReelProcessor& proc;
};

//==============================================================================
class BeatReelEditor::Preview : public Component
{
public:
    explicit Preview (BeatReelEditor& e) : ed (e) {}

    void update (double t)
    {
        auto d = ed.proc.getData();
        auto m = ed.proc.getMidi();
        const auto size = aspectSize (ed.proc.style.aspect);
        auto area = getLocalBounds().reduced (16).withTrimmedBottom (ed.proc.isDemo() ? 40 : 0).toFloat();
        const float scale = jmin (area.getWidth() / size.x, area.getHeight() / size.y);
        const int w = jmax (8, (int) (size.x * scale)), h = jmax (8, (int) (size.y * scale));
        const float ds = (float) getDesktopScaleFactor() * (float) Desktop::getInstance().getDisplays().getPrimaryDisplay()->scale;
        const int pw = (int) (w * jmin (2.0f, ds)), phh = (int) (h * jmin (2.0f, ds));
        if (frame.getWidth() != pw || frame.getHeight() != phh) frame = Image (Image::RGB, pw, phh, true);
        frameBounds = Rectangle<int> (w, h).withCentre (area.getCentre().toInt());
        if (d != nullptr && d->numFrames > 0)
        {
            Graphics g (frame);
            RenderContext rc;
            rc.data = d.get(); rc.midi = m.get(); rc.style = &ed.proc.style;
            rc.backgroundImage = ed.proc.getBackgroundImage();
            rc.t = t;
            renderFrame (g, pw, phh, rc);
            hasFrame = true;
        }
        repaint();
    }

    void paint (Graphics& g) override
    {
        auto& P = ed.proc;
        g.fillAll (UiLookAndFeel::bg().darker (0.3f));
        if (hasFrame)
        {
            g.setColour (Colours::black.withAlpha (0.5f));
            g.fillRect (frameBounds.translated (6, 8));
            g.setOpacity (1.0f);
            g.drawImage (frame, frameBounds.toFloat());
        }

        const int st = P.getRecState();
        auto panel = [&] (int h)
        {
            auto r = getLocalBounds().withSizeKeepingCentre (jmin (getWidth() - 40, 460), h);
            g.setColour (UiLookAndFeel::panel().withAlpha (0.96f));
            g.fillRoundedRectangle (r.toFloat(), 8);
            g.setColour (UiLookAndFeel::accent());
            g.drawRoundedRectangle (r.toFloat(), 8, 2);
            return r.reduced (20, 16);
        };
        const String host = P.getHostShortName();
        if (st == BeatReelProcessor::recRecording || st == BeatReelProcessor::recArmed)
        {
            auto r = panel (190);
            g.setColour (UiLookAndFeel::accent());
            g.setFont (UiLookAndFeel::ui (54, true));
            g.drawText (st == BeatReelProcessor::recArmed ? "ARMED" : formatTime (P.getRecordedSeconds()), r.removeFromTop (64), Justification::centred, false);
            g.setColour (UiLookAndFeel::text());
            g.setFont (UiLookAndFeel::ui (17));
            String line;
            if (st == BeatReelProcessor::recArmed) line = "NOW PRESS PLAY IN " + host;
            else
            {
                line = String::fromUTF8 ("RECORDING  ·  ") + String (P.getRecordedBars(), 1) + " BARS";
                if (auto lp = P.getLoopProgress(); lp >= 0) line << String::fromUTF8 ("  ·  ") << (int) (lp * 100) << String::fromUTF8 (" %  ·  STOPS AT THE END OF THE LOOP");
                else line << String::fromUTF8 ("  ·  PRESS REC OR STOP TO FINISH");
            }
            g.drawFittedText (line, r.removeFromTop (44), Justification::centred, 2);
            g.setColour (UiLookAndFeel::dim());
            g.setFont (UiLookAndFeel::ui (15));
            g.drawText (host + "  " + (P.hostPlaying.load() ? "PLAYING" : "STOPPED") + String::fromUTF8 ("  ·  ") + String (P.hostBpm.load(), 1) + " BPM",
                        r.removeFromTop (24), Justification::centred, false);
            g.drawText ("BIT-EXACT 32-BIT FLOAT CAPTURE OF YOUR MASTER", r.removeFromTop (24), Justification::centred, false);
        }
        else if (P.isAnalysing())
        {
            auto r = panel (90);
            g.setColour (UiLookAndFeel::text());
            g.setFont (UiLookAndFeel::ui (22, true));
            g.drawText ("ANALYSING YOUR BEAT...", r, Justification::centred, false);
        }
        else if (P.isDemo() && hasFrame)
        {
            auto r = getLocalBounds().removeFromBottom (46).withSizeKeepingCentre (jmin (getWidth() - 30, 520), 34);
            g.setColour (Colours::black.withAlpha (0.8f));
            g.fillRoundedRectangle (r.toFloat(), 6);
            g.setColour (UiLookAndFeel::text());
            g.setFont (UiLookAndFeel::ui (15));
            g.drawText (String::fromUTF8 ("DEMO BEAT  ·  press REC, then play in ") + host + String::fromUTF8 ("  ·  or IMPORT AUDIO"), r, Justification::centred, false);
        }

        if (P.exporter.isBusy())
        {
            auto r = panel (150);
            g.setColour (UiLookAndFeel::text());
            g.setFont (UiLookAndFeel::ui (26, true));
            g.drawText ("EXPORTING", r.removeFromTop (40), Justification::centred, false);
            auto bar = r.removeFromTop (16).toFloat();
            g.setColour (UiLookAndFeel::line());
            g.fillRoundedRectangle (bar, 4);
            g.setColour (UiLookAndFeel::accent());
            g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * P.exporter.getProgress()), 4);
            g.setColour (UiLookAndFeel::dim());
            g.setFont (UiLookAndFeel::ui (15));
            g.drawText (P.exporter.getStatus() + "   (click to cancel)", r.removeFromTop (34), Justification::centred, false);
        }
    }
    //==========================================================================
    // Drag a card onto another card to swap their places.
    Point<int> exportSize() const { return aspectSize (ed.proc.style.aspect); }
    float toScreenScale() const   { return frameBounds.getWidth() / (float) exportSize().x; }
    Point<float> toExport (Point<float> p) const
    {
        return (p - frameBounds.getPosition().toFloat()) / toScreenScale();
    }
    Rectangle<float> toScreen (Rectangle<float> r) const
    {
        const float k = toScreenScale();
        return Rectangle<float> (r.getX() * k, r.getY() * k, r.getWidth() * k, r.getHeight() * k) + frameBounds.getPosition().toFloat();
    }
    FrameLayout layout() const    { return computeLayout (ed.proc.style, exportSize().x, exportSize().y); }
    Rectangle<float> rectOf (const FrameLayout& L, int id) const
    {
        for (auto& [cid, r] : L.cards) if (cid == id) return toScreen (r);
        return {};
    }
    bool canDrag() const { return hasFrame && ! ed.proc.exporter.isBusy() && ed.proc.getRecState() == BeatReelProcessor::recIdle; }

    void mouseMove (const MouseEvent& e) override
    {
        const int id = canDrag() ? layout().cardAt (toExport (e.position)) : -1;
        if (id != hoverId) { hoverId = id; repaint(); }
        setMouseCursor (id >= 0 ? MouseCursor::DraggingHandCursor : MouseCursor::NormalCursor);
    }
    void mouseExit (const MouseEvent&) override { hoverId = -1; repaint(); }
    void mouseDown (const MouseEvent& e) override
    {
        if (ed.proc.exporter.isBusy()) { ed.proc.exporter.cancel(); return; }
        dragId = canDrag() ? layout().cardAt (toExport (e.position)) : -1;
        targetId = -1;
        dragPos = e.position;
    }
    void mouseDrag (const MouseEvent& e) override
    {
        if (dragId < 0) return;
        dragPos = e.position;
        const int over = layout().cardAt (toExport (e.position));
        targetId = over != dragId ? over : -1;
        repaint();
    }
    void mouseUp (const MouseEvent&) override
    {
        if (dragId >= 0 && targetId >= 0)
        {
            ed.proc.style.swapCards (dragId, targetId);
            ed.styleChanged();
        }
        dragId = targetId = -1;
        repaint();
    }

    void paintOverChildren (Graphics& g) override
    {
        if (! hasFrame) return;
        const auto L = layout();
        if (dragId < 0 && hoverId >= 0)
        {
            auto r = rectOf (L, hoverId);
            g.setColour (Colours::white.withAlpha (0.85f));
            Path outline; outline.addRoundedRectangle (r.reduced (1), 4.0f);
            const float dashes[] = { 6.0f, 4.0f };
            PathStrokeType (2.0f).createDashedStroke (outline, outline, dashes, 2);
            g.fillPath (outline);
            auto tag = Rectangle<float> (0, 0, 110, 22).withCentre (r.getCentre());
            g.setColour (Colours::black.withAlpha (0.75f));
            g.fillRoundedRectangle (tag, 4);
            g.setColour (Colours::white);
            g.setFont (UiLookAndFeel::ui (13));
            g.drawText ("DRAG TO MOVE", tag, Justification::centred, false);
        }
        if (dragId >= 0)
        {
            auto src = rectOf (L, dragId);
            g.setColour (Colours::black.withAlpha (0.45f));
            g.fillRoundedRectangle (src, 4);
            if (targetId >= 0)
            {
                auto dst = rectOf (L, targetId);
                g.setColour (UiLookAndFeel::accent().withAlpha (0.25f));
                g.fillRoundedRectangle (dst, 4);
                g.setColour (UiLookAndFeel::accent());
                g.drawRoundedRectangle (dst.reduced (1), 4, 3);
                g.setColour (Colours::white);
                g.setFont (UiLookAndFeel::ui (14, true));
                g.drawText ("SWAP", dst, Justification::centred, false);
            }
            // ghost of the dragged card under the mouse
            auto ghost = src.withSizeKeepingCentre (jmin (src.getWidth(), 150.0f), jmin (src.getHeight(), 70.0f)).withCentre (dragPos);
            g.setColour (UiLookAndFeel::panel().withAlpha (0.92f));
            g.fillRoundedRectangle (ghost, 6);
            g.setColour (UiLookAndFeel::accent());
            g.drawRoundedRectangle (ghost, 6, 2);
            g.setColour (UiLookAndFeel::text());
            g.setFont (UiLookAndFeel::ui (16, true));
            g.drawText (cardName (dragId), ghost, Justification::centred, false);
        }
    }

    int hoverId = -1, dragId = -1, targetId = -1;
    Point<float> dragPos;

    BeatReelEditor& ed;
    Image frame;
    Rectangle<int> frameBounds;
    bool hasFrame = false;
};

//==============================================================================
BeatReelEditor::BeatReelEditor (BeatReelProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);
    recButton = std::make_unique<RecButton> (p);
    preview = std::make_unique<Preview> (*this);
    addAndMakeVisible (*recButton);
    addAndMakeVisible (*preview);
    recButton->onClick = [this] { proc.toggleRec(); };

    for (auto* b : { &randomBtn, &presetsBtn, &importBtn, &midiBtn, &exportBtn, &activateBtn, &playBtn })
        addAndMakeVisible (b);
    exportBtn.setColour (TextButton::buttonColourId, UiLookAndFeel::accent());
    exportBtn.setColour (TextButton::textColourOffId, Colours::black);

    randomBtn.onClick = [this] { auto keep = proc.style; randomise (proc.style, rng); proc.style.title = keep.title;
                                 proc.style.subtitle = keep.subtitle; proc.style.aspect = keep.aspect; styleChanged(); };
    presetsBtn.onClick = [this] { showPresetsMenu(); };
    importBtn.onClick = [this]
    {
        chooseFile ("Import your exported beat", "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg", [this] (const File& f) { proc.importAudio (f); styleChanged(); });
    };
    midiBtn.onClick = [this] { chooseFile ("Load the .mid of your melody", "*.mid;*.midi", [this] (const File& f) { proc.loadMidi (f); }); };
    exportBtn.onClick = [this] { startExport(); };
    activateBtn.onClick = [this] { showActivation(); };
    playBtn.onClick = [this] { proc.previewPlaying = ! proc.previewPlaying.load(); };

    const char* tabNames[] = { "LOOKS", "CARDS", "STYLE", "INTRO", "INFO" };
    for (int i = 0; i < 5; ++i)
    {
        tabs[i].setButtonText (tabNames[i]);
        tabs[i].setClickingTogglesState (false);
        tabs[i].onClick = [this, i] { showPage (i); };
        addAndMakeVisible (tabs[i]);
    }
    pages[0] = std::make_unique<LooksPage> (*this);
    pages[1] = std::make_unique<CardsPage> (*this);
    pages[2] = std::make_unique<StylePage> (*this);
    pages[3] = std::make_unique<IntroPage> (*this);
    pages[4] = std::make_unique<InfoPage> (*this);
    pageView.setScrollBarsShown (true, false);
    pageView.setScrollBarThickness (8);
    addAndMakeVisible (pageView);

    for (int i = 0; i < kNumAspects; ++i)
    {
        aspectBtns[i].setButtonText (aspectName (i));
        aspectBtns[i].onClick = [this, i] { proc.style.aspect = i; styleChanged(); };
        addAndMakeVisible (aspectBtns[i]);
    }
    scrub.setSliderStyle (Slider::LinearHorizontal);
    scrub.setTextBoxStyle (Slider::NoTextBox, true, 0, 0);
    scrub.setRange (0.0, 1.0);
    scrub.onValueChange = [this]
    {
        if (auto d = proc.getData(); d != nullptr && scrub.isMouseButtonDown())
        {
            introPreview = false;
            proc.previewHold = 0;
            proc.previewPos = (int64) (scrub.getValue() * d->duration * d->info.sampleRate);
        }
    };
    addAndMakeVisible (scrub);
    timeLabel.setJustificationType (Justification::centredRight);
    addAndMakeVisible (timeLabel);

    showPage (0);
    setResizable (true, true);
    setResizeLimits (1000, 660, 2400, 1600);
    setSize (1240, 800);
    startTimerHz (30);
}

BeatReelEditor::~BeatReelEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void BeatReelEditor::showPage (int index)
{
    currentPage = index;
    for (int i = 0; i < 5; ++i) tabs[i].setToggleState (i == index, dontSendNotification);
    auto* p = static_cast<Page*> (pages[index].get());
    pageView.setViewedComponent (p, false);
    p->setSize (pageView.getWidth() - 10, p->contentHeight());
    p->resized();
    p->setSize (pageView.getWidth() - 10, jmax (p->contentHeight(), 10));
    p->refresh();
}

void BeatReelEditor::styleChanged()
{
    ++proc.styleVersion;
}

void BeatReelEditor::startIntroPreview()
{
    auto d = proc.getData();
    if (d == nullptr || proc.style.intro == introNone) { introPreview = false; return; }
    const auto it = introTiming (proc.style, *d);
    introPreview = true;
    introT = 0;
    introStartMs = Time::getMillisecondCounterHiRes();
    proc.previewPlaying = true;
    proc.previewPos = 0;
    proc.previewHold = (int64) (it.audioOffset() * (proc.getSampleRate() > 0 ? proc.getSampleRate() : 44100.0));
}

void BeatReelEditor::showMessage (const String& title, const String& msg)
{
    AlertWindow::showMessageBoxAsync (MessageBoxIconType::NoIcon, title, msg, "OK", this);
}

void BeatReelEditor::chooseFile (const String& title, const String& patterns, std::function<void (const File&)> cb, bool save, File initial)
{
    chooser = std::make_unique<FileChooser> (title, initial, patterns);
    auto flags = save ? (FileBrowserComponent::saveMode | FileBrowserComponent::warnAboutOverwriting)
                      : (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles);
    chooser->launchAsync (flags | FileBrowserComponent::canSelectFiles, [cb] (const FileChooser& fc)
    {
        auto f = fc.getResult();
        if (f != File()) cb (f);
    });
}

void BeatReelEditor::showPresetsMenu()
{
    PopupMenu m, del;
    m.addItem (1, "Save current look as preset...");
    m.addSeparator();
    auto names = Presets::list();
    for (int i = 0; i < names.size(); ++i) { m.addItem (100 + i, names[i]); del.addItem (1000 + i, names[i]); }
    if (names.isEmpty()) m.addItem (2, "(no presets yet)", false);
    else m.addSubMenu ("Delete", del);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (presetsBtn), [this, names] (int r)
    {
        if (r == 1)
        {
            auto* w = new AlertWindow ("SAVE PRESET", "Name your look", MessageBoxIconType::NoIcon, this);
            w->addTextEditor ("name", proc.style.lookName == "RANDOM" || proc.style.lookName == "CUSTOM" ? "MY LOOK" : proc.style.lookName);
            w->addButton ("SAVE", 1, KeyPress (KeyPress::returnKey));
            w->addButton ("CANCEL", 0, KeyPress (KeyPress::escapeKey));
            w->enterModalState (true, ModalCallbackFunction::create ([this, w] (int res)
            {
                auto name = w->getTextEditorContents ("name").trim().toUpperCase();
                if (res == 1 && name.isNotEmpty())
                {
                    Presets::save (name, proc.style);
                    proc.style.lookName = name;
                    styleChanged();
                }
            }), true);
        }
        else if (r >= 1000) Presets::remove (names[r - 1000]);
        else if (r >= 100) { Presets::load (names[r - 100], proc.style); styleChanged(); }
    });
}

void BeatReelEditor::startExport()
{
    if (proc.exporter.isBusy()) return;
    auto d = proc.getData();
    if (d == nullptr) return;
    auto ff = proc.findFfmpeg();
    if (! ff.existsAsFile())
    {
        showMessage ("FFMPEG NEEDED", "BEATREEL uses ffmpeg to encode the MP4.\nThe installer puts it in place for you; if it's missing, "
                                      "go to INFO > LOCATE FFMPEG, or copy ffmpeg into:\n" + getUserDataDir().getFullPathName());
        return;
    }
    const bool demoMark = ! proc.license.valid;
    auto dir = File (proc.settings().getValue ("exportDir", File::getSpecialLocation (File::userMoviesDirectory).getFullPathName()));
    auto name = File::createLegalFileName (proc.style.title.isEmpty() ? "beatreel" : proc.style.title)
              + "_" + String (aspectName (proc.style.aspect)).replace (":", "x") + (proc.style.lossless ? ".mov" : ".mp4");
    chooseFile ("Export video", proc.style.lossless ? "*.mov" : "*.mp4", [this, d, ff, demoMark] (const File& f)
    {
        proc.settings().setValue ("exportDir", f.getParentDirectory().getFullPathName());
        ExportJob job;
        job.data = d; job.midi = proc.getMidi(); job.style = proc.style;
        job.output = f.withFileExtension (proc.style.lossless ? "mov" : "mp4");
        job.ffmpeg = ff;
        job.watermark = demoMark;
        proc.exporter.start (std::move (job));
        exportWasBusy = true;
    }, true, dir.getChildFile (name));
}

void BeatReelEditor::showActivation()
{
    if (proc.license.valid)
    {
        showMessage ("ACTIVATED", "Licensed to " + proc.license.email + "\nOrder " + proc.license.orderId);
        return;
    }
    auto* w = new AlertWindow ("ACTIVATE BEATREEL", "Paste the license key from your purchase email.\n"
                               "Activation is offline: no internet needed, now or later.", MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("key", {}, "License key");
    w->addButton ("ACTIVATE", 1, KeyPress (KeyPress::returnKey));
    w->addButton ("CANCEL", 0, KeyPress (KeyPress::escapeKey));
    w->enterModalState (true, ModalCallbackFunction::create ([this, w] (int res)
    {
        if (res != 1) return;
        auto err = License::activate (w->getTextEditorContents ("key"));
        if (err.isNotEmpty()) { showMessage ("NOT ACTIVATED", err); return; }
        proc.license = License::current();
        showMessage ("THANK YOU", "BEATREEL is activated for " + proc.license.email + ".\nExports no longer carry the demo watermark.");
    }), true);
}

void BeatReelEditor::timerCallback()
{
    const double t = proc.getPreviewSeconds();
    lastPreviewT = t;
    double videoT = t;
    if (auto d0 = proc.getData())
    {
        const auto it = introTiming (proc.style, *d0);
        if (introPreview)
        {
            introT = (Time::getMillisecondCounterHiRes() - introStartMs) / 1000.0;
            if (! it.active) introPreview = false;
            else if (introT > it.end() + 1.0)
            {
                if (introLoop) startIntroPreview();
                else introPreview = false;
            }
        }
        videoT = introPreview ? introT : t + it.audioOffset();
    }
    preview->update (videoT);
    recButton->repaint();

    if (auto d = proc.getData())
    {
        if (! scrub.isMouseButtonDown()) scrub.setValue (t / jmax (1e-6, d->duration), dontSendNotification);
        timeLabel.setText (introPreview ? "INTRO " + String (introT, 1) + " s" : formatTime (t) + " / " + formatTime (d->duration), dontSendNotification);
    }
    playBtn.setButtonText (proc.previewPlaying.load() ? "PAUSE" : "PLAY");
    for (int i = 0; i < kNumAspects; ++i) aspectBtns[i].setToggleState (proc.style.aspect == i, dontSendNotification);
    activateBtn.setButtonText (proc.license.valid ? "ACTIVATED" : "ACTIVATE");

    if (seenStyleVersion != proc.styleVersion.load())
    {
        seenStyleVersion = proc.styleVersion.load();
        static_cast<Page*> (pages[currentPage].get())->refresh();
        repaint();
    }
    if (exportWasBusy && ! proc.exporter.isBusy())
    {
        exportWasBusy = false;
        auto res = proc.exporter.getResult();
        if (res.isEmpty())
        {
            auto out = proc.exporter.getOutput();
            AlertWindow::showOkCancelBox (MessageBoxIconType::NoIcon, "READY TO POST", out.getFileName() + " is ready.",
                                          "SHOW FILE", "OK", this, ModalCallbackFunction::create ([out] (int r) { if (r == 1) out.revealToUser(); }));
        }
        else if (res != "Cancelled") showMessage ("EXPORT FAILED", res);
    }
    if (auto err = proc.getLastError(); err.isNotEmpty() && err != lastShownError) { lastShownError = err; showMessage ("BEATREEL", err); }
}

void BeatReelEditor::paint (Graphics& g)
{
    g.fillAll (UiLookAndFeel::bg());
    auto top = getLocalBounds().removeFromTop (64);
    g.setColour (UiLookAndFeel::panel());
    g.fillRect (top);
    g.setColour (UiLookAndFeel::line());
    g.fillRect (top.removeFromBottom (1));

    // logo: three little bars + wordmark
    auto logo = Rectangle<int> (18, 14, 200, 36);
    for (int i = 0; i < 3; ++i)
    {
        const int h = 10 + i * 8;
        g.setColour (i == 2 ? UiLookAndFeel::accent() : UiLookAndFeel::text());
        g.fillRect (logo.getX() + i * 8, logo.getBottom() - h, 5, h);
    }
    g.setColour (UiLookAndFeel::text());
    g.setFont (UiLookAndFeel::ui (34, true));
    g.drawText ("BEATREEL", logo.withTrimmedLeft (30), Justification::centredLeft, false);

    // look name under the left tabs
    g.setColour (UiLookAndFeel::dim());
    g.setFont (UiLookAndFeel::ui (14));
    g.drawText ("LOOK: " + proc.style.lookName, Rectangle<int> (16, getHeight() - 30, 300, 22), Justification::centredLeft, false);
   #ifndef BEATREEL_BUILD
    #define BEATREEL_BUILD "dev"
   #endif
   #ifndef JucePlugin_VersionString
    #define JucePlugin_VersionString "1.0.0"
   #endif
    g.drawText ("v" JucePlugin_VersionString "  build " BEATREEL_BUILD, Rectangle<int> (16, getHeight() - 30, 300, 22), Justification::centredRight, false);
}

void BeatReelEditor::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop (64).reduced (12, 12);
    top.removeFromLeft (210);
    recButton->setBounds (top.removeFromLeft (150));
    top.removeFromLeft (16);
    activateBtn.setBounds (top.removeFromRight (110));
    top.removeFromRight (8);
    exportBtn.setBounds (top.removeFromRight (120));
    top.removeFromRight (8);
    for (auto* b : { &midiBtn, &importBtn, &presetsBtn, &randomBtn })
    {
        b->setBounds (top.removeFromRight (jmin (130, top.getWidth() / 4)));
        top.removeFromRight (8);
    }

    auto left = r.removeFromLeft (330).reduced (12, 10);
    left.removeFromBottom (24);
    auto tabRow = left.removeFromTop (32);
    const int tw = tabRow.getWidth() / 5;
    for (auto& t : tabs) t.setBounds (tabRow.removeFromLeft (tw).reduced (2, 0));
    left.removeFromTop (10);
    pageView.setBounds (left);
    if (auto* p = static_cast<Page*> (pages[currentPage].get()))
    {
        p->setSize (pageView.getWidth() - 10, p->contentHeight());
        p->setSize (pageView.getWidth() - 10, jmax (p->contentHeight(), 10));
    }

    auto bottom = r.removeFromBottom (52).reduced (12, 10);
    playBtn.setBounds (bottom.removeFromLeft (80));
    bottom.removeFromLeft (10);
    for (int i = kNumAspects - 1; i >= 0; --i) { aspectBtns[i].setBounds (bottom.removeFromRight (62)); bottom.removeFromRight (4); }
    bottom.removeFromRight (10);
    timeLabel.setBounds (bottom.removeFromRight (100));
    scrub.setBounds (bottom.reduced (0, 8));
    preview->setBounds (r);
}
}
