#pragma once

#include "PluginProcessor.h"
#include <juce_gui_extra/juce_gui_extra.h>

namespace brl
{
    class UiLookAndFeel : public LookAndFeel_V4
    {
    public:
        UiLookAndFeel();
        Font getTextButtonFont (TextButton&, int h) override;
        Font getLabelFont (Label&) override;
        Font getComboBoxFont (ComboBox&) override;
        Font getPopupMenuFont() override;
        void drawButtonBackground (Graphics&, Button&, const Colour&, bool over, bool down) override;
        void drawToggleButton (Graphics&, ToggleButton&, bool over, bool down) override;

        static Colour bg()      { return Colour (0xff111014); }
        static Colour panel()   { return Colour (0xff1a181e); }
        static Colour line()    { return Colour (0xff2d2a33); }
        static Colour text()    { return Colour (0xfff2eef5); }
        static Colour dim()     { return Colour (0xff8d8796); }
        static Colour accent()  { return Colour (0xffff6a2b); }
        static Colour accent2() { return Colour (0xffffc24b); }
        static Font ui (float h, bool heading = false);
    };

    class BeatReelEditor : public AudioProcessorEditor, private Timer
    {
    public:
        explicit BeatReelEditor (BeatReelProcessor&);
        ~BeatReelEditor() override;
        void paint (Graphics&) override;
        void resized() override;

        BeatReelProcessor& proc;
        void styleChanged();                      // call after editing proc.style

        // intro preview: plays the intro (and the start of the beat) from the top
        void startIntroPreview();
        void stopIntroPreview()                   { introPreview = false; }
        bool isIntroPreviewing() const            { return introPreview; }
        double getIntroPreviewTime() const        { return introPreview ? introT : -1.0; }
        bool introLoop = false;
        double lastPreviewT = 0.0;               // capture time shown in the preview (for live UI bits)
        void showMessage (const String& title, const String& msg);

    private:
        void timerCallback() override;
        void showPage (int index);
        void showPresetsMenu();
        void startExport();
        void showActivation();
        void chooseFile (const String& title, const String& patterns, std::function<void (const File&)> cb, bool save = false, File initial = {});

        UiLookAndFeel lnf;
        class RecButton;     std::unique_ptr<RecButton> recButton;
        class Preview;       std::unique_ptr<Preview> preview;
        TextButton randomBtn { "RANDOMIZE" }, presetsBtn { "PRESETS" }, importBtn { "IMPORT AUDIO" },
                   midiBtn { "LOAD .MID" }, exportBtn { "EXPORT" }, activateBtn { "ACTIVATE" };
        TextButton tabs[5];
        std::unique_ptr<Component> pages[5];
        Viewport pageView;
        int currentPage = 0;
        TextButton playBtn { "PAUSE" }, aspectBtns[kNumAspects];
        Slider scrub;
        Label timeLabel;
        std::unique_ptr<FileChooser> chooser;
        int seenStyleVersion = -1;
        bool exportWasBusy = false;
        bool introPreview = false;
        double introStartMs = 0, introT = 0;
        String lastShownError;
        Random rng;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BeatReelEditor)
    };
}
