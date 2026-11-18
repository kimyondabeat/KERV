#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <set>
#include "PluginProcessor.h"
#include "Theme.h"

class MainView;

// ---------------------------------------------------------------------------
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState&, const juce::String& paramID, const juce::String& caption, bool showValue = true);
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    juce::Slider slider;
    juce::String caption;
    bool showValue;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};

// ---------------------------------------------------------------------------
// Left sidebar: All / Favorites / 12 categories
class CategoryList : public juce::Component
{
public:
    static constexpr int allSounds = -2, favourites = -1;
    explicit CategoryList (MainView&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = -99; repaint(); }

private:
    int rowAt (int y) const;
    juce::Rectangle<int> rowBounds (int row) const;
    MainView& main;
    int hover = -99;
};

// ---------------------------------------------------------------------------
class WaveformView : public juce::Component
{
public:
    void setSample (SamplePtr s) { sample = std::move (s); repaint(); }
    void setEmptyText (const juce::String& t) { emptyText = t; }
    void paint (juce::Graphics&) override;
private:
    SamplePtr sample;
    juce::String emptyText { "Select a sound to see its waveform" };
};

// Centre: search, list, waveform
class LibraryPanel : public juce::Component,
                     private juce::ListBoxModel
{
public:
    explicit LibraryPanel (MainView&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refreshList();
    void showSample (SamplePtr);
    void clearSearch() { search.setText ({}, juce::dontSendNotification); }
    void flashMessage (const juce::String& m) { message = m; messageTime = juce::Time::getMillisecondCounter(); repaint(); }

private:
    int getNumRows() override { return (int) shown.size(); }
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
    juce::var getDragSourceDescription (const juce::SparseSet<int>& rows) override;
    void updateAssignButton();

    MainView& main;
    juce::TextEditor search;
    juce::ListBox list;
    WaveformView wave;
    juce::TextButton playButton { "PLAY" }, favButton { "ADD FAVORITE" }, assignButton { "ASSIGN TO PAD" };
    std::vector<SamplePtr> shown;
    SamplePtr current;
    juce::String message;
    juce::uint32 messageTime = 0;

    friend class MainView;
};

// ---------------------------------------------------------------------------
// Right: sound controls for the selected pad
class ControlsPanel : public juce::Component
{
public:
    explicit ControlsPanel (MainView&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void padChanged();

private:
    MainView& main;
    std::vector<std::unique_ptr<Knob>> knobs;
    juce::TextButton prevSound { "<" }, nextSound { ">" }, clearPad { "CLEAR" }, resetButton { "RESET" };
    int effectsY = 0;
};

// ---------------------------------------------------------------------------
class PadView : public juce::Component,
                public juce::DragAndDropTarget
{
public:
    PadView (MainView&, int index);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void refresh();

    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override { dropHover = true; repaint(); }
    void itemDragExit (const SourceDetails&) override { dropHover = false; repaint(); }
    void itemDropped (const SourceDetails&) override;

private:
    MainView& main;
    const int index;
    float flash = 0.0f;
    uint32_t lastHits = 0;
    bool dropHover = false;
    juce::String sampleName;
    Knob vol, pan;
};

class PadsPanel : public juce::Component
{
public:
    explicit PadsPanel (MainView&);
    void resized() override;
    void refresh();
private:
    std::vector<std::unique_ptr<PadView>> pads;
};

// ---------------------------------------------------------------------------
class StepGrid : public juce::Component
{
public:
    explicit StepGrid (MainView&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    static constexpr int labelW = 132;
private:
    bool cellAt (juce::Point<int>, int& pad, int& step) const;
    MainView& main;
    int paintValue = 1;
    int lastPad = -1, lastStep = -1;
};

class SequencerPanel : public juce::Component
{
public:
    explicit SequencerPanel (MainView&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();
private:
    MainView& main;
    juce::TextButton playButton { "PLAY" }, clearButton { "CLEAR" }, defaultButton { "KOMPA GROOVE" };
    StepGrid grid;
};

// ---------------------------------------------------------------------------
class MixStrip : public juce::Component
{
public:
    MixStrip (MainView&, int index);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
private:
    MainView& main;
    const int index;
    juce::Slider fader;
    juce::TextButton mute { "M" }, solo { "S" };
    Knob pan;
    juce::AudioProcessorValueTreeState::SliderAttachment faderAttachment;
    juce::AudioProcessorValueTreeState::ButtonAttachment muteAttachment, soloAttachment;
};

class MixPanel : public juce::Component
{
public:
    explicit MixPanel (MainView&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void padChanged();
private:
    MainView& main;
    std::vector<std::unique_ptr<MixStrip>> strips;
    std::vector<std::unique_ptr<Knob>> fx;
    juce::TextButton resetFx { "RESET" };
    juce::Rectangle<int> fxArea;
};

// ---------------------------------------------------------------------------
// LAYER: stack a second sound on the selected pad and blend them.
class LayerPanel : public juce::Component,
                   private juce::ListBoxModel
{
public:
    explicit LayerPanel (MainView&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void padChanged();

private:
    int getNumRows() override { return (int) sounds.size(); }
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    void fillList();

    MainView& main;
    juce::ComboBox categoryBox;
    juce::ListBox list;
    WaveformView mainWave, layerWave;
    juce::TextButton onButton { "LAYER ON" }, playButton { "PLAY PAD" }, removeButton { "REMOVE" };
    std::vector<std::unique_ptr<Knob>> knobs;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAttachment;
    std::vector<SamplePtr> sounds;
};

// ---------------------------------------------------------------------------
// HOME: the drum kit drawn as real instruments. Click to play; lights up on every hit.
class KitView : public juce::Component
{
public:
    explicit KitView (MainView&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = -1; hoverPlus = -1; repaint(); }
    void refresh();

    enum Shape { cymbalShape, drumShape, padShape, cowbellShape, congaShape, triangleShape };
    struct Piece { const char* name; float cx, cy, w, h; Shape shape; int pads[2]; };

private:
    juce::Rectangle<float> boundsOf (int piece) const;
    juce::Rectangle<float> plusOf (int piece) const;
    int pieceAt (juce::Point<float>) const;
    int plusAt (juce::Point<float>) const;
    MainView& main;
    std::array<float, 12> glow {};
    std::array<uint32_t, numPads> lastHits {};
    int hover = -1, hoverPlus = -1, heldPad = -1;
};

// ---------------------------------------------------------------------------
class MainView : public juce::Component,
                 public juce::DragAndDropContainer
{
public:
    static constexpr int baseW = 1200, baseH = 780;

    MainView (KervProcessor&, std::function<void (float)> onScale);
    ~MainView() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void tick(); // 30 Hz from the editor

    KervProcessor& processor;
    std::unique_ptr<juce::PropertiesFile> settings;

    // shared UI state
    int selectedPad = 0;
    int category = CategoryList::allSounds;
    juce::String searchText;
    std::set<juce::String> favourites;

    void selectPad (int pad);
    void setCategory (int c);
    void setSearch (const juce::String& s);
    bool isFavourite (const juce::String& id) const { return favourites.count (id) > 0; }
    void toggleFavourite (const juce::String& id);
    bool assignToPad (int pad, SamplePtr);
    void stepPadSound (int pad, int delta);
    void kitChanged();
    void openPadInLibrary (int pad);   // EDITOR + the pad's category
    void setHome (bool home);
    void applyColours (int palette);

private:
    void buildPresetMenu();
    void loadPresetIndex (int index);
    void savePreset();
    void newKit();
    void showSettings();
    void showTab (int tab);

    std::function<void (float)> onScale;
    std::vector<std::unique_ptr<juce::XmlElement>> factoryPresets;
    juce::Array<juce::File> userPresetFiles;

    juce::ComboBox presetBox;
    juce::TextButton prevPreset { "<" }, nextPreset { ">" }, saveButton { "SAVE" }, newButton { "NEW" },
                     randomButton { "RANDOMIZE" }, settingsButton { "SETTINGS" };
    Knob master;

    CategoryList categoryList;
    LibraryPanel library;
    ControlsPanel controls;
    juce::TextButton homeButton { "HOME" }, editorButton { "EDITOR" };
    KitView kitView;
    juce::TextButton padsTab { "PADS" }, seqTab { "SEQUENCER" }, mixTab { "QUICK MIX" }, layerTab { "LAYER" };
    PadsPanel padsPanel;
    SequencerPanel seqPanel;
    MixPanel mixPanel;
    LayerPanel layerPanel;
    juce::String shownPresetName;
    int currentTab = 0;
    bool homeMode = true;
    std::array<uint32_t, numPads> trigSeen {};
    float trigLevel = 0.0f;
    juce::Rectangle<int> trigBounds;

    friend class KervEditor;
    friend class KitView;
};

// ---------------------------------------------------------------------------
class KervEditor : public juce::AudioProcessorEditor,
                   private juce::Timer
{
public:
    explicit KervEditor (KervProcessor&);
    ~KervEditor() override;
    void resized() override;
    KervLookAndFeel& getKervLookAndFeel() { return lnf; }

private:
    void timerCallback() override { view.tick(); }
    void setScale (float);

    KervLookAndFeel lnf;
    MainView view;
    juce::TooltipWindow tooltips { this, 600 };
    float scale = 0.9f;
};
