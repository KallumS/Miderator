#include "Panels.h"

#include "Spelling.h"

namespace nt
{

//==============================================================================

void IconButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    getLookAndFeel().drawButtonBackground (g, *this, findColour (juce::TextButton::buttonColourId), highlighted, down);
    juce::Path p;
    if (kind == Kind::start || kind == Kind::end)
    {
        // A bar and a triangle pointing at it: back to the start, or on to the end.
        const auto r = getLocalBounds().toFloat().withSizeKeepingCentre (14.0f, 12.0f);
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
        return;
    }
    // A chevron up or down, or a cross, stroked as the Mac's symbols are.
    const auto r = getLocalBounds().toFloat().withSizeKeepingCentre (9.0f, 9.0f);
    if (kind == Kind::up)
    {
        p.startNewSubPath (r.getX(), r.getBottom() - 2.0f);
        p.lineTo (r.getCentreX(), r.getY() + 2.0f);
        p.lineTo (r.getRight(), r.getBottom() - 2.0f);
    }
    else if (kind == Kind::down)
    {
        p.startNewSubPath (r.getX(), r.getY() + 2.0f);
        p.lineTo (r.getCentreX(), r.getBottom() - 2.0f);
        p.lineTo (r.getRight(), r.getY() + 2.0f);
    }
    else
    {
        p.startNewSubPath (r.getX(), r.getY());
        p.lineTo (r.getRight(), r.getBottom());
        p.startNewSubPath (r.getRight(), r.getY());
        p.lineTo (r.getX(), r.getBottom());
    }
    g.setColour (isEnabled() ? theme::ink : theme::ink.withAlpha (0.4f));
    g.strokePath (p, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void PlayButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    getLookAndFeel().drawButtonBackground (g, *this, findColour (juce::TextButton::buttonColourId), highlighted, down);
    const auto font = getLookAndFeel().getTextButtonFont (*this, getHeight());
    const float textW = juce::GlyphArrangement::getStringWidth (font, getButtonText());
    const float iconW = 10.0f, gap = 6.0f;
    const float x = (static_cast<float> (getWidth()) - (iconW + gap + textW)) * 0.5f;
    const auto icon = juce::Rectangle<float> (x, static_cast<float> (getHeight()) * 0.5f - 5.0f, iconW, 10.0f);
    g.setColour (theme::ink);
    if (getToggleState()) g.fillRect (icon.reduced (0.5f));
    else
    {
        juce::Path p;
        p.addTriangle (icon.getX(), icon.getY(), icon.getX(), icon.getBottom(), icon.getRight(), icon.getCentreY());
        g.fillPath (p);
    }
    g.setFont (font);
    g.drawText (getButtonText(), juce::Rectangle<float> (icon.getRight() + gap, 0.0f, textW + 2.0f, static_cast<float> (getHeight())),
                juce::Justification::centredLeft, false);
}

//==============================================================================

Toolbar::Toolbar (Controller& c) : controller (c)
{
    for (auto* b : std::initializer_list<juce::Button*> { &startButton, &endButton }) addAndMakeVisible (b);
    for (auto* b : std::initializer_list<juce::Component*> { &fileButton, &playButton, &followButton,
                     &selectButton, &drawButton, &tripletButton, &snapButton, &quantiseButton, &zoomOut, &zoomIn })
        addAndMakeVisible (b);
    addAndMakeVisible (gridLabel);
    addAndMakeVisible (gridBox);
    gridLabel.setColour (juce::Label::textColourId, theme::textDim);
    gridLabel.setJustificationType (juce::Justification::centredRight);

    fileButton.setTooltip ("New, Open, Save, Export, Undo and Redo, the song's settings, the sound, step input and the light look");
    playButton.setTooltip ("Play from the caret, or stop (Shift+Space; Space plays from bar 1)");
    startButton.setTooltip ("Return to the start (Home). If it is playing, it plays on from bar 1.");
    endButton.setTooltip ("Skip to the end of the music (End)");
    followButton.setTooltip ("Follow (F): while it plays, the view scrolls along with the music, or turns a page at a time (Play menu)");
    selectButton.setTooltip ("Select (D switches): click a note to choose it, drag it to move it, drag its end to stretch it, double-click to draw one");
    drawButton.setTooltip ("Draw (D switches): click the roll to draw a note, drag to make it longer, click a note to delete it");
    gridBox.setTooltip ("The grid notes snap to, and how long a drawn note is (keys 1-6)");
    tripletButton.setTooltip ("A triplet grid: three in the space of two (T)");
    snapButton.setTooltip ("Notes snap to the grid when they are drawn, moved or stretched");
    quantiseButton.setTooltip ("Pull the selected notes onto the grid, or the whole part in the roll if none are selected (Q)");
    zoomIn.setTooltip ("Zoom in (Cmd+=)");
    zoomOut.setTooltip ("Zoom out (Cmd+-)");
    // What VoiceOver says for buttons whose face is a sign, and which are on or off.
    zoomIn.setTitle ("Zoom In");
    zoomOut.setTitle ("Zoom Out");
    gridBox.setTitle ("Grid");
    for (auto* b : { &followButton, &snapButton, &tripletButton }) b->setToggleable (true);
    playButton.setToggleable (true);
    selectButton.setRadioGroupId (1, juce::dontSendNotification);
    drawButton.setRadioGroupId (1, juce::dontSendNotification);

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
    theme::clicksKeepFocus (*this);
    refresh();
}

Toolbar::~Toolbar() { controller.removeChangeListener (this); }

void Toolbar::paint (juce::Graphics& g)
{
    g.fillAll (theme::popup);
    g.setColour (theme::rule);
    g.fillRect (0, getHeight() - 1, getWidth(), 1);
}

// The piano roll's tools - Select, Draw, the grid, Quantise - are the
// second row when one row is not wide enough.
namespace
{
constexpr int leftWidth = 66 + 30 + 2 + 72 + 2 + 30 + 6 + 60 + 14;
constexpr int toolsWidth = 62 + 70 + 40 + 78 + 64 + 60 + 88;
constexpr int rightWidth = 28 + 2 + 28;
} // namespace

int Toolbar::heightFor (int width) const
{
    return width - 16 >= leftWidth + toolsWidth + 20 + rightWidth ? rowHeight : 2 * rowHeight - 8;
}

void Toolbar::resized()
{
    const bool twoRows = getHeight() > rowHeight;
    auto all = getLocalBounds().reduced (8, 7);
    auto r = twoRows ? all.removeFromTop (rowHeight - 14) : all;
    auto place = [] (juce::Rectangle<int>& row, juce::Component& c, int w, int gap = 4) { c.setBounds (row.removeFromLeft (w)); row.removeFromLeft (gap); };
    place (r, fileButton, 52, 14);
    place (r, startButton, 30, 2);
    place (r, playButton, 72, 2);
    place (r, endButton, 30, 6);
    place (r, followButton, 60, 14);
    auto right = r;
    zoomIn.setBounds (right.removeFromRight (28));
    right.removeFromRight (2);
    zoomOut.setBounds (right.removeFromRight (28));

    auto tools = twoRows ? all.withTrimmedTop (6) : r;
    place (tools, selectButton, 60, 2); place (tools, drawButton, 56, 14);
    place (tools, gridLabel, 36, 4);
    place (tools, gridBox, 74, 4);
    place (tools, tripletButton, 62, 2); place (tools, snapButton, 52, 8);
    place (tools, quantiseButton, 74, 14);
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
        // VoiceOver reads these rather than the letters on the buttons.
        name.setTitle ("Part name");
        instrument.setTitle ("Instrument");
        mute.setTitle ("Mute");
        solo.setTitle ("Solo");
        volume.setTitle ("Volume");
        mute.setToggleable (true);
        solo.setToggleable (true);
        autoCC.setTooltip ("Draw this part's dynamics, expression and vibrato curves automatically (AutoCC)");
        mute.onClick = [this] { toggle ([] (Part& p) { p.mute = ! p.mute; }); };
        solo.onClick = [this] { toggle ([] (Part& p) { p.solo = ! p.solo; }); };
        autoCC.onClick = [this] { toggle ([] (Part& p) { p.autoCC = ! p.autoCC; }); };
        volume.setSliderStyle (juce::Slider::LinearHorizontal);
        volume.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        volume.setRange (0.0, 1.0);
        volume.setTooltip ("Volume");
        volume.onDragEnd = [this] { commitVolume(); };
        // The arrow keys move it too, once Tab has reached it: each step is an edit.
        volume.onValueChange = [this] { if (! volume.isMouseButtonDown()) commitVolume(); };
        up.onClick = [this] { controller.movePart (partId, -1); };
        down.onClick = [this] { controller.movePart (partId, 1); };
        remove.onClick = [this]
        {
            const auto* p = controller.score.partById (partId);
            if (p == nullptr) return;
            if (p->notes.empty()) { controller.removePart (partId); return; }
            // The Mac's own alert, its button saying what it does (decision 0048).
            const auto options = juce::MessageBoxOptions()
                                     .withIconType (juce::MessageBoxIconType::QuestionIcon)
                                     .withTitle ("Remove " + juce::String (p->name) + "?")
                                     .withMessage ("Its notes go with it. Undo brings them back.")
                                     .withButton ("Remove")
                                     .withButton ("Cancel")
                                     .withAssociatedComponent (this);
            juce::NativeMessageBox::showAsync (options, [safe = juce::Component::SafePointer<Row> (this)] (int r)
            {
                if (safe != nullptr && r == 0) safe->controller.removePart (safe->partId);
            });
        };
        up.setTooltip ("Move up");
        down.setTooltip ("Move down");
        remove.setTooltip ("Remove this part");
        theme::clicksKeepFocus (*this);
    }

    void commitVolume()
    {
        const auto v = static_cast<float> (volume.getValue());
        const auto* p = controller.score.partById (partId);
        if (p == nullptr || std::abs (p->volume - v) < 0.0001f) return;
        controller.edit ("Changed a volume", [this, v] (Score& s) { if (auto* q = s.partById (partId)) q->volume = v; });
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
        current = controller.isPartChosen (partId);
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

    void mouseDown (const juce::MouseEvent& e) override { controller.clickPart (partId, e.mods.isCommandDown(), e.mods.isShiftDown()); }

    Controller& controller;
    uint32_t partId;
    bool current = false;
    std::vector<std::string> ids;
    juce::TextEditor name;
    juce::ComboBox instrument;
    juce::TextButton mute, solo;
    IconButton up { "Move Up", IconButton::Kind::up }, down { "Move Down", IconButton::Kind::down }, remove { "Remove", IconButton::Kind::remove };
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
    addButton.setMouseClickGrabsKeyboardFocus (false);
    viewport.setTitle ("Parts");
    addAndMakeVisible (info);
    info.setJustificationType (juce::Justification::topLeft);
    info.setFont (juce::FontOptions (13.0f));
    info.setColour (juce::Label::textColourId, theme::text);
    info.setTitle ("About this instrument");
    controller.addChangeListener (this);
    rebuild();
}

PartsPanel::~PartsPanel() { controller.removeChangeListener (this); }

void PartsPanel::paint (juce::Graphics& g) { g.fillAll (theme::ground); }

void PartsPanel::resized()
{
    // Add Instrument at the top, away from the window's bottom edge.
    auto r = getLocalBounds().reduced (8);
    addButton.setBounds (r.removeFromTop (28));
    r.removeFromTop (8);
    info.setBounds (r.removeFromBottom (130));
    r.removeFromBottom (6);
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
                       : inst.cc == CCShape::neutral ? "Default" : "None (it doesn't swell)";
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
    setTitle ("Status");
    controller.addChangeListener (this);
    startTimerHz (2);
}

StatusBar::~StatusBar() { controller.removeChangeListener (this); }

juce::String StatusBar::message() const
{
    if (controller.stepInput)
        return "Step input: play a MIDI keyboard and each note goes at the caret, " + juce::String (controller.grid.name()) + " long. Esc stops it.";
    return controller.status;
}

void StatusBar::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // A new message is read out as well as shown, for VoiceOver.
    const auto now = message();
    if (now != announced && now.isNotEmpty())
        juce::AccessibilityHandler::postAnnouncement (now, juce::AccessibilityHandler::AnnouncementPriority::medium);
    announced = now;
    setTooltip (now);
    repaint();
}

std::unique_ptr<juce::AccessibilityHandler> StatusBar::createAccessibilityHandler()
{
    // Read as text: what the line says now.
    struct Value final : public juce::AccessibilityTextValueInterface
    {
        explicit Value (StatusBar& b) : bar (b) {}
        bool isReadOnly() const override { return true; }
        juce::String getCurrentValueAsString() const override { return bar.message(); }
        void setValueAsString (const juce::String&) override {}
        StatusBar& bar;
    };
    return std::make_unique<juce::AccessibilityHandler> (*this, juce::AccessibilityRole::staticText,
                                                         juce::AccessibilityActions(),
                                                         juce::AccessibilityHandler::Interfaces { std::make_unique<Value> (*this) });
}

void StatusBar::paint (juce::Graphics& g)
{
    g.fillAll (theme::popup);
    g.setColour (theme::rule);
    g.fillRect (0, getHeight() - 1, getWidth(), 1);
    auto r = getLocalBounds().reduced (10, 0);
    g.setFont (juce::FontOptions (13.0f));

    // Right: what is making the sound and what is listening.
    const int midiIns = juce::MidiInput::getAvailableDevices().size();
    const juce::String right = controller.audio.synthName() + "   |   "
                             + (midiIns == 0 ? juce::String ("No MIDI keyboard") : juce::String (midiIns) + " MIDI input" + (midiIns > 1 ? "s" : ""));

    // Middle: where the caret is and what is selected.
    juce::String where;
    if (const auto* p = controller.caretPartPtr())
    {
        const auto& s = controller.score;
        const int bar = s.barAt (controller.caret);
        const Tick inBar = controller.caret - s.barStart (bar);
        const auto beat = s.meterAtBar (bar).beatTicks();
        where = juce::String (p->name) + " in the roll, cursor at bar " + juce::String (bar + 1) + " beat " + juce::String (1.0 + static_cast<double> (inBar) / static_cast<double> (beat), 2);
        if (! controller.selection.empty()) where += "   |   " + juce::String (static_cast<int> (controller.selection.size())) + " selected";
        if (controller.noPartChosen) where += "   |   no part chosen";
        else if (controller.chosenParts.size() > 1) where += "   |   " + juce::String (static_cast<int> (controller.chosenParts.size())) + " parts chosen";
    }

    // Left: the last thing that happened, or the mode. It comes first: when
    // the line is short of room - a narrow window, a large interface - the
    // sound goes, then the caret moves over, before the message is cut
    // (and the whole of it is the line's tooltip).
    const auto left = message();
    const auto& font = g.getCurrentFont();
    // Measured with some to spare: text drawn at 150% or 200% runs a little wider.
    const auto width = [&font] (const juce::String& t) { return juce::roundToInt (juce::GlyphArrangement::getStringWidth (font, t) * 1.05f) + 6; };
    const int gap = 24, leftW = width (left), whereW = width (where), rightW = width (right);
    const bool showRight = leftW + whereW + rightW + 2 * gap <= r.getWidth();
    auto rest = r;
    if (showRight)
    {
        g.setColour (theme::textDim);
        g.drawText (right, rest.removeFromRight (rightW), juce::Justification::centredRight);
        rest.removeFromRight (gap);
    }
    // The caret in the middle when there is room, at the right when not.
    auto middle = r.withSizeKeepingCentre (whereW, r.getHeight());
    if (middle.getX() < r.getX() + leftW + gap || middle.getRight() > rest.getRight())
        middle = rest.withLeft (std::max (rest.getX(), rest.getRight() - whereW));
    g.setColour (theme::text);
    g.drawText (where, middle, juce::Justification::centredLeft, true);
    const int leftEnd = where.isEmpty() ? rest.getRight() : middle.getX() - gap;

    g.setColour (controller.stepInput ? theme::accent : theme::text);
    g.drawText (left, r.withRight (std::max (r.getX() + 80, leftEnd)), juce::Justification::centredLeft, true);
}

} // namespace nt
