#include "Panels.h"

#include "Spelling.h"

namespace nt
{

//==============================================================================

void TransportButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    getLookAndFeel().drawButtonBackground (g, *this, findColour (juce::TextButton::buttonColourId), highlighted, down);
    // A bar and a triangle pointing at it: back to the start, or on to the end.
    const auto r = getLocalBounds().toFloat().withSizeKeepingCentre (14.0f, 12.0f);
    juce::Path p;
    if (kind == Kind::start)
    {
        p.addRectangle (r.getX(), r.getY(), 2.5f, r.getHeight());
        p.addTriangle (r.getRight(), r.getY(), r.getRight(), r.getBottom(), r.getX() + 3.5f, r.getCentreY());
    }
    else
    {
        p.addRectangle (r.getRight() - 2.5f, r.getY(), 2.5f, r.getHeight());
        p.addTriangle (r.getX(), r.getY(), r.getX(), r.getBottom(), r.getRight() - 3.5f, r.getCentreY());
    }
    g.setColour (isEnabled() ? theme::ink : theme::ink.withAlpha (0.4f));
    g.fillPath (p);
}

//==============================================================================

Toolbar::Toolbar (Controller& c) : controller (c)
{
    for (auto* b : std::initializer_list<juce::Button*> { &startButton, &endButton }) addAndMakeVisible (b);
    for (auto* b : { &fileButton, &playButton, &followButton,
                     &selectButton, &drawButton, &tripletButton, &snapButton, &quantiseButton, &zoomOut, &zoomIn })
        addAndMakeVisible (b);
    addAndMakeVisible (gridLabel);
    addAndMakeVisible (gridBox);
    gridLabel.setColour (juce::Label::textColourId, theme::textDim);
    gridLabel.setJustificationType (juce::Justification::centredRight);

    fileButton.setTooltip ("New, Open, Save, Export, Undo and Redo, the song's settings, the sound, step input and the light look");
    playButton.setTooltip ("Play from the caret, or stop (Shift+Space; Space plays from bar 1)");
    startButton.setTooltip ("Return to the start (Home) - if it is playing, it plays on from bar 1");
    endButton.setTooltip ("Skip to the end of the music (End)");
    followButton.setTooltip ("Follow (F): while it plays, the view scrolls along with the music (or turns a page at a time - Play menu)");
    selectButton.setTooltip ("Select (D switches): click a note to choose it, drag it to move it, drag its end to stretch it, double-click to draw one");
    drawButton.setTooltip ("Draw (D switches): click the roll to draw a note, drag to make it longer, click a note to delete it");
    gridBox.setTooltip ("The grid notes snap to, and how long a drawn note is (keys 1-6)");
    tripletButton.setTooltip ("A triplet grid: three in the space of two (T)");
    snapButton.setTooltip ("Notes snap to the grid when they are drawn, moved or stretched");
    quantiseButton.setTooltip ("Pull the selected notes onto the grid - or the whole part in the roll if none are selected (Q)");
    zoomIn.setTooltip ("Zoom in (Cmd+=)");
    zoomOut.setTooltip ("Zoom out (Cmd+-)");

    int id = 1;
    for (const auto t : roll::gridValues())
    {
        roll::Grid g;
        g.base = t;
        gridBox.addItem (g.name(), id++);
    }

    fileButton.onClick = [this] { if (onFile) onFile(); };
    playButton.onClick = [this] { controller.togglePlay(); };
    startButton.onClick = [this] { if (onStart) onStart(); };
    endButton.onClick = [this] { if (onEnd) onEnd(); };
    followButton.onClick = [this] { controller.toggleFollow(); };
    selectButton.onClick = [this] { if (controller.drawTool) controller.toggleDrawTool(); };
    drawButton.onClick = [this] { if (! controller.drawTool) controller.toggleDrawTool(); };
    gridBox.onChange = [this]
    {
        const int i = gridBox.getSelectedId() - 1;
        const auto& values = roll::gridValues();
        if (i >= 0 && i < static_cast<int> (values.size()) && values[static_cast<size_t> (i)] != controller.grid.base)
            controller.setGrid (values[static_cast<size_t> (i)], controller.grid.triplet);
    };
    tripletButton.onClick = [this] { controller.setGrid (controller.grid.base, ! controller.grid.triplet); };
    snapButton.onClick = [this] { controller.toggleSnap(); };
    quantiseButton.onClick = [this] { controller.quantiseSelection(); };
    zoomOut.onClick = [this] { if (onZoomOut) onZoomOut(); };
    zoomIn.onClick = [this] { if (onZoomIn) onZoomIn(); };

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
    place (fileButton, 52, 14);
    place (startButton, 30, 2);
    place (playButton, 60, 2);
    place (endButton, 30, 6);
    place (followButton, 60, 14);
    place (selectButton, 60, 2); place (drawButton, 56, 14);
    place (gridLabel, 36, 4);
    place (gridBox, 74, 4);
    place (tripletButton, 62, 2); place (snapButton, 52, 8);
    place (quantiseButton, 74, 14);
    auto right = r;
    zoomIn.setBounds (right.removeFromRight (28));
    right.removeFromRight (2);
    zoomOut.setBounds (right.removeFromRight (28));
}

void Toolbar::changeListenerCallback (juce::ChangeBroadcaster*) { refresh(); }

void Toolbar::refresh()
{
    playButton.setButtonText (controller.audio.isPlaying() ? "Stop" : "Play");
    playButton.setToggleState (controller.audio.isPlaying(), juce::dontSendNotification);
    followButton.setToggleState (controller.followPlayback, juce::dontSendNotification);
    selectButton.setToggleState (! controller.drawTool, juce::dontSendNotification);
    drawButton.setToggleState (controller.drawTool, juce::dontSendNotification);
    const auto& values = roll::gridValues();
    for (size_t i = 0; i < values.size(); ++i)
        if (values[i] == controller.grid.base) gridBox.setSelectedId (static_cast<int> (i) + 1, juce::dontSendNotification);
    tripletButton.setToggleState (controller.grid.triplet, juce::dontSendNotification);
    snapButton.setToggleState (controller.grid.snap, juce::dontSendNotification);
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

    void mouseDown (const juce::MouseEvent&) override { controller.choosePart (partId); }

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
        where = juce::String (p->name) + " in the roll, caret at bar " + juce::String (bar + 1) + " beat " + juce::String (1.0 + static_cast<double> (inBar) / static_cast<double> (beat), 2);
        if (! controller.selection.empty()) where += "   |   " + juce::String (static_cast<int> (controller.selection.size())) + " selected";
    }
    g.setColour (theme::text);
    const auto middle = r.withTrimmedRight (320);
    g.drawText (where, middle, juce::Justification::centred);
    // The message on the left stops short of the middle, ending in "..." if
    // it is too long, rather than running into it.
    const int whereWidth = juce::roundToInt (juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), where));
    const int leftEnd = where.isEmpty() ? r.getRight() - 320 : middle.getCentreX() - whereWidth / 2 - 16;

    // Left: the last thing that happened, or the mode.
    juce::String left = controller.status;
    if (controller.stepInput) left = "STEP INPUT - play a MIDI keyboard: each note goes at the caret, " + juce::String (controller.grid.name()) + " long. Esc to stop.";
    g.setColour (controller.stepInput ? theme::accent : theme::text);
    g.drawText (left, r.withRight (std::max (r.getX() + 80, leftEnd)), juce::Justification::centredLeft, true);
}

} // namespace nt
