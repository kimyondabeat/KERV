#include "PluginEditor.h"

using namespace Theme;

namespace
{
    void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title = {})
    {
        g.setColour (panel);
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (line);
        g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);
        if (title.isNotEmpty())
        {
            g.setColour (accent);
            g.setFont (font (11.0f, true));
            g.drawText (title, r.reduced (14, 10).removeFromTop (14).toNearestInt(), juce::Justification::topLeft);
        }
    }

    juce::String categoryName (int c) { return c >= 0 ? juce::String (categories[(size_t) c].label) : juce::String(); }
}

// ===========================================================================
Knob::Knob (juce::AudioProcessorValueTreeState& state, const juce::String& id, const juce::String& cap, bool value)
    : caption (cap), showValue (value), attachment (state, id, slider)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setPopupDisplayEnabled (true, true, nullptr);
    if (auto* p = state.getParameter (id))
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    slider.onValueChange = [this] { repaint(); };
    addAndMakeVisible (slider);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    r.removeFromBottom (showValue ? 28 : 14);
    const int d = juce::jmin (r.getWidth(), r.getHeight(), 62);
    slider.setBounds (r.withSizeKeepingCentre (d, d).withY (r.getBottom() - d));
}

void Knob::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    if (showValue)
    {
        auto v = r.removeFromBottom (14);
        g.setColour (text);
        g.setFont (font (11.0f));
        g.drawText (slider.getTextFromValue (slider.getValue()), v, juce::Justification::centred);
    }
    g.setColour (dim);
    g.setFont (font (10.5f, true));
    g.drawText (caption.toUpperCase(), r.removeFromBottom (14), juce::Justification::centred);
}

// ===========================================================================
CategoryList::CategoryList (MainView& m) : main (m) {}

juce::Rectangle<int> CategoryList::rowBounds (int row) const
{
    const int top = 32, h = 25;
    const int y = top + row * h + (row >= 2 ? 6 : 0);
    return { 8, y, getWidth() - 16, h - 2 };
}

int CategoryList::rowAt (int y) const
{
    for (int r = 0; r < numCategories + 2; ++r)
        if (rowBounds (r).contains (12, y))
            return r;
    return -99;
}

void CategoryList::paint (juce::Graphics& g)
{
    drawPanel (g, getLocalBounds().toFloat(), "DRUM CATEGORIES");
    const auto& lib = Library::get();

    for (int r = 0; r < numCategories + 2; ++r)
    {
        const int cat = r == 0 ? allSounds : r == 1 ? favourites : r - 2;
        auto b = rowBounds (r);
        const bool sel = main.category == cat;

        if (sel)
        {
            g.setGradientFill (juce::ColourGradient (primary, (float) b.getX(), 0, primary.withAlpha (0.55f), (float) b.getRight(), 0, false));
            g.fillRoundedRectangle (b.toFloat(), 6.0f);
        }
        else if (hover == r)
        {
            g.setColour (panel2);
            g.fillRoundedRectangle (b.toFloat(), 6.0f);
        }

        juce::String label, count;
        if (cat == allSounds)       { label = "All Sounds"; count = juce::String ((int) lib.all().size()); }
        else if (cat == favourites) { label = "     Favorites"; count = juce::String ((int) main.favourites.size());
                                      drawStar (g, b.reduced (10, 0).removeFromLeft (14).toFloat().withSizeKeepingCentre (13, 13), true, star); }
        else                        { label = juce::String (r - 1) + ".  " + categories[(size_t) cat].label;
                                      count = juce::String ((int) lib.inCategory (cat).size()); }

        auto tb = b.reduced (10, 0);
        g.setColour (sel ? onPrimary : text.withAlpha (0.88f));
        g.setFont (font (13.0f, sel));
        g.drawText (label, tb, juce::Justification::centredLeft);
        g.setColour (sel ? onPrimary.withAlpha (0.85f) : faint);
        g.setFont (font (11.0f));
        g.drawText (count, tb, juce::Justification::centredRight);
    }
}

void CategoryList::mouseDown (const juce::MouseEvent& e)
{
    const int r = rowAt (e.y);
    if (r >= 0)
        main.setCategory (r == 0 ? allSounds : r == 1 ? favourites : r - 2);
}

void CategoryList::mouseMove (const juce::MouseEvent& e)
{
    const int r = rowAt (e.y);
    if (r != hover) { hover = r; repaint(); }
}

// ===========================================================================
void WaveformView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (bg);
    g.fillRoundedRectangle (r, 8.0f);

    if (sample == nullptr || sample->peaks.empty())
    {
        g.setColour (faint);
        g.setFont (font (12.0f));
        g.drawText (emptyText, r, juce::Justification::centred);
        return;
    }

    auto area = r.reduced (10, 6);
    auto infoRow = area.removeFromTop (16);
    const float mid = area.getCentreY();
    const float half = area.getHeight() * 0.5f;
    const auto& p = sample->peaks;
    juce::Path path;
    path.startNewSubPath (area.getX(), mid);
    for (size_t i = 0; i < p.size(); ++i)
        path.lineTo (area.getX() + area.getWidth() * (float) i / (float) (p.size() - 1), mid - p[i] * half);
    for (size_t i = p.size(); i-- > 0;)
        path.lineTo (area.getX() + area.getWidth() * (float) i / (float) (p.size() - 1), mid + p[i] * half);
    path.closeSubPath();

    g.setGradientFill (juce::ColourGradient (accent, area.getX(), mid - half, primary, area.getX(), mid + half, false));
    g.fillPath (path);

    const double secs = sample->buffer.getNumSamples() / sample->sampleRate;
    g.setColour (text);
    g.setFont (font (12.0f, true));
    g.drawText (sample->name, infoRow.toNearestInt(), juce::Justification::centredLeft);
    g.setColour (dim);
    g.setFont (font (11.0f));
    g.drawText (categoryName (sample->category) + "   " + juce::String (secs, 2) + " s", infoRow.toNearestInt(), juce::Justification::centredRight);
}

// ===========================================================================
LibraryPanel::LibraryPanel (MainView& m) : main (m)
{
    search.setTextToShowWhenEmpty ("Search sounds...", faint);
    search.setFont (font (14.0f));
    search.setIndents (12, 7);
    search.onTextChange = [this] { main.setSearch (search.getText()); };
    addAndMakeVisible (search);

    list.setModel (this);
    list.setRowHeight (28);
    list.setOutlineThickness (0);
    addAndMakeVisible (list);
    addAndMakeVisible (wave);

    playButton.onClick = [this] { if (current) main.processor.preview (current); };
    favButton.onClick = [this] { if (current) { main.toggleFavourite (current->id); showSample (current); } };
    assignButton.onClick = [this] { if (current) main.assignToPad (main.selectedPad, current); };
    for (auto* b : { &playButton, &favButton, &assignButton })
        addAndMakeVisible (b);

    refreshList();
}

void LibraryPanel::paint (juce::Graphics& g)
{
    juce::String title = "SAMPLE LIBRARY";
    if (main.category >= 0) title << "  /  " << categoryName (main.category).toUpperCase();
    else if (main.category == CategoryList::favourites) title << "  /  FAVORITES";
    drawPanel (g, getLocalBounds().toFloat(), title);

    const bool showMessage = message.isNotEmpty() && juce::Time::getMillisecondCounter() - messageTime < 2500;
    g.setColour (showMessage ? accent : faint);
    g.setFont (font (11.0f, showMessage));
    g.drawText (showMessage ? message : juce::String ((int) shown.size()) + " sounds  -  drag a sound onto its pad",
                getLocalBounds().reduced (14, 10).removeFromTop (14), juce::Justification::topRight);
}

void LibraryPanel::resized()
{
    auto r = getLocalBounds().reduced (12);
    r.removeFromTop (22);
    search.setBounds (r.removeFromTop (32));
    r.removeFromTop (6);

    auto bottom = r.removeFromBottom (96);
    auto buttons = bottom.removeFromBottom (28);
    bottom.removeFromBottom (6);
    wave.setBounds (bottom);
    playButton.setBounds (buttons.removeFromLeft (80));
    buttons.removeFromLeft (6);
    favButton.setBounds (buttons.removeFromLeft (124));
    assignButton.setBounds (buttons.removeFromRight (190));
    r.removeFromBottom (6);
    list.setBounds (r);
}

void LibraryPanel::refreshList()
{
    shown.clear();
    const auto needle = main.searchText.trim().toLowerCase();

    for (const auto& s : Library::get().all())
    {
        if (main.category >= 0 && s->category != main.category) continue;
        if (main.category == CategoryList::favourites && ! main.isFavourite (s->id)) continue;
        if (needle.isNotEmpty() && ! s->name.toLowerCase().contains (needle)
            && ! categoryName (s->category).toLowerCase().contains (needle)) continue;
        shown.push_back (s);
    }

    list.updateContent();
    list.repaint();
    repaint();
}

void LibraryPanel::updateAssignButton()
{
    const int pad = main.selectedPad;
    const bool ok = current != nullptr && main.processor.canAssign (pad, current);
    assignButton.setEnabled (ok);
    assignButton.setButtonText (current == nullptr || ok ? "ASSIGN TO PAD " + juce::String (pad + 1)
                                                         : "PAD " + juce::String (pad + 1) + " = " + categoryName (main.processor.getPadCategory (pad)).toUpperCase() + " ONLY");
}

void LibraryPanel::showSample (SamplePtr s)
{
    current = std::move (s);
    wave.setSample (current);
    favButton.setToggleState (current != nullptr && main.isFavourite (current->id), juce::dontSendNotification);
    favButton.setButtonText (favButton.getToggleState() ? "IN FAVORITES" : "ADD FAVORITE");
    updateAssignButton();
    list.repaint();
}

void LibraryPanel::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool)
{
    if (row < 0 || row >= (int) shown.size())
        return;
    const auto& s = shown[(size_t) row];
    const bool sel = current == s;
    const bool fits = main.processor.canAssign (main.selectedPad, s);
    auto r = juce::Rectangle<int> (0, 0, w, h).reduced (2, 1);

    g.setColour (sel ? primary.withAlpha (0.28f) : (row % 2 ? panel2.withAlpha (0.5f) : juce::Colours::transparentBlack));
    g.fillRoundedRectangle (r.toFloat(), 5.0f);

    auto play = r.removeFromLeft (30).toFloat().withSizeKeepingCentre (16, 16);
    g.setColour (sel ? accent : primary);
    g.fillEllipse (play);
    juce::Path tri;
    tri.addTriangle (play.getX() + 6, play.getY() + 4.5f, play.getX() + 6, play.getBottom() - 4.5f, play.getRight() - 4, play.getCentreY());
    g.setColour (onPrimary);
    g.fillPath (tri);

    auto starArea = r.removeFromRight (28);
    const bool fav = main.isFavourite (s->id);
    drawStar (g, starArea.toFloat().withSizeKeepingCentre (15, 15), fav, fav ? star : faint);

    auto waveArea = r.removeFromRight (130).reduced (4, 6).toFloat();
    g.setColour (sel ? accent : primary.withAlpha (0.75f));
    const auto& p = s->peaks;
    const int bars = 40;
    for (int i = 0; i < bars; ++i)
    {
        const float v = p.empty() ? 0.0f : p[(size_t) (i * (int) p.size() / bars)];
        const float bh = juce::jmax (1.0f, v * waveArea.getHeight());
        g.fillRect (waveArea.getX() + i * waveArea.getWidth() / bars, waveArea.getCentreY() - bh * 0.5f,
                    juce::jmax (1.0f, waveArea.getWidth() / bars - 1.0f), bh);
    }

    auto tag = r.removeFromRight (84);
    g.setColour (faint);
    g.setFont (font (10.5f));
    g.drawText (categoryName (s->category).toUpperCase(), tag, juce::Justification::centredRight);

    g.setColour (fits ? text : text.withAlpha (0.45f));
    g.setFont (font (13.0f, sel));
    g.drawText (s->name, r.reduced (4, 0), juce::Justification::centredLeft);
}

void LibraryPanel::listBoxItemClicked (int row, const juce::MouseEvent& e)
{
    if (row < 0 || row >= (int) shown.size())
        return;
    const auto s = shown[(size_t) row];
    if (e.x > list.getWidth() - 40)
    {
        main.toggleFavourite (s->id);
        showSample (s);
        return;
    }
    showSample (s);
    main.processor.preview (s);
}

void LibraryPanel::listBoxItemDoubleClicked (int row, const juce::MouseEvent&)
{
    if (row >= 0 && row < (int) shown.size())
        main.assignToPad (main.selectedPad, shown[(size_t) row]);
}

juce::var LibraryPanel::getDragSourceDescription (const juce::SparseSet<int>& rows)
{
    if (rows.size() > 0 && rows[0] < (int) shown.size())
        return "kerv-sample:" + shown[(size_t) rows[0]]->id;
    return {};
}

// ===========================================================================
ControlsPanel::ControlsPanel (MainView& m) : main (m)
{
    prevSound.setTooltip ("Previous sound in this pad's category");
    nextSound.setTooltip ("Next sound in this pad's category");
    clearPad.setTooltip ("Remove the sound from this pad");
    resetButton.setTooltip ("Reset Pitch, Attack, Decay, Release, Reverb, Delay and Filter");

    prevSound.onClick = [this] { main.stepPadSound (main.selectedPad, -1); };
    nextSound.onClick = [this] { main.stepPadSound (main.selectedPad, +1); };
    clearPad.onClick = [this] { main.processor.assignSample (main.selectedPad, nullptr); main.kitChanged(); };
    resetButton.onClick = [this] { main.processor.resetSoundControls (main.selectedPad); };

    for (auto* b : { &prevSound, &nextSound, &clearPad, &resetButton })
        addAndMakeVisible (b);
    padChanged();
}

void ControlsPanel::padChanged()
{
    knobs.clear();
    const int p = main.selectedPad;
    const std::pair<const char*, const char*> list[] = { { "pitch", "Pitch" }, { "attack", "Attack" }, { "decay", "Decay" },
        { "release", "Release" }, { "rev", "Reverb" }, { "dly", "Delay" }, { "filter", "Filter" } };
    for (const auto& [id, name] : list)
    {
        knobs.push_back (std::make_unique<Knob> (main.processor.apvts, KervProcessor::pid (p, id), name));
        addAndMakeVisible (*knobs.back());
    }
    resized();
    repaint();
}

void ControlsPanel::paint (juce::Graphics& g)
{
    drawPanel (g, getLocalBounds().toFloat(), "SOUND CONTROLS");

    const int p = main.selectedPad;
    auto r = getLocalBounds().reduced (14, 10);
    r.removeFromTop (18);
    auto head = r.removeFromTop (44);

    auto badge = head.removeFromLeft (44).toFloat().reduced (2);
    g.setGradientFill (juce::ColourGradient (primary, badge.getX(), badge.getY(), accent, badge.getRight(), badge.getBottom(), false));
    g.fillRoundedRectangle (badge, 8.0f);
    g.setColour (onPrimary);
    g.setFont (font (18.0f, true));
    g.drawText (juce::String (p + 1), badge, juce::Justification::centred);

    head.removeFromLeft (10);
    const auto s = main.processor.getPadSample (p);
    g.setColour (text);
    g.setFont (font (15.0f, true));
    g.drawText (s != nullptr ? s->name : juce::String ("(empty pad)"), head.removeFromTop (22), juce::Justification::centredLeft);
    g.setColour (dim);
    g.setFont (font (11.5f));
    g.drawText ("PAD " + juce::String (p + 1) + "  -  " + categoryName (main.processor.getPadCategory (p)).toUpperCase()
                + "  -  MIDI NOTE " + juce::String (firstPadNote + p), head, juce::Justification::centredLeft);

    // section label between the two rows of knobs
    g.setColour (accent);
    g.setFont (font (11.0f, true));
    g.drawText ("SOUND EFFECTS", 14, effectsY, 200, 14, juce::Justification::left);
    g.setColour (line);
    g.drawHorizontalLine (effectsY + 7, 120.0f, (float) getWidth() - 14.0f);
}

void ControlsPanel::resized()
{
    auto r = getLocalBounds().reduced (12, 10);
    r.removeFromTop (18 + 44 + 4);

    auto buttons = r.removeFromBottom (28);
    prevSound.setBounds (buttons.removeFromLeft (30));
    buttons.removeFromLeft (4);
    nextSound.setBounds (buttons.removeFromLeft (30));
    resetButton.setBounds (buttons.removeFromRight (74));
    buttons.removeFromRight (6);
    clearPad.setBounds (buttons.removeFromRight (74));
    r.removeFromBottom (6);

    const int cw = r.getWidth() / 4;
    const int rowH = (r.getHeight() - 20) / 2;
    auto row1 = r.removeFromTop (rowH);
    effectsY = r.getY() + 2;
    r.removeFromTop (20);
    auto row2 = r;
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        if (i < 4) knobs[i]->setBounds (row1.getX() + (int) i * cw, row1.getY(), cw, rowH);
        else       knobs[i]->setBounds (row2.getX() + (int) (i - 4) * cw, row2.getY(), cw, rowH);
    }
}

// ===========================================================================
MainView::MainView (KervProcessor& p, std::function<void (float)> scaleCallback)
    : processor (p), onScale (std::move (scaleCallback)),
      master (p.apvts, "master", "Master", false),
      categoryList (*this), library (*this), controls (*this), kitView (*this),
      padsPanel (*this), seqPanel (*this), mixPanel (*this), layerPanel (*this)
{
    juce::PropertiesFile::Options o;
    o.applicationName = "KERV";
    o.folderName = "KERV";
    o.filenameSuffix = ".settings";
    o.osxLibrarySubFolder = "Application Support";
    o.storageFormat = juce::PropertiesFile::storeAsXML;
    settings = std::make_unique<juce::PropertiesFile> (o);
    Theme::apply (settings->getIntValue ("colour", 0));

    for (auto& f : juce::StringArray::fromLines (settings->getValue ("favorites")))
        if (f.isNotEmpty()) favourites.insert (f);

    factoryPresets = Presets::factory();
    buildPresetMenu();
    presetBox.onChange = [this] { loadPresetIndex (presetBox.getSelectedId() - 1); };
    prevPreset.onClick = [this] { loadPresetIndex (juce::jmax (0, presetBox.getSelectedId() - 2)); };
    nextPreset.onClick = [this] { loadPresetIndex (juce::jmin (presetBox.getNumItems() - 1, presetBox.getSelectedId())); };
    saveButton.onClick = [this] { savePreset(); };
    newButton.onClick = [this] { newKit(); };
    randomButton.onClick = [this] { processor.randomizeKit(); kitChanged(); };
    settingsButton.onClick = [this] { showSettings(); };

    saveButton.setTooltip ("Kit Maker: save your kit");
    newButton.setTooltip ("Kit Maker: start an empty kit, then fill each pad from its category");
    randomButton.setTooltip ("Pick a random sound for every pad, inside its category");

    for (auto* c : std::initializer_list<juce::Component*> { &presetBox, &prevPreset, &nextPreset, &saveButton, &newButton,
                                                             &randomButton, &settingsButton, &master, &categoryList, &library,
                                                             &controls, &homeButton, &editorButton, &kitView, &padsTab, &seqTab,
                                                             &mixTab, &layerTab, &padsPanel, &seqPanel, &mixPanel, &layerPanel })
        addAndMakeVisible (c);

    int tabIndex = 0;
    for (auto* t : { &padsTab, &seqTab, &mixTab, &layerTab })
    {
        t->setClickingTogglesState (true);
        t->setRadioGroupId (77);
        t->onClick = [this, tabIndex] { showTab (tabIndex); };
        ++tabIndex;
    }
    for (auto* t : { &homeButton, &editorButton })
    {
        t->setClickingTogglesState (true);
        t->setRadioGroupId (78);
    }
    homeButton.setTooltip ("HOME: your kit, drawn like a real drum set");
    editorButton.setTooltip ("EDITOR: library, pads, sequencer, mix and layers");
    homeButton.onClick = [this] { setHome (true); };
    editorButton.onClick = [this] { setHome (false); };

    padsTab.setToggleState (true, juce::dontSendNotification);
    showTab (0);
    setHome (true); // KERV always opens on HOME
    selectPad (0);

    setSize (baseW, baseH);
}

MainView::~MainView()
{
    settings->saveIfNeeded();
}

void MainView::buildPresetMenu()
{
    presetBox.clear (juce::dontSendNotification);
    userPresetFiles = Presets::userPresets();
    int id = 1;
    presetBox.addSectionHeading ("KERV KITS");
    for (const auto& f : factoryPresets)
        presetBox.addItem (f->getStringAttribute ("name"), id++);
    if (! userPresetFiles.isEmpty())
    {
        presetBox.addSeparator();
        presetBox.addSectionHeading ("MY KITS");
        for (const auto& f : userPresetFiles)
            presetBox.addItem (f.getFileNameWithoutExtension(), id++);
    }

    for (int i = 0; i < presetBox.getNumItems(); ++i)
        if (presetBox.getItemText (i) == processor.getPresetName())
            presetBox.setSelectedItemIndex (i, juce::dontSendNotification);
    if (presetBox.getSelectedId() == 0)
        presetBox.setText (processor.getPresetName(), juce::dontSendNotification);
    shownPresetName = processor.getPresetName();
}

void MainView::loadPresetIndex (int index)
{
    const int numFactory = (int) factoryPresets.size();
    if (index < 0)
        return;
    if (index < numFactory)
        processor.applyPresetXml (*factoryPresets[(size_t) index]);
    else if (index - numFactory < userPresetFiles.size())
        if (auto xml = juce::XmlDocument::parse (userPresetFiles[index - numFactory]))
            if (xml->hasTagName ("KERVPRESET"))
                processor.applyPresetXml (*xml);
    presetBox.setSelectedId (index + 1, juce::dontSendNotification);
    kitChanged();
}

void MainView::savePreset()
{
    auto* w = new juce::AlertWindow ("KIT MAKER", "Name your kit:", juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", processor.getPresetName() == "New Kit" ? juce::String ("My Kompa Kit") : processor.getPresetName());
    w->addButton ("SAVE", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("CANCEL", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w] (int result)
    {
        const auto name = juce::File::createLegalFileName (w->getTextEditorContents ("name").trim());
        if (result == 1 && name.isNotEmpty())
        {
            auto xml = processor.createPresetXml (name);
            xml->writeTo (Presets::userFolder().getChildFile (name + Presets::fileExtension));
            processor.applyPresetXml (*xml, false); // takes the new name
            buildPresetMenu();
        }
    }), true);
}

void MainView::newKit()
{
    juce::XmlElement empty ("KERVPRESET");
    empty.setAttribute ("name", "New Kit");
    for (int p = 0; p < numPads; ++p)
    {
        auto* e = empty.createNewChildElement ("PAD");
        e->setAttribute ("i", p);
        e->setAttribute ("category", processor.getPadCategory (p));
    }
    processor.applyPresetXml (empty);
    kitChanged();
}

void MainView::applyColours (int palette)
{
    Theme::apply (palette);
    settings->setValue ("colour", palette);
    settings->saveIfNeeded();
    if (auto* ed = findParentComponentOfClass<KervEditor>())
    {
        ed->getKervLookAndFeel().refreshColours();
        ed->sendLookAndFeelChange();
    }
    library.search.setTextToShowWhenEmpty ("Search sounds...", faint);
    library.search.applyColourToAllText (text);
    repaint();
}

void MainView::showSettings()
{
    juce::PopupMenu menu, sizeMenu, colourMenu;
    const float current = (float) settings->getDoubleValue ("uiScale", 0.9);
    const float sizes[] = { 0.75f, 0.9f, 1.0f, 1.15f, 1.3f };
    for (int i = 0; i < 5; ++i)
        sizeMenu.addItem (100 + i, juce::String (juce::roundToInt (sizes[i] * 100)) + " %", true, std::abs (sizes[i] - current) < 0.01f);

    const int colour = settings->getIntValue ("colour", 0);
    for (int i = 0; i < Theme::numPalettes; ++i)
        colourMenu.addItem (200 + i, Theme::palettes[i].name, true, i == colour);

    menu.setLookAndFeel (&getLookAndFeel());
    menu.addSectionHeader ("KERV - Kompa Drum Engine");
    menu.addSubMenu ("Interface size", sizeMenu);
    menu.addSubMenu ("Interface colour", colourMenu);
    menu.addSeparator();
    menu.addItem (1, "About KERV");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&settingsButton), [this, sizes] (int r)
    {
        if (r >= 100 && r < 105)
        {
            settings->setValue ("uiScale", sizes[r - 100]);
            settings->saveIfNeeded();
            if (onScale) onScale (sizes[r - 100]);
        }
        else if (r >= 200 && r < 200 + Theme::numPalettes)
            applyColours (r - 200);
        else if (r == 1)
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::NoIcon, "KERV",
                "KERV - Kompa Drum Engine v1.0\nby kimyondabeat\n\n16 pads  -  12 drum categories  -  16-step Kompa sequencer\n"
                "Pads 1 to 16 = MIDI notes 36 to 51.\nThe sequencer follows your DAW's tempo and play button.");
    });
}

void MainView::showTab (int tab)
{
    currentTab = tab;
    padsPanel.setVisible (! homeMode && tab == 0);
    seqPanel.setVisible (! homeMode && tab == 1);
    mixPanel.setVisible (! homeMode && tab == 2);
    layerPanel.setVisible (! homeMode && tab == 3);
}

void MainView::setHome (bool home)
{
    homeMode = home;
    homeButton.setToggleState (home, juce::dontSendNotification);
    editorButton.setToggleState (! home, juce::dontSendNotification);
    kitView.setVisible (home);
    for (auto* c : std::initializer_list<juce::Component*> { &categoryList, &library, &controls, &padsTab, &seqTab, &mixTab, &layerTab })
        c->setVisible (! home);
    showTab (currentTab);
    repaint();
}

void MainView::openPadInLibrary (int pad)
{
    selectPad (pad);
    setHome (false);
}

void MainView::selectPad (int pad)
{
    selectedPad = juce::jlimit (0, numPads - 1, pad);
    // each pad lives in its own category: the library follows the selected pad
    category = processor.getPadCategory (selectedPad);
    searchText = {};
    library.clearSearch();
    categoryList.repaint();
    library.refreshList();
    controls.padChanged();
    mixPanel.padChanged();
    layerPanel.padChanged();
    library.showSample (processor.getPadSample (selectedPad));
    padsPanel.repaint();
    seqPanel.repaint();
    mixPanel.repaint();
    kitView.repaint();
}

void MainView::setCategory (int c)
{
    category = c;
    categoryList.repaint();
    library.refreshList();
}

void MainView::setSearch (const juce::String& s)
{
    searchText = s;
    library.refreshList();
}

void MainView::toggleFavourite (const juce::String& id)
{
    if (! favourites.erase (id))
        favourites.insert (id);

    juce::StringArray arr;
    for (const auto& f : favourites) arr.add (f);
    settings->setValue ("favorites", arr.joinIntoString ("\n"));
    settings->saveIfNeeded();

    categoryList.repaint();
    if (category == CategoryList::favourites) library.refreshList();
    else library.repaint();
}

bool MainView::assignToPad (int pad, SamplePtr s)
{
    if (! processor.assignSample (pad, s))
    {
        library.flashMessage ("Pad " + juce::String (pad + 1) + " only takes " + categoryName (processor.getPadCategory (pad)) + " sounds");
        return false;
    }
    if (s != nullptr)
        processor.triggerPad (pad, 0.9f);
    if (pad != selectedPad) selectPad (pad);
    kitChanged();
    return true;
}

void MainView::stepPadSound (int pad, int delta)
{
    auto options = Library::get().inCategory (processor.getPadCategory (pad));
    if (options.empty())
        return;
    const auto current = processor.getPadSample (pad);
    auto it = std::find (options.begin(), options.end(), current);
    const int idx = it == options.end() ? 0
                                        : ((int) (it - options.begin()) + delta + (int) options.size()) % (int) options.size();
    processor.assignSample (pad, options[(size_t) idx]);
    processor.triggerPad (pad, 0.9f);
    kitChanged();
}

void MainView::kitChanged()
{
    if (processor.getPresetName() != shownPresetName)
        buildPresetMenu();
    controls.padChanged();
    layerPanel.padChanged();
    library.showSample (processor.getPadSample (selectedPad));
    library.repaint();
    padsPanel.repaint();
    seqPanel.repaint();
    mixPanel.repaint();
    kitView.repaint();
}

void MainView::tick()
{
    if (kitView.isVisible())
        kitView.refresh();

    bool hit = false;
    for (int p = 0; p < numPads; ++p)
    {
        const auto c = processor.getHitCount (p);
        if (c != trigSeen[(size_t) p]) { trigSeen[(size_t) p] = c; hit = true; }
    }
    const float before = trigLevel;
    trigLevel = hit ? 1.0f : juce::jmax (0.0f, trigLevel - 0.12f);
    if (std::abs (trigLevel - before) > 1.0e-6f)
        repaint (trigBounds);

    padsPanel.refresh();
    if (seqPanel.isVisible())
        seqPanel.refresh();
    if (library.message.isNotEmpty() && juce::Time::getMillisecondCounter() - library.messageTime > 2500)
    {
        library.message = {};
        library.repaint();
    }
    if (processor.getPresetName() != shownPresetName)
        buildPresetMenu();
}

void MainView::paint (juce::Graphics& g)
{
    g.fillAll (bg);

    auto head = getLocalBounds().removeFromTop (70).reduced (8, 8);
    drawPanel (g, head.toFloat());

    auto logo = head.reduced (18, 0).removeFromLeft (260);
    g.setGradientFill (juce::ColourGradient (accent, (float) logo.getX(), (float) logo.getY(),
                                             primary, (float) logo.getX() + 120.0f, (float) logo.getBottom(), false));
    g.setFont (font (38.0f, true));
    g.drawText ("KERV", logo.removeFromLeft (112), juce::Justification::centredLeft);
    auto sub = logo.reduced (0, 12);
    g.setColour (text);
    g.setFont (font (12.0f, true));
    g.drawText ("KOMPA DRUM ENGINE", sub.removeFromTop (sub.getHeight() / 2), juce::Justification::bottomLeft);
    g.setColour (dim);
    g.setFont (font (11.0f));
    g.drawText ("by kimyondabeat", sub, juce::Justification::topLeft);

    g.setColour (faint);
    g.setFont (font (10.0f, true));
    g.drawText ("KIT", 296, 14, 80, 12, juce::Justification::left);

    // TRIG light: flashes on every hit (MIDI, sequencer or click)
    const auto dot = juce::Rectangle<float> (12.0f, 12.0f).withCentre ({ (float) trigBounds.getCentreX(), trigBounds.getY() + 12.0f });
    if (trigLevel > 0.01f)
    {
        g.setColour (primary.withAlpha (0.35f * trigLevel));
        g.fillEllipse (dot.expanded (5.0f * trigLevel));
    }
    g.setColour (line.interpolatedWith (primary, trigLevel));
    g.fillEllipse (dot);
    g.setColour (faint);
    g.setFont (font (9.5f, true));
    g.drawText ("TRIG", trigBounds.withTrimmedTop (22), juce::Justification::centredTop);
}

void MainView::resized()
{
    auto head = getLocalBounds().removeFromTop (70).reduced (8, 8).reduced (12, 0);
    head.removeFromLeft (276);
    auto row = head.withSizeKeepingCentre (head.getWidth(), 32).translated (0, 6);
    presetBox.setBounds (row.removeFromLeft (200));
    row.removeFromLeft (6);
    prevPreset.setBounds (row.removeFromLeft (32));
    row.removeFromLeft (4);
    nextPreset.setBounds (row.removeFromLeft (32));
    row.removeFromLeft (12);
    newButton.setBounds (row.removeFromLeft (58));
    row.removeFromLeft (6);
    saveButton.setBounds (row.removeFromLeft (62));
    row.removeFromLeft (6);
    randomButton.setBounds (row.removeFromLeft (96));
    row.removeFromLeft (14);
    homeButton.setBounds (row.removeFromLeft (66));
    row.removeFromLeft (4);
    editorButton.setBounds (row.removeFromLeft (72));

    auto right = getLocalBounds().removeFromTop (70).reduced (8, 8).reduced (10, 0);
    master.setBounds (right.removeFromRight (64).reduced (0, 2));
    right.removeFromRight (10);
    settingsButton.setBounds (right.removeFromRight (84).withSizeKeepingCentre (84, 32));
    right.removeFromRight (10);
    trigBounds = right.removeFromRight (30).withTrimmedTop (8).withHeight (40);

    auto mid = juce::Rectangle<int> (8, 78, baseW - 16, 398);
    categoryList.setBounds (mid.removeFromLeft (184));
    mid.removeFromLeft (8);
    controls.setBounds (mid.removeFromRight (370));
    mid.removeFromRight (8);
    library.setBounds (mid);
    kitView.setBounds (8, 78, baseW - 16, baseH - 86);

    auto bottom = juce::Rectangle<int> (8, 484, baseW - 16, baseH - 492);
    auto tabs = bottom.removeFromTop (28);
    for (auto* t : { &padsTab, &seqTab, &mixTab, &layerTab })
    {
        t->setBounds (tabs.removeFromLeft (120));
        tabs.removeFromLeft (6);
    }
    bottom.removeFromTop (6);
    padsPanel.setBounds (bottom);
    seqPanel.setBounds (bottom);
    mixPanel.setBounds (bottom);
    layerPanel.setBounds (bottom);
}

// ===========================================================================
KervEditor::KervEditor (KervProcessor& p)
    : AudioProcessorEditor (&p),
      view (p, [this] (float s) { setScale (s); })
{
    lnf.refreshColours(); // the view loaded the saved colour palette
    setLookAndFeel (&lnf);
    addAndMakeVisible (view);
    scale = (float) view.settings->getDoubleValue ("uiScale", 0.9);
    setScale (scale);
    startTimerHz (30);
}

KervEditor::~KervEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void KervEditor::setScale (float s)
{
    scale = juce::jlimit (0.5f, 2.0f, s);
    view.setTransform (juce::AffineTransform::scale (scale));
    setSize (juce::roundToInt (MainView::baseW * scale), juce::roundToInt (MainView::baseH * scale));
}

void KervEditor::resized()
{
    view.setBounds (0, 0, MainView::baseW, MainView::baseH);
}
