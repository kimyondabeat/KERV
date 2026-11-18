#include "PluginEditor.h"

using namespace Theme;

namespace
{
    juce::String categoryName (int c) { return c >= 0 ? juce::String (categories[(size_t) c].label) : juce::String(); }
    juce::String shortName (const SamplePtr& s) { return s != nullptr ? s->name : juce::String ("Empty"); }

    void drawBox (juce::Graphics& g, juce::Rectangle<float> r, float radius = 10.0f)
    {
        g.setColour (panel);
        g.fillRoundedRectangle (r, radius);
        g.setColour (line);
        g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
    }
}

// ===========================================================================
// Pads
PadView::PadView (MainView& m, int i)
    : main (m), index (i),
      vol (m.processor.apvts, KervProcessor::pid (i, "vol"), "Vol", false),
      pan (m.processor.apvts, KervProcessor::pid (i, "pan"), "Pan", false)
{
    addAndMakeVisible (vol);
    addAndMakeVisible (pan);
}

void PadView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const bool selected = main.selectedPad == index;
    const auto s = main.processor.getPadSample (index);

    g.setColour (panel);
    g.fillRoundedRectangle (r, 10.0f);

    auto hit = r.reduced (6.0f).withHeight (r.getHeight() * 0.56f);
    const auto base = s != nullptr ? panel2 : bg;
    g.setGradientFill (juce::ColourGradient (base.interpolatedWith (primary, flash * 0.95f), hit.getX(), hit.getY(),
                                             base.interpolatedWith (accent, flash * 0.6f), hit.getRight(), hit.getBottom(), false));
    g.fillRoundedRectangle (hit, 8.0f);

    if (selected || dropHover)
    {
        g.setColour ((dropHover ? accent : primary).withAlpha (0.35f));
        g.drawRoundedRectangle (r, 10.0f, 4.0f);
        g.setColour (dropHover ? accent : primary);
        g.drawRoundedRectangle (r, 10.0f, 1.8f);
    }
    else
    {
        g.setColour (line);
        g.drawRoundedRectangle (r, 10.0f, 1.0f);
    }

    auto t = hit.toNearestInt().reduced (8, 5);
    auto top = t.removeFromTop (14);
    g.setColour (selected ? accent : dim);
    g.setFont (font (11.0f, true));
    g.drawText (juce::String (index + 1), top, juce::Justification::left);
    g.setColour (faint);
    g.setFont (font (9.5f, true));
    g.drawText (categoryName (main.processor.getPadCategory (index)).toUpperCase(), top, juce::Justification::right);

    g.setColour (s != nullptr ? text : faint);
    g.setFont (font (12.5f, true));
    g.drawFittedText (dropHover ? juce::String ("DROP HERE") : shortName (s), t, juce::Justification::centred, 2, 0.8f);
}

void PadView::resized()
{
    auto r = getLocalBounds().reduced (6);
    r.removeFromTop ((int) (getHeight() * 0.56f) + 2);
    vol.setBounds (r.removeFromLeft (r.getWidth() / 2));
    pan.setBounds (r);
}

void PadView::mouseDown (const juce::MouseEvent& e)
{
    main.selectPad (index);
    if (e.y < getHeight() * 0.6f)
    {
        const float vel = juce::jmap ((float) e.y, 0.0f, getHeight() * 0.6f, 0.6f, 1.0f);
        main.processor.triggerPad (index, juce::jlimit (0.1f, 1.0f, vel));
    }
}

void PadView::mouseUp (const juce::MouseEvent&)
{
    main.processor.releasePad (index);
}

void PadView::refresh()
{
    bool dirty = false;
    const auto hits = main.processor.getHitCount (index);
    if (hits != lastHits) { lastHits = hits; flash = 1.0f; dirty = true; }
    else if (flash > 0.0f) { flash = juce::jmax (0.0f, flash - 0.09f); dirty = true; }

    const auto name = shortName (main.processor.getPadSample (index));
    if (name != sampleName) { sampleName = name; dirty = true; }
    if (dirty) repaint();
}

bool PadView::isInterestedInDragSource (const SourceDetails& d)
{
    const auto desc = d.description.toString();
    if (! desc.startsWith ("kerv-sample:"))
        return false;
    // a pad only accepts sounds from its own category
    return main.processor.canAssign (index, Library::get().find (desc.fromFirstOccurrenceOf ("kerv-sample:", false, false)));
}

void PadView::itemDropped (const SourceDetails& d)
{
    dropHover = false;
    const auto id = d.description.toString().fromFirstOccurrenceOf ("kerv-sample:", false, false);
    if (auto s = Library::get().find (id))
        main.assignToPad (index, s);
}

PadsPanel::PadsPanel (MainView& m)
{
    for (int i = 0; i < numPads; ++i)
    {
        pads.push_back (std::make_unique<PadView> (m, i));
        addAndMakeVisible (*pads.back());
    }
}

void PadsPanel::resized()
{
    const int gap = 8, cols = 8;
    const int w = (getWidth() - gap * (cols - 1)) / cols;
    const int h = (getHeight() - gap) / 2;
    for (int i = 0; i < numPads; ++i)
    {
        const int col = i % cols;
        const int row = i < cols ? 1 : 0; // pad 1 bottom-left, like a hardware drum machine
        pads[(size_t) i]->setBounds (col * (w + gap), row * (h + gap), w, h);
    }
}

void PadsPanel::refresh()
{
    for (auto& p : pads) p->refresh();
}

// ===========================================================================
// Step sequencer
StepGrid::StepGrid (MainView& m) : main (m) {}

bool StepGrid::cellAt (juce::Point<int> pt, int& pad, int& step) const
{
    const float rowH = getHeight() / (float) numPads;
    const float cellW = (getWidth() - labelW) / (float) numSteps;
    pad = (int) (pt.y / rowH);
    step = (int) ((pt.x - labelW) / cellW);
    return pad >= 0 && pad < numPads && pt.x >= labelW && step >= 0 && step < numSteps;
}

void StepGrid::paint (juce::Graphics& g)
{
    const float rowH = getHeight() / (float) numPads;
    const float cellW = (getWidth() - labelW) / (float) numSteps;
    const int playing = main.processor.getPlayingStep();

    for (int p = 0; p < numPads; ++p)
    {
        const float y = p * rowH;
        const bool sel = main.selectedPad == p;
        auto label = juce::Rectangle<float> (0, y, (float) labelW - 6, rowH).reduced (0, 1);
        g.setColour (sel ? primary.withAlpha (0.35f) : panel2.withAlpha (0.6f));
        g.fillRoundedRectangle (label, 3.0f);
        g.setColour (sel ? text : dim);
        g.setFont (font (juce::jmin (11.0f, rowH * 0.78f), sel));
        const auto s = main.processor.getPadSample (p);
        g.drawText (juce::String (p + 1).paddedLeft ('0', 2) + "  " + (s ? s->name : juce::String ("-")),
                    label.reduced (6, 0).toNearestInt(), juce::Justification::centredLeft);

        for (int st = 0; st < numSteps; ++st)
        {
            auto c = juce::Rectangle<float> (labelW + st * cellW, y, cellW, rowH).reduced (1.5f, 1.0f);
            const int v = main.processor.getStep (p, st);
            juce::Colour col = (st / 4) % 2 == 0 ? panel2 : panel2.brighter (0.06f);
            if (v == 1) col = primary;
            if (v == 2) col = accent;
            if (st == playing) col = col.brighter (v ? 0.25f : 0.35f);
            g.setColour (col);
            g.fillRoundedRectangle (c, 2.5f);
        }
    }

    if (playing >= 0)
    {
        g.setColour (accent.withAlpha (0.9f));
        g.fillRect (labelW + playing * cellW + 1.5f, (float) getHeight() - 2.0f, cellW - 3.0f, 2.0f);
    }
}

void StepGrid::mouseDown (const juce::MouseEvent& e)
{
    int pad, step;
    if (e.x < labelW)
    {
        const int row = (int) (e.y / (getHeight() / (float) numPads));
        if (row >= 0 && row < numPads)
        {
            main.selectPad (row);
            main.processor.triggerPad (row, 0.9f);
        }
        return;
    }
    if (! cellAt (e.getPosition(), pad, step))
        return;

    const int cur = main.processor.getStep (pad, step);
    paintValue = (e.mods.isRightButtonDown() || e.mods.isShiftDown()) ? (cur == 2 ? 1 : 2) : (cur == 0 ? 1 : 0);
    main.processor.setStep (pad, step, paintValue);
    lastPad = pad; lastStep = step;
    repaint();
}

void StepGrid::mouseDrag (const juce::MouseEvent& e)
{
    int pad, step;
    if (e.mouseDownPosition.x < labelW || ! cellAt (e.getPosition(), pad, step))
        return;
    if (pad == lastPad && step == lastStep)
        return;
    main.processor.setStep (pad, step, paintValue);
    lastPad = pad; lastStep = step;
    repaint();
}

SequencerPanel::SequencerPanel (MainView& m) : main (m), grid (m)
{
    playButton.setClickingTogglesState (true);
    playButton.onClick = [this] { main.processor.setInternalPlay (playButton.getToggleState()); };
    playButton.setTooltip ("Play the pattern on its own. Inside your DAW it plays with the song.");
    clearButton.onClick = [this] { main.processor.clearPattern(); grid.repaint(); };
    defaultButton.setTooltip ("Load the default Kompa groove");
    defaultButton.onClick = [this] { main.processor.loadDefaultPattern(); grid.repaint(); };

    for (auto* c : std::initializer_list<juce::Component*> { &playButton, &clearButton, &defaultButton, &grid })
        addAndMakeVisible (c);
}

void SequencerPanel::paint (juce::Graphics& g)
{
    drawBox (g, getLocalBounds().toFloat());

    auto gb = grid.getBounds();
    const float cellW = (gb.getWidth() - StepGrid::labelW) / (float) numSteps;
    g.setFont (font (9.5f, true));
    for (int s = 0; s < numSteps; ++s)
    {
        g.setColour (s % 4 == 0 ? accent : faint);
        g.drawText (juce::String (s + 1), juce::Rectangle<float> (gb.getX() + StepGrid::labelW + s * cellW, (float) gb.getY() - 12, cellW, 11).toNearestInt(),
                    juce::Justification::centred);
    }
}

void SequencerPanel::resized()
{
    auto r = getLocalBounds().reduced (12, 8);
    auto bar = r.removeFromTop (30);
    playButton.setBounds (bar.removeFromLeft (80).reduced (0, 2));
    bar.removeFromLeft (8);
    clearButton.setBounds (bar.removeFromLeft (70).reduced (0, 2));
    bar.removeFromLeft (8);
    defaultButton.setBounds (bar.removeFromLeft (130).reduced (0, 2));
    r.removeFromTop (16);
    grid.setBounds (r);
}

void SequencerPanel::refresh()
{
    playButton.setButtonText (main.processor.getInternalPlay() ? "STOP" : "PLAY");
    grid.repaint();
}

// ===========================================================================
// Quick Mix: 16 channel strips + channel FX for the selected pad
MixStrip::MixStrip (MainView& m, int i)
    : main (m), index (i),
      pan (m.processor.apvts, KervProcessor::pid (i, "pan"), "Pan", false),
      faderAttachment (m.processor.apvts, KervProcessor::pid (i, "vol"), fader),
      muteAttachment (m.processor.apvts, KervProcessor::pid (i, "mute"), mute),
      soloAttachment (m.processor.apvts, KervProcessor::pid (i, "solo"), solo)
{
    fader.setSliderStyle (juce::Slider::LinearVertical);
    fader.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    fader.setPopupDisplayEnabled (true, true, nullptr);
    fader.setDoubleClickReturnValue (true, 1.0);
    fader.onDragStart = [this] { main.selectPad (index); };
    mute.setClickingTogglesState (true);
    solo.setClickingTogglesState (true);
    mute.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffe0475b));
    solo.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffe6b422));
    for (auto* c : std::initializer_list<juce::Component*> { &fader, &mute, &solo, &pan })
        addAndMakeVisible (c);
}

void MixStrip::paint (juce::Graphics& g)
{
    const bool sel = main.selectedPad == index;
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (sel ? panel2.brighter (0.05f) : panel);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (sel ? primary : line);
    g.drawRoundedRectangle (r, 8.0f, sel ? 1.5f : 1.0f);

    auto t = getLocalBounds().reduced (3, 4);
    g.setColour (sel ? accent : text);
    g.setFont (font (11.5f, true));
    g.drawText (juce::String (index + 1), t.removeFromTop (13), juce::Justification::centred);
    g.setColour (dim);
    g.setFont (font (8.5f, true));
    g.drawFittedText (categoryName (main.processor.getPadCategory (index)).toUpperCase(), t.removeFromTop (12), juce::Justification::centredTop, 1, 0.6f);
}

void MixStrip::resized()
{
    auto r = getLocalBounds().reduced (3, 4);
    r.removeFromTop (28);
    auto buttons = r.removeFromBottom (20);
    mute.setBounds (buttons.removeFromLeft (buttons.getWidth() / 2).reduced (1, 0));
    solo.setBounds (buttons.reduced (1, 0));
    r.removeFromBottom (2);
    pan.setBounds (r.removeFromBottom (44));
    fader.setBounds (r);
}

void MixStrip::mouseDown (const juce::MouseEvent&)
{
    main.selectPad (index);
}

MixPanel::MixPanel (MainView& m) : main (m)
{
    for (int i = 0; i < numPads; ++i)
    {
        strips.push_back (std::make_unique<MixStrip> (m, i));
        addAndMakeVisible (*strips.back());
    }
    resetFx.setTooltip ("Reset the EQ, compressor and saturation of this pad");
    resetFx.onClick = [this] { main.processor.resetChannelFx (main.selectedPad); };
    addAndMakeVisible (resetFx);
    padChanged();
}

void MixPanel::padChanged()
{
    fx.clear();
    const std::pair<const char*, const char*> list[] = { { "eqlow", "Low" }, { "eqmid", "Mid" }, { "eqhigh", "High" },
                                                         { "comp", "Compressor" }, { "sat", "Saturation" } };
    for (const auto& [id, name] : list)
    {
        fx.push_back (std::make_unique<Knob> (main.processor.apvts, KervProcessor::pid (main.selectedPad, id), name));
        addAndMakeVisible (*fx.back());
    }
    resized();
    repaint();
}

void MixPanel::paint (juce::Graphics& g)
{
    drawBox (g, fxArea.toFloat());
    auto r = fxArea.reduced (14, 10);
    g.setColour (accent);
    g.setFont (font (11.0f, true));
    g.drawText ("CHANNEL FX  -  PAD " + juce::String (main.selectedPad + 1), r.removeFromTop (14), juce::Justification::left);
    const auto s = main.processor.getPadSample (main.selectedPad);
    g.setColour (dim);
    g.setFont (font (11.0f));
    g.drawText (s ? s->name : juce::String ("(empty pad)"), r.removeFromTop (16), juce::Justification::left);

    g.setColour (faint);
    g.setFont (font (10.0f, true));
    g.drawText ("EQUALIZER", fxArea.getX() + 14, fx[0]->getY() - 14, 120, 12, juce::Justification::left);
    g.drawText ("DYNAMICS", fxArea.getX() + 14, fx[3]->getY() - 14, 120, 12, juce::Justification::left);
}

void MixPanel::resized()
{
    auto r = getLocalBounds();
    fxArea = r.removeFromRight (300);
    r.removeFromRight (8);

    const int gap = 3;
    const int w = (r.getWidth() - gap * (numPads - 1)) / numPads;
    for (int i = 0; i < numPads; ++i)
        strips[(size_t) i]->setBounds (r.getX() + i * (w + gap), r.getY(), w, r.getHeight());

    if (fx.size() == 5)
    {
        auto a = fxArea.reduced (12, 10);
        a.removeFromTop (34);
        resetFx.setBounds (a.removeFromBottom (24).removeFromRight (70));
        const int rowH = (a.getHeight() - 14) / 2;
        auto row1 = a.removeFromTop (rowH).withTrimmedTop (14);
        a.removeFromTop (14);
        auto row2 = a.withTrimmedTop (14);
        const int cw = row1.getWidth() / 3;
        for (int i = 0; i < 3; ++i) fx[(size_t) i]->setBounds (row1.getX() + i * cw, row1.getY(), cw, row1.getHeight());
        for (int i = 0; i < 2; ++i) fx[(size_t) (3 + i)]->setBounds (row2.getX() + i * cw, row2.getY(), cw, row2.getHeight());
    }
}

// ===========================================================================
// LAYER
LayerPanel::LayerPanel (MainView& m) : main (m)
{
    for (int c = 0; c < numCategories; ++c)
        categoryBox.addItem (categories[(size_t) c].label, c + 1);
    categoryBox.onChange = [this] { fillList(); };

    list.setModel (this);
    list.setRowHeight (22);
    list.setOutlineThickness (0);

    mainWave.setEmptyText ("This pad is empty");
    layerWave.setEmptyText ("Pick a layer sound from the list");

    onButton.setClickingTogglesState (true);
    playButton.onClick = [this] { main.processor.triggerPad (main.selectedPad, 0.95f); };
    removeButton.onClick = [this] { main.processor.assignLayer (main.selectedPad, nullptr); padChanged(); };

    for (auto* c : std::initializer_list<juce::Component*> { &categoryBox, &list, &mainWave, &layerWave, &onButton, &playButton, &removeButton })
        addAndMakeVisible (c);
    padChanged();
}

void LayerPanel::padChanged()
{
    const int p = main.selectedPad;
    onAttachment.reset();
    onAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (main.processor.apvts, KervProcessor::pid (p, "layer"), onButton);

    knobs.clear();
    const std::pair<const char*, const char*> list2[] = { { "blend", "Blend" }, { "lpitch", "Layer Pitch" }, { "loffset", "Offset" } };
    for (const auto& [id, name] : list2)
    {
        knobs.push_back (std::make_unique<Knob> (main.processor.apvts, KervProcessor::pid (p, id), name));
        addAndMakeVisible (*knobs.back());
    }

    mainWave.setSample (main.processor.getPadSample (p));
    const auto layer = main.processor.getLayerSample (p);
    layerWave.setSample (layer);
    const int cat = layer != nullptr ? layer->category : main.processor.getPadCategory (p);
    categoryBox.setSelectedId (cat + 1, juce::dontSendNotification);
    fillList();
    resized();
    repaint();
}

void LayerPanel::fillList()
{
    sounds = Library::get().inCategory (categoryBox.getSelectedId() - 1);
    list.updateContent();
    list.repaint();
}

void LayerPanel::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool)
{
    if (row < 0 || row >= (int) sounds.size())
        return;
    const bool sel = main.processor.getLayerSample (main.selectedPad) == sounds[(size_t) row];
    g.setColour (sel ? primary.withAlpha (0.3f) : (row % 2 ? panel2.withAlpha (0.5f) : juce::Colours::transparentBlack));
    g.fillRoundedRectangle (juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (1.0f), 4.0f);
    g.setColour (text);
    g.setFont (font (12.0f, sel));
    g.drawText (sounds[(size_t) row]->name, juce::Rectangle<int> (10, 0, w - 14, h), juce::Justification::centredLeft);
}

void LayerPanel::listBoxItemClicked (int row, const juce::MouseEvent&)
{
    if (row < 0 || row >= (int) sounds.size())
        return;
    const int p = main.selectedPad;
    main.processor.assignLayer (p, sounds[(size_t) row]);
    if (auto* param = main.processor.apvts.getParameter (KervProcessor::pid (p, "layer")))
        param->setValueNotifyingHost (1.0f); // picking a layer switches it on
    layerWave.setSample (sounds[(size_t) row]);
    list.repaint();
    main.processor.triggerPad (p, 0.95f);
}

void LayerPanel::paint (juce::Graphics& g)
{
    drawBox (g, getLocalBounds().toFloat());
    const int p = main.selectedPad;
    g.setColour (accent);
    g.setFont (font (11.0f, true));
    g.drawText ("LAYER  -  PAD " + juce::String (p + 1) + "  " + categoryName (main.processor.getPadCategory (p)).toUpperCase(),
                14, 10, 400, 14, juce::Justification::left);
    g.setColour (faint);
    g.setFont (font (10.0f, true));
    g.drawText ("MAIN SOUND", mainWave.getX(), mainWave.getY() - 14, 200, 12, juce::Justification::left);
    g.drawText ("LAYER SOUND", layerWave.getX(), layerWave.getY() - 14, 200, 12, juce::Justification::left);
}

void LayerPanel::resized()
{
    auto r = getLocalBounds().reduced (12, 10);
    r.removeFromTop (20);

    auto left = r.removeFromLeft (300);
    categoryBox.setBounds (left.removeFromTop (28));
    left.removeFromTop (6);
    list.setBounds (left);
    r.removeFromLeft (14);

    auto right = r.removeFromRight (330);
    auto buttons = right.removeFromTop (28);
    onButton.setBounds (buttons.removeFromLeft (100));
    buttons.removeFromLeft (6);
    playButton.setBounds (buttons.removeFromLeft (100));
    buttons.removeFromLeft (6);
    removeButton.setBounds (buttons);
    right.removeFromTop (10);
    const int cw = right.getWidth() / 3;
    for (size_t i = 0; i < knobs.size(); ++i)
        knobs[i]->setBounds (right.getX() + (int) i * cw, right.getY(), cw, right.getHeight());
    r.removeFromRight (14);

    const int h = (r.getHeight() - 28) / 2;
    r.removeFromTop (14);
    mainWave.setBounds (r.removeFromTop (h));
    r.removeFromTop (14);
    layerWave.setBounds (r.removeFromTop (h));
}
