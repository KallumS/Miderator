/*
    GeneratorPanel - where most of the music comes from.

    Pick a generator, set it up, press Generate: a list of results comes
    back. Click one to hear it on the instruments it would go to, and Insert
    puts it into the part the caret is in, at the caret's bar, fitted to that
    instrument. The settings are each generator's own, drawn from what its
    adapter says it has, so nothing here knows what Good Idea or the Catalogue
    can do - a setting added to an engine shows up here by itself.
*/

#pragma once

#include "Controller.h"
#include "SettingsList.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace nt
{

class GeneratorPanel : public juce::Component,
                       private juce::ChangeListener,
                       private juce::ListBoxModel
{
public:
    explicit GeneratorPanel (Controller& c);
    ~GeneratorPanel() override;
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    Controller& controller;
    juce::ComboBox generator;
    juce::Label description;
    juce::ToggleButton followKey { "Use the score's key" };
    // Vary Notes only, where the score's key means nothing (it hears the key
    // in the music): the variation after the selection, or in its place
    // (decision 0049).
    juce::Label placementLabel { {}, "Put it" };
    juce::ComboBox placement;
    bool varies() const { return current == "midi-variator"; }
    SettingsList settings { controller };
    juce::TextButton generateButton { "Generate" }, moreButton { "More" }, insertButton { "Insert" }, stopButton { "Stop" };
    // The results, ringed when Tab reaches them (decision 0048).
    struct ResultsList : public juce::ListBox
    {
        using juce::ListBox::ListBox;
        void paintOverChildren (juce::Graphics&) override;
        void focusGained (FocusChangeType) override { repaint(); }
        void focusLost (FocusChangeType) override { repaint(); }
        void focusOfChildComponentChanged (FocusChangeType) override { repaint(); }
    };
    ResultsList results { "Results", this };
    juce::Label message, target;
    std::vector<GeneratedResult> found;
    int seed = 1;
    std::string current;
    std::vector<std::string> listed;   // ids, in the order of the menu

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    int getNumRows() override { return static_cast<int> (found.size()); }
    void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
    void selectedRowsChanged (int row) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
    void returnKeyPressed (int row) override;
    juce::String getNameForRow (int row) override;
    juce::String getTooltipForRow (int row) override;

    void rebuildSettings();
    void run (bool more);
    void insertSelected();
    bool needsSelection() const;
    void showMessage (const juce::String& text, bool error = false);
};

} // namespace nt
