#include "Panels.h"

#include "Spelling.h"

namespace nt
{

//==============================================================================

GlyphButton::GlyphButton (const juce::String& name, juce::juce_wchar g, float s) : juce::Button (name), glyph (g), scale (s)
{
    setClickingTogglesState (false);
}

void GlyphButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    juce::Colour fill = getToggleState() ? theme::accent : theme::control;
    if (down) fill = getToggleState() ? theme::shade (theme::accent, -0.18f) : theme::controlHeld;
    else if (highlighted) fill = getToggleState() ? theme::shade (theme::accent, 0.18f) : theme::controlHover;
    g.setColour (fill);
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (theme::ink);
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
    auto& gl = Glyphs::get();
    const auto b = gl.bounds (glyph);
    if (b.isEmpty()) return;
    const float sp = std::min ((r.getHeight() - 8.0f) / std::max (1.0f, b.getHeight()), (r.getWidth() - 8.0f) / std::max (1.0f, b.getWidth())) * scale;
    const float x = r.getCentreX() - (b.getX() + b.getWidth() * 0.5f) * sp;
    const float y = r.getCentreY() - (b.getY() + b.getHeight() * 0.5f) * sp;
    gl.draw (g, glyph, x, y, sp);
}

//==============================================================================

namespace
{
struct Duration { const char* name; Tick ticks; juce::juce_wchar glyph; const char* key; };
const Duration durationList[] = {
    { "Whole note", 4 * PPQ, 0xE1D2, "7" }, { "Half note", 2 * PPQ, 0xE1D3, "6" }, { "Quarter note", PPQ, 0xE1D5, "5" },
    { "Eighth note", PPQ / 2, 0xE1D7, "4" }, { "Sixteenth note", PPQ / 4, 0xE1D9, "3" }, { "Thirty-second note", PPQ / 8, 0xE1DB, "2" },
};
} // namespace

Toolbar::Toolbar (Controller& c) : controller (c)
{
    for (auto* b : { &newButton, &openButton, &saveButton, &exportButton, &undoButton, &redoButton, &playButton,
                     &inputButton, &voiceButton, &transposeButton, &pageButton, &zoomOut, &zoomIn, &settingsButton })
        addAndMakeVisible (b);

    newButton.setTooltip ("A new score, from a template");
    openButton.setTooltip ("Open a Noterator project or a MIDI file (Cmd+O)");
    saveButton.setTooltip ("Save the project (Cmd+S)");
    exportButton.setTooltip ("Export the score, or the selected bars, as MIDI or audio");
    undoButton.setTooltip ("Undo (Cmd+Z)");
    redoButton.setTooltip ("Redo (Shift+Cmd+Z)");
    playButton.setTooltip ("Play from the caret or the selection, or stop (Space)");
    inputButton.setTooltip ("Note input (N): click a line or space, or type A-G, or play a MIDI keyboard");
    voiceButton.setTooltip ("Which voice notes are written in: 1 stems up, 2 stems down (V changes the selection's)");
    transposeButton.setTooltip ("Show the score at concert pitch, or as the transposing instruments read it");
    pageButton.setTooltip ("Set the page dark, light ink on dark paper - or back to black on white");
    settingsButton.setTooltip ("The sound, and audio and MIDI devices");

    newButton.onClick = [this] { if (onNew) onNew(); };
    openButton.onClick = [this] { if (onOpen) onOpen(); };
    saveButton.onClick = [this] { if (onSave) onSave(); };
    exportButton.onClick = [this] { if (onExport) onExport(); };
    settingsButton.onClick = [this] { if (onSettings) onSettings(); };
    undoButton.onClick = [this] { controller.undo(); };
    redoButton.onClick = [this] { controller.redo(); };
    playButton.onClick = [this] { controller.togglePlay(); };
    inputButton.onClick = [this] { controller.toggleNoteInput(); };
    voiceButton.onClick = [this] { controller.setVoice (1 - controller.input.voice); };
    transposeButton.onClick = [this] { controller.transposedScore = ! controller.transposedScore; controller.viewChanged(); };
    pageButton.onClick = [this] { controller.lightPage = ! controller.lightPage; controller.viewChanged(); };
    zoomOut.onClick = [this] { controller.zoom = std::max (5.0f, controller.zoom / 1.15f); controller.viewChanged(); };
    zoomIn.onClick = [this] { controller.zoom = std::min (24.0f, controller.zoom * 1.15f); controller.viewChanged(); };

    for (const auto& d : durationList)
    {
        auto b = std::make_unique<GlyphButton> (d.name, d.glyph, 0.95f);
        b->setTooltip (juce::String (d.name) + " (" + d.key + ")");
        const Tick t = d.ticks;
        b->onClick = [this, t] { controller.setDuration (t); };
        addAndMakeVisible (*b);
        durations.push_back (std::move (b));
    }
    for (auto* b : { &dotButton, &tripletButton, &restButton }) addAndMakeVisible (b);
    dotButton.setTooltip ("Dotted (.)");
    tripletButton.setTooltip ("Triplet (T)");
    restButton.setTooltip ("Write a rest at the caret (0)");
    dotButton.onClick = [this] { controller.toggleDot(); };
    tripletButton.onClick = [this] { controller.toggleTriplet(); };
    restButton.onClick = [this] { controller.typeRest(); };

    controller.addChangeListener (this);
    refresh();
}

Toolbar::~Toolbar() { controller.removeChangeListener (this); }

void Toolbar::paint (juce::Graphics& g)
{
    g.fillAll (theme::popup);
    g.setColour (theme::rule);
    g.fillRect (0, getHeight() - 1, getWidth(), 1);
}

void Toolbar::resized()
{
    auto r = getLocalBounds().reduced (8, 7);
    auto place = [&r] (juce::Component& c, int w, int gap = 4) { c.setBounds (r.removeFromLeft (w)); r.removeFromLeft (gap); };
    place (newButton, 52); place (openButton, 56); place (saveButton, 52); place (exportButton, 62, 14);
    place (undoButton, 52); place (redoButton, 52, 14);
    place (playButton, 60, 14);
    place (inputButton, 92, 6);
    for (auto& d : durations) place (*d, 30, 2);
    r.removeFromLeft (4);
    place (dotButton, 30, 2); place (tripletButton, 30, 2); place (restButton, 30, 6);
    place (voiceButton, 66, 14);
    auto right = r;
    settingsButton.setBounds (right.removeFromRight (64));
    right.removeFromRight (10);
    zoomIn.setBounds (right.removeFromRight (28));
    right.removeFromRight (2);
    zoomOut.setBounds (right.removeFromRight (28));
    right.removeFromRight (10);
    pageButton.setBounds (right.removeFromRight (86));
    right.removeFromRight (4);
    transposeButton.setBounds (right.removeFromRight (106));
}

void Toolbar::changeListenerCallback (juce::ChangeBroadcaster*) { refresh(); }

void Toolbar::refresh()
{
    undoButton.setEnabled (controller.canUndo());
    redoButton.setEnabled (controller.canRedo());
    playButton.setButtonText (controller.audio.isPlaying() ? "Stop" : "Play");
    playButton.setToggleState (controller.audio.isPlaying(), juce::dontSendNotification);
    inputButton.setToggleState (controller.input.noteInput, juce::dontSendNotification);
    for (size_t i = 0; i < durations.size(); ++i)
        durations[i]->setToggleState (durationList[i].ticks == controller.input.base, juce::dontSendNotification);
    dotButton.setToggleState (controller.input.dotted, juce::dontSendNotification);
    tripletButton.setToggleState (controller.input.triplet, juce::dontSendNotification);
    voiceButton.setButtonText ("Voice " + juce::String (controller.input.voice + 1));
    transposeButton.setButtonText (controller.transposedScore ? "Transposed" : "Concert pitch");
    pageButton.setToggleState (! controller.lightPage, juce::dontSendNotification);
    repaint();
}

//==============================================================================

struct PartsPanel::Row : public juce::Component
{
    Row (Controller& c, uint32_t id) : controller (c), partId (id)
    {
        for (auto* comp : std::initializer_list<juce::Component*> { &name, &instrument, &mute, &solo, &autoCC, &volume, &up, &down, &remove })
            addAndMakeVisible (comp);
        name.setFont (juce::FontOptions (14.0f));
        name.onReturnKey = name.onFocusLost = [this]
        {
            const auto text = name.getText().trim().toStdString();
            const auto* p = controller.score.partById (partId);
            if (p != nullptr && ! text.empty() && text != p->name)
                controller.edit ("Renamed a part", [this, text] (Score& s) { if (auto* q = s.partById (partId)) q->name = text; });
        };
        int itemId = 1;
        for (const auto& fam : instrumentFamilies())
        {
            instrument.addSectionHeading (fam);
            for (const auto& inst : instruments())
                if (inst.family == fam) { instrument.addItem (inst.name, itemId); ids.push_back (inst.id); ++itemId; }
        }
        instrument.onChange = [this]
        {
            const int i = instrument.getSelectedId() - 1;
            if (i >= 0 && i < static_cast<int> (ids.size())) controller.setPartInstrument (partId, ids[static_cast<size_t> (i)]);
        };
        mute.setButtonText ("M");
        solo.setButtonText ("S");
        autoCC.setButtonText ("AutoCC");
        mute.setTooltip ("Mute");
        solo.setTooltip ("Solo");
        autoCC.setTooltip ("Draw this part's dynamics, expression and vibrato curves automatically (AutoCC)");
        mute.onClick = [this] { toggle ([] (Part& p) { p.mute = ! p.mute; }); };
        solo.onClick = [this] { toggle ([] (Part& p) { p.solo = ! p.solo; }); };
        autoCC.onClick = [this] { toggle ([] (Part& p) { p.autoCC = ! p.autoCC; }); };
        volume.setSliderStyle (juce::Slider::LinearHorizontal);
        volume.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        volume.setRange (0.0, 1.0);
        volume.setTooltip ("Volume");
        volume.onDragEnd = [this]
        {
            const auto v = static_cast<float> (volume.getValue());
            controller.edit ("Changed a volume", [this, v] (Score& s) { if (auto* p = s.partById (partId)) p->volume = v; });
        };
        up.onClick = [this] { controller.movePart (partId, -1); };
        down.onClick = [this] { controller.movePart (partId, 1); };
        remove.onClick = [this]
        {
            const auto* p = controller.score.partById (partId);
            if (p == nullptr) return;
            if (p->notes.empty()) { controller.removePart (partId); return; }
            juce::NativeMessageBox::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Remove " + juce::String (p->name) + "?",
                                                     "Its notes go with it. Undo brings them back.", this,
                                                     juce::ModalCallbackFunction::create ([this] (int r) { if (r != 0) controller.removePart (partId); }));
        };
        up.setTooltip ("Move up");
        down.setTooltip ("Move down");
        remove.setTooltip ("Remove this part");
    }

    void toggle (const std::function<void (Part&)>& fn)
    {
        controller.edit ("Changed a part", [this, fn] (Score& s) { if (auto* p = s.partById (partId)) fn (*p); });
    }

    void refresh()
    {
        const auto* p = controller.score.partById (partId);
        if (p == nullptr) return;
        if (! name.hasKeyboardFocus (true)) name.setText (p->name, juce::dontSendNotification);
        for (size_t i = 0; i < ids.size(); ++i)
            if (ids[i] == p->instrument) instrument.setSelectedId (static_cast<int> (i) + 1, juce::dontSendNotification);
        mute.setToggleState (p->mute, juce::dontSendNotification);
        solo.setToggleState (p->solo, juce::dontSendNotification);
        autoCC.setToggleState (p->autoCC, juce::dontSendNotification);
        autoCC.setEnabled (instrumentById (p->instrument).cc != CCShape::none);
        volume.setValue (p->volume, juce::dontSendNotification);
        current = controller.caretPart == partId;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (current ? theme::frameActive : theme::sunken);
        g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 4.0f);
        if (current)
        {
            g.setColour (theme::accent);
            g.fillRect (1, 4, 3, getHeight() - 8);
        }
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (8, 6);
        auto top = r.removeFromTop (24);
        remove.setBounds (top.removeFromRight (24));
        top.removeFromRight (3);
        down.setBounds (top.removeFromRight (24));
        top.removeFromRight (3);
        up.setBounds (top.removeFromRight (24));
        top.removeFromRight (6);
        name.setBounds (top);
        r.removeFromTop (4);
        auto mid = r.removeFromTop (24);
        instrument.setBounds (mid);
        r.removeFromTop (4);
        auto bottom = r.removeFromTop (24);
        mute.setBounds (bottom.removeFromLeft (28));
        bottom.removeFromLeft (3);
        solo.setBounds (bottom.removeFromLeft (28));
        bottom.removeFromLeft (8);
        autoCC.setBounds (bottom.removeFromLeft (80));
        bottom.removeFromLeft (6);
        volume.setBounds (bottom);
    }

    void mouseDown (const juce::MouseEvent&) override { controller.setCaret (partId, controller.caret); }

    Controller& controller;
    uint32_t partId;
    bool current = false;
    std::vector<std::string> ids;
    juce::TextEditor name;
    juce::ComboBox instrument;
    juce::TextButton mute, solo, up { "^" }, down { "v" }, remove { "x" };
    juce::ToggleButton autoCC;
    juce::Slider volume;
};

PartsPanel::PartsPanel (Controller& c) : controller (c)
{
    addAndMakeVisible (viewport);
    viewport.setViewedComponent (&rows, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (addButton);
    addButton.onClick = [this] { showAddMenu(); };
    addAndMakeVisible (info);
    info.setJustificationType (juce::Justification::topLeft);
    info.setFont (juce::FontOptions (13.0f));
    info.setColour (juce::Label::textColourId, theme::text);
    controller.addChangeListener (this);
    rebuild();
}

PartsPanel::~PartsPanel() { controller.removeChangeListener (this); }

void PartsPanel::paint (juce::Graphics& g) { g.fillAll (theme::ground); }

void PartsPanel::resized()
{
    auto r = getLocalBounds().reduced (8);
    info.setBounds (r.removeFromBottom (150));
    r.removeFromBottom (6);
    addButton.setBounds (r.removeFromBottom (28));
    r.removeFromBottom (8);
    viewport.setBounds (r);
    layoutRows();
}

void PartsPanel::layoutRows()
{
    const int w = viewport.getWidth() - viewport.getScrollBarThickness();
    int y = 0;
    for (auto& row : rowList)
    {
        row->setBounds (0, y, w, 96);
        y += 100;
    }
    rows.setSize (w, y);
}

void PartsPanel::rebuild()
{
    rowList.clear();
    for (const auto& p : controller.score.parts)
    {
        auto row = std::make_unique<Row> (controller, p.id);
        rows.addAndMakeVisible (*row);
        row->refresh();
        rowList.push_back (std::move (row));
    }
    lastCount = controller.score.parts.size();
    layoutRows();
}

void PartsPanel::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // Rows are rebuilt when the parts themselves change, refreshed otherwise.
    bool same = controller.score.parts.size() == rowList.size();
    for (size_t i = 0; same && i < rowList.size(); ++i) same = rowList[i]->partId == controller.score.parts[i].id;
    if (! same) rebuild();
    else for (auto& r : rowList) r->refresh();

    if (const auto* p = controller.caretPartPtr())
    {
        const auto& inst = instrumentById (p->instrument);
        const auto ctx = keyContext (0, 0);
        juce::StringArray lines;
        lines.add (juce::String (inst.name) + "  (" + juce::String (inst.family) + ")");
        lines.add ("Range " + juce::String (pitchName (inst.low, ctx)) + " to " + juce::String (pitchName (inst.high, ctx))
                   + ", at its best " + juce::String (pitchName (inst.sweetLow, ctx)) + " to " + juce::String (pitchName (inst.sweetHigh, ctx)));
        const char* role = inst.role == 'S' ? "soprano" : inst.role == 'A' ? "alto" : inst.role == 'T' ? "tenor" : "bass";
        lines.add (juce::String (inst.monophonic() ? "Plays one line" : "Plays up to " + juce::String (inst.poly) + " notes at once")
                   + ", takes the " + role + " in harmony");
        lines.add ("Moves cleanly every " + juce::String (inst.fast * 1000.0, 0) + " ms, leaps up to " + juce::String (inst.leap) + " semitones"
                   + (inst.breath ? ", needs to breathe" : ""));
        if (inst.transposition != 0) lines.add ("Transposing instrument " + juce::String (inst.transposedName));
        if (inst.octave != 0) lines.add (juce::String ("Written an octave ") + (inst.octave > 0 ? "above" : "below") + " where it sounds");
        const char* cc = inst.cc == CCShape::strings ? "Strings" : inst.cc == CCShape::brass ? "Brass" : inst.cc == CCShape::woodwinds ? "Woodwinds"
                       : inst.cc == CCShape::neutral ? "Default" : "none - it does not swell";
        lines.add ("AutoCC shape: " + juce::String (cc));
        info.setText (lines.joinIntoString ("\n"), juce::dontSendNotification);
    }
}

void PartsPanel::showAddMenu()
{
    juce::PopupMenu menu;
    int itemId = 1;
    std::vector<std::string> ids;
    for (const auto& fam : instrumentFamilies())
    {
        juce::PopupMenu sub;
        for (const auto& inst : instruments())
            if (inst.family == fam) { sub.addItem (itemId++, inst.name); ids.push_back (inst.id); }
        menu.addSubMenu (fam, sub);
    }
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&addButton), [this, ids] (int r)
    {
        if (r > 0 && r <= static_cast<int> (ids.size())) controller.addPart (ids[static_cast<size_t> (r - 1)]);
    });
}

//==============================================================================

ScorePanel::ScorePanel (Controller& c) : controller (c)
{
    for (auto* comp : std::initializer_list<juce::Component*> { &titleLabel, &title, &composerLabel, &composer, &tempoLabel, &tempo,
                                                                 &meterLabel, &meterNum, &meterDen, &meterApply, &keyLabel, &keyRoot,
                                                                 &keyScale, &keyApply, &keyDetected, &barsLabel, &insertBar, &deleteBars,
                                                                 &addBars, &soundLabel, &synthChoice, &audioSettings })
        addAndMakeVisible (comp);
    for (auto* l : { &titleLabel, &composerLabel, &tempoLabel, &meterLabel, &keyLabel, &barsLabel, &soundLabel })
        l->setColour (juce::Label::textColourId, theme::stepNumber);

    title.onReturnKey = title.onFocusLost = [this]
    {
        const auto t = title.getText().toStdString();
        if (t != controller.score.title) controller.edit ("Retitled", [t] (Score& s) { s.title = t; });
    };
    composer.onReturnKey = composer.onFocusLost = [this]
    {
        const auto t = composer.getText().toStdString();
        if (t != controller.score.composer) controller.edit ("Changed the composer", [t] (Score& s) { s.composer = t; });
    };

    tempo.setRange (30.0, 240.0, 1.0);
    tempo.setTextValueSuffix (" bpm");
    tempo.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 22);
    tempo.onDragEnd = [this] { controller.setTempo (tempo.getValue()); };
    tempo.onValueChange = [this] { if (! tempo.isMouseButtonDown()) controller.setTempo (tempo.getValue()); };

    for (int n = 1; n <= 16; ++n) meterNum.addItem (juce::String (n), n);
    for (int d : { 2, 4, 8, 16 }) meterDen.addItem (juce::String (d), d);
    meterApply.onClick = [this]
    {
        controller.setMeterAt (controller.score.barAt (controller.caret), meterNum.getSelectedId(), meterDen.getSelectedId());
    };

    for (size_t i = 0; i < scaleview::roots.size(); ++i) keyRoot.addItem (scaleview::roots[i].name, static_cast<int> (i) + 1);
    for (size_t i = 0; i < scaleview::scales.size(); ++i) keyScale.addItem (scaleview::scales[i].name, static_cast<int> (i) + 1);
    keyApply.onClick = [this]
    {
        controller.setKeyAt (controller.score.barAt (controller.caret), keyRoot.getSelectedId() - 1, keyScale.getSelectedId() - 1);
    };
    keyDetected.setTooltip ("Sets the key signature to the scale shown in the Scale lane at the caret");
    keyDetected.onClick = [this]
    {
        for (const auto& k : controller.keys)
            if (controller.caret >= k.start && controller.caret < k.end)
            {
                controller.setKeyAt (controller.score.barAt (k.start), k.root, k.scale);
                return;
            }
        controller.setStatus ("Nothing to hear yet: write or generate some music first.");
    };

    insertBar.onClick = [this] { controller.insertBarsAtCaret (1); };
    deleteBars.onClick = [this] { controller.deleteSelectedBars(); };
    addBars.onClick = [this] { controller.setBars (controller.score.bars + 4); };

    synthChoice.addItem ("Apple General MIDI (macOS)", 1);
    synthChoice.addItem ("Built-in synth", 2);
    synthChoice.onChange = [this]
    {
        controller.audio.useBuiltInSynth (synthChoice.getSelectedId() == 2);
        controller.setStatus ("Sound: " + controller.audio.synthName());
    };
    audioSettings.onClick = [this] { if (onAudioSettings) onAudioSettings(); };

    controller.addChangeListener (this);
    refresh();
}

ScorePanel::~ScorePanel() { controller.removeChangeListener (this); }

void ScorePanel::paint (juce::Graphics& g) { g.fillAll (theme::ground); }

void ScorePanel::resized()
{
    auto r = getLocalBounds().reduced (10);
    auto row = [&r] (int h = 26) { auto x = r.removeFromTop (h); r.removeFromTop (6); return x; };
    titleLabel.setBounds (row (18)); title.setBounds (row());
    composerLabel.setBounds (row (18)); composer.setBounds (row());
    r.removeFromTop (6);
    tempoLabel.setBounds (row (18)); tempo.setBounds (row());
    r.removeFromTop (6);
    meterLabel.setBounds (row (18));
    {
        auto x = row();
        meterNum.setBounds (x.removeFromLeft (60)); x.removeFromLeft (4);
        meterDen.setBounds (x.removeFromLeft (60)); x.removeFromLeft (8);
        meterApply.setBounds (x);
    }
    r.removeFromTop (6);
    keyLabel.setBounds (row (18));
    {
        auto x = row();
        keyRoot.setBounds (x.removeFromLeft (64)); x.removeFromLeft (4);
        keyScale.setBounds (x);
    }
    {
        auto x = row();
        keyApply.setBounds (x.removeFromLeft (x.getWidth() / 2 - 2)); x.removeFromLeft (4);
        keyDetected.setBounds (x);
    }
    r.removeFromTop (6);
    barsLabel.setBounds (row (18));
    insertBar.setBounds (row());
    deleteBars.setBounds (row());
    addBars.setBounds (row());
    r.removeFromTop (6);
    soundLabel.setBounds (row (18));
    synthChoice.setBounds (row());
    audioSettings.setBounds (row());
}

void ScorePanel::changeListenerCallback (juce::ChangeBroadcaster*) { refresh(); }

void ScorePanel::refresh()
{
    if (! title.hasKeyboardFocus (true)) title.setText (controller.score.title, false);
    if (! composer.hasKeyboardFocus (true)) composer.setText (controller.score.composer, false);
    if (! tempo.isMouseButtonDown()) tempo.setValue (controller.score.tempos.front().bpm, juce::dontSendNotification);
    const int bar = controller.score.barAt (controller.caret);
    const auto& m = controller.score.meterAtBar (bar);
    meterNum.setSelectedId (m.num, juce::dontSendNotification);
    meterDen.setSelectedId (m.den, juce::dontSendNotification);
    const auto& k = controller.score.keyAtBar (bar);
    keyRoot.setSelectedId (k.root + 1, juce::dontSendNotification);
    keyScale.setSelectedId (k.scale + 1, juce::dontSendNotification);
    barsLabel.setText ("Bars - " + juce::String (controller.score.bars) + " in all, caret in bar " + juce::String (bar + 1), juce::dontSendNotification);
    synthChoice.setSelectedId (controller.audio.usingBuiltInSynth() ? 2 : 1, juce::dontSendNotification);
}

//==============================================================================

StatusBar::StatusBar (Controller& c) : controller (c)
{
    controller.addChangeListener (this);
    startTimerHz (2);
}

StatusBar::~StatusBar() { controller.removeChangeListener (this); }

void StatusBar::paint (juce::Graphics& g)
{
    g.fillAll (theme::popup);
    g.setColour (theme::rule);
    g.fillRect (0, 0, getWidth(), 1);
    auto r = getLocalBounds().reduced (10, 0);
    g.setFont (juce::FontOptions (13.0f));

    // Right: what is making the sound and what is listening.
    const int midiIns = juce::MidiInput::getAvailableDevices().size();
    const juce::String right = controller.audio.synthName() + "   |   "
                             + (midiIns == 0 ? juce::String ("no MIDI keyboard") : juce::String (midiIns) + " MIDI input" + (midiIns > 1 ? "s" : ""));
    g.setColour (theme::textDim);
    g.drawText (right, r, juce::Justification::centredRight);

    // Middle: where the caret is and what is selected.
    juce::String where;
    if (const auto* p = controller.caretPartPtr())
    {
        const auto& s = controller.score;
        const int bar = s.barAt (controller.caret);
        const Tick inBar = controller.caret - s.barStart (bar);
        const auto beat = s.meterAtBar (bar).beatTicks();
        where = juce::String (p->name) + ", bar " + juce::String (bar + 1) + " beat " + juce::String (1.0 + static_cast<double> (inBar) / static_cast<double> (beat), 2);
        if (! controller.selection.empty()) where += "   |   " + juce::String (static_cast<int> (controller.selection.size())) + " selected";
    }
    g.setColour (theme::text);
    g.drawText (where, r.withTrimmedRight (320), juce::Justification::centred);

    // Left: the last thing that happened, or the mode.
    juce::String left = controller.status;
    if (controller.input.noteInput) left = "NOTE INPUT - click the staff, type A-G (Shift adds to the chord), or play a MIDI keyboard. Esc to stop.";
    g.setColour (controller.input.noteInput ? theme::accent : theme::text);
    g.drawText (left, r, juce::Justification::centredLeft);
}

} // namespace nt
