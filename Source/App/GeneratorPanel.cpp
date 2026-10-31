#include "GeneratorPanel.h"

#include "Theme.h"

namespace nt
{

GeneratorPanel::GeneratorPanel (Controller& c) : controller (c)
{
    for (auto* comp : std::initializer_list<juce::Component*> { &generator, &description, &followKey, &settings, &generateButton,
                                                                 &insertButton, &stopButton, &results, &message, &target })
        addAndMakeVisible (comp);
    settings.context = [this] { return controller.generatorContext (needsSelection()); };
    settings.onChange = [this] { resized(); };

    // Only the generators meant for this tab: Good Idea first, the main one,
    // then the two that work on music already written (decision 0017).
    for (const auto& g : controller.lua.generators())
        if (g.panel == "generate")
        {
            listed.push_back (g.id);
            generator.addItem (g.name, static_cast<int> (listed.size()));
        }
    generator.onChange = [this]
    {
        const int i = generator.getSelectedId() - 1;
        if (i < 0 || i >= static_cast<int> (listed.size())) return;
        current = listed[static_cast<size_t> (i)];
        if (! found.empty()) history[shown].row = results.getSelectedRow();
        found.clear();
        results.updateContent();
        if (controller.lua.ok()) showMessage ("Press Generate to make some ideas, then click one to hear it.");
        rebuildSettings();
    };

    generator.setTitle ("Generator");
    description.setFont (juce::FontOptions (13.0f));
    description.setColour (juce::Label::textColourId, theme::textDim);
    description.setJustificationType (juce::Justification::topLeft);
    message.setFont (juce::FontOptions (13.0f));
    message.setColour (juce::Label::textColourId, theme::stepNumber);
    message.setJustificationType (juce::Justification::topLeft);
    target.setFont (juce::FontOptions (13.0f));
    target.setColour (juce::Label::textColourId, theme::text);

    followKey.setToggleState (true, juce::dontSendNotification);
    followKey.setTooltip ("Write in the key of the music at the cursor, instead of the generator's own key setting");
    followKey.onClick = [this] { rebuildSettings(); };

    generateButton.setTooltip ("Make new results");
    moreButton.setTooltip ("More results with the same settings");
    insertButton.setTooltip ("Put the chosen result in, where the Into line says (Return, or double-click it)");
    stopButton.setTooltip ("Stop the audition (Esc)");
    generateButton.onClick = [this] { run (false); };
    moreButton.onClick = [this] { run (true); };
    insertButton.onClick = [this] { insertSelected(); };
    stopButton.onClick = [this] { controller.stop(); };

    results.setRowHeight (44);
    results.setColour (juce::ListBox::backgroundColourId, theme::sunken);
    results.setColour (juce::ListBox::outlineColourId, theme::rule);
    results.setOutlineThickness (1);
    results.setTitle ("Results");

    controller.addChangeListener (this);
    controller.stepResults = [this] (int direction) { showListed (direction); };
    generator.setSelectedId (1);
    theme::clicksKeepFocus (*this);
    if (! controller.lua.ok())
        showMessage ("The generators did not load. Reinstall the app, and if that does not help, please report it:\n"
                     + juce::String (controller.lua.error()), true);
}

void GeneratorPanel::showMessage (const juce::String& text, bool error)
{
    // An error in a red light enough to read; the house red only marks things.
    message.setText (text, juce::dontSendNotification);
    message.setColour (juce::Label::textColourId, error ? theme::warnText : theme::stepNumber);
}

void GeneratorPanel::ResultsList::paintOverChildren (juce::Graphics& g)
{
    juce::ListBox::paintOverChildren (g);
    if (! hasKeyboardFocus (true)) return;
    g.setColour (theme::text);
    g.drawRect (getLocalBounds(), 2);
}

GeneratorPanel::~GeneratorPanel()
{
    controller.stepResults = nullptr;
    controller.removeChangeListener (this);
}

bool GeneratorPanel::needsSelection() const
{
    for (const auto& g : controller.lua.generators())
        if (g.id == current) return g.needsSelection;
    return false;
}

void GeneratorPanel::paint (juce::Graphics& g) { g.fillAll (theme::ground); }

void GeneratorPanel::resized()
{
    auto r = getLocalBounds().reduced (10);
    generator.setBounds (r.removeFromTop (28));
    r.removeFromTop (4);
    // In a short panel the description gives way to the results; it is the
    // menu's tooltip as well.
    description.setVisible (r.getHeight() > 520);
    if (description.isVisible()) description.setBounds (r.removeFromTop (52));
    followKey.setBounds (r.removeFromTop (24));
    r.removeFromTop (4);
    // The results keep room for two or three, however short the window
    // (a large interface size makes it short): the settings scroll instead.
    const int below = 8 + 20 + 4 + 30 + 6 + 38 + 4, minResults = 140;
    const int settingsH = std::max (60, std::min (settings.contentHeight(), std::min (std::max (120, r.getHeight() / 2 - 40),
                                                                                     r.getHeight() - below - minResults)));
    settings.setBounds (r.removeFromTop (settingsH));
    r.removeFromTop (8);
    target.setBounds (r.removeFromTop (20));
    r.removeFromTop (4);
    auto buttons = r.removeFromTop (30);
    const int w = (buttons.getWidth() - 8) / 3;
    generateButton.setBounds (buttons.removeFromLeft (w)); buttons.removeFromLeft (4);
    insertButton.setBounds (buttons.removeFromLeft (w)); buttons.removeFromLeft (4);
    stopButton.setBounds (buttons);
    r.removeFromTop (6);
    // What happened, under the buttons that did it, above the results.
    message.setBounds (r.removeFromTop (38));
    r.removeFromTop (4);
    results.setBounds (r);
}

void GeneratorPanel::rebuildSettings()
{
    const auto ctx = controller.generatorContext (needsSelection());
    if (followKey.getToggleState()) controller.lua.useKey (current, ctx.root, ctx.scale);
    for (const auto& g : controller.lua.generators())
        if (g.id == current)
        {
            description.setText (g.description, juce::dontSendNotification);
            generator.setTooltip (g.description);
        }
    // The key and scale settings are the score's while the toggle is on.
    settings.show (current, followKey.getToggleState() ? std::set<std::string> { "Key", "Scale" } : std::set<std::string> {});
    resized();
}

void GeneratorPanel::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // Nothing chosen: chords shared across every part from the caret's bar
    // (0041), a single line into the caret's part (0042) - or across the
    // parts chosen by name, a single line to the first chosen (0050).
    juce::String into = "Into: ";
    const juce::String bar = "from bar " + juce::String (controller.score.barAt (controller.caret) + 1);
    const auto* line = controller.score.partById (controller.lineTarget());
    const juce::String lineName = line != nullptr ? juce::String (line->name) : juce::String();
    if (controller.score.parts.size() > 1) into += bar + ui (" — chords to ") + controller.partsText() + ", one line to " + lineName;
    else into += lineName + ", " + bar;
    if (controller.range.active() && (! needsSelection() || current == "midi-variator") && ! controller.variesAcrossParts (current))
    {
        into = "Into: " + controller.rangeText();
        if (! needsSelection() && controller.range.parts.size() > 1) into += ui (" — one line to ") + lineName;
        if (needsSelection())
            into = controller.selection.empty() ? juce::String ("Choose bars with music in them to vary.")
                 : controller.rangeText() + ui (" — the variation replaces them");
    }
    // Vary Notes on music in several parts goes after it, across them (0052).
    else if (needsSelection() && ! controller.selection.empty() && controller.variesAcrossParts (current))
    {
        const auto [a, b] = controller.selectedBars();
        into = "Into: after bar " + juce::String (b + 1) + ", across the "
             + juce::String (static_cast<int> (controller.partsWithSelection().size())) + " parts it varies";
    }
    else if (needsSelection())
    {
        const auto [a, b] = controller.selectedBars();
        const juce::String bars = "bar" + juce::String (a == b ? " " : "s ") + juce::String (a + 1) + (a == b ? juce::String() : ui ("–") + juce::String (b + 1));
        into = controller.selection.empty() ? juce::String ("Select some notes in the score to use this generator.")
             : juce::String (static_cast<int> (controller.selection.size())) + " notes in " + bars
               + (current == "midi-variator" ? ui (" — the variation goes after them") : ui (" — ideas go beside them"));
    }
    target.setText (into, juce::dontSendNotification);
    insertButton.setEnabled (results.getSelectedRow() >= 0);
}

void GeneratorPanel::run()
{
    if (current.empty()) return;
    seed = static_cast<int> (juce::Random::getSystemRandom().nextInt (90000)) + 1;
    const auto ctx = controller.generatorContext (needsSelection());
    if (followKey.getToggleState()) controller.lua.useKey (current, ctx.root, ctx.scale);
    const auto out = controller.lua.generate (current, ctx, seed, 6);
    if (! out.error.empty())
    {
        showMessage ("Nothing came back. Try other settings; if it keeps happening, please report it: "
                     + juce::String (out.error).upToFirstOccurrenceOf ("\n", false, false), true);
        return;
    }
    if (more) found.insert (found.end(), out.results.begin(), out.results.end());
    else found = out.results;
    showMessage (out.message.empty() ? juce::String (static_cast<int> (found.size())) + " results. Click one to hear it; Insert puts it in."
                                     : juce::String (out.message));
    results.updateContent();
    results.repaint();
    if (! found.empty()) results.selectRow (0, false, true);

    // Kept, and a step in Undo (decision 0049). A Generate after an Undo
    // drops the lists that Redo would have brought back, as an edit does.
    history.resize (shown + 1);
    history.push_back ({ found, current, results.getSelectedRow(), message.getText() });
    if (history.size() > 301) history.erase (history.begin());
    shown = history.size() - 1;
    controller.generated();
}

void GeneratorPanel::showListed (int direction)
{
    const int to = static_cast<int> (shown) + direction;
    if (to < 0 || to >= static_cast<int> (history.size())) return;
    if (! found.empty()) history[shown].row = results.getSelectedRow();
    shown = static_cast<size_t> (to);
    const auto& l = history[shown];
    // The list may have come from another generator: that one is chosen again,
    // so Insert places it as its own generator would.
    if (! l.generator.empty() && l.generator != current)
    {
        current = l.generator;
        for (size_t i = 0; i < listed.size(); ++i)
            if (listed[i] == current) generator.setSelectedId (static_cast<int> (i) + 1, juce::dontSendNotification);
        rebuildSettings();
    }
    found = l.results;
    message.setColour (juce::Label::textColourId, theme::stepNumber);
    message.setText (l.message, juce::dontSendNotification);
    results.updateContent();
    results.repaint();
    const juce::ScopedValueSetter<bool> hush (quiet, true);
    if (l.row >= 0 && l.row < static_cast<int> (found.size())) results.selectRow (l.row, false, true);
    else results.deselectAllRows();
    insertButton.setEnabled (results.getSelectedRow() >= 0);
}

void GeneratorPanel::insertSelected()
{
    const int row = results.getSelectedRow();
    if (row < 0 || row >= static_cast<int> (found.size())) return;
    controller.stop();
    controller.insertGenerated (found[static_cast<size_t> (row)], needsSelection(), current);
}

void GeneratorPanel::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (row < 0 || row >= static_cast<int> (found.size())) return;
    const auto& r = found[static_cast<size_t> (row)];
    if (selected)
    {
        g.setColour (theme::frameActive);
        g.fillRect (0, 0, width, height);
        g.setColour (theme::accent);
        g.fillRect (0, 2, 3, height - 4);
    }
    g.setColour (theme::rule);
    g.fillRect (0, height - 1, width, 1);
    g.setColour (theme::text);
    g.setFont (juce::FontOptions (13.5f, juce::Font::bold));
    g.drawText (r.title, 10, 3, width - 16, 20, juce::Justification::centredLeft, true);
    int notes = 0;
    for (const auto& p : r.parts) notes += static_cast<int> (p.notes.size());
    juce::String sub;
    for (const auto& p : r.parts) sub += (sub.isEmpty() ? "" : ", ") + juce::String (p.name);
    sub += ui ("  ·  ") + juce::String (static_cast<double> (r.length) / PPQ, 1) + " beats, " + juce::String (notes) + " notes";
    g.setColour (theme::textDim);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText (sub, 10, 22, width - 16, 18, juce::Justification::centredLeft, true);
}

void GeneratorPanel::selectedRowsChanged (int row)
{
    insertButton.setEnabled (row >= 0);
    if (! quiet && row >= 0 && row < static_cast<int> (found.size())) controller.auditionGenerated (found[static_cast<size_t> (row)], needsSelection(), current);
}

void GeneratorPanel::listBoxItemDoubleClicked (int row, const juce::MouseEvent&)
{
    results.selectRow (row);
    insertSelected();
}

void GeneratorPanel::returnKeyPressed (int row)
{
    results.selectRow (row);
    insertSelected();
}

juce::String GeneratorPanel::getNameForRow (int row)
{
    // What VoiceOver reads for a result: its title and what is in it.
    if (row < 0 || row >= static_cast<int> (found.size())) return {};
    const auto& r = found[static_cast<size_t> (row)];
    int notes = 0;
    for (const auto& p : r.parts) notes += static_cast<int> (p.notes.size());
    return juce::String (r.title) + ", " + juce::String (notes) + " notes";
}

juce::String GeneratorPanel::getTooltipForRow (int row)
{
    if (row < 0 || row >= static_cast<int> (found.size())) return {};
    return found[static_cast<size_t> (row)].detail;
}

} // namespace nt
