/*
    GeneratorPanel - where most of the music comes from.

    Pick a generator, set it up, press Generate: a list of results comes
    back. Click one to hear it on the instruments it would go to, and Insert
    puts it into the part the caret is in, at the caret's bar, fitted to that
    instrument. The settings are each generator's own, drawn from what its
    adapter says it has, so nothing here knows what Good Idea or the Catalogue
    can do - a setting added to an engine shows up here by itself.

    Every list Generate (or More) makes is kept, and each is a step in Undo:
    an idea lost to one Generate too many comes back with Cmd+Z (0049).
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
    SettingsList settings { controller };
    juce::TextButton generateButton { "Generate" }, moreButton { "More" }, insertButton { "Insert" }, stopButton { "Stop" };
    juce::ListBox results { "Results", this };
    juce::Label message, target;
    std::vector<GeneratedResult> found;
    int seed = 1;
    std::string current;
    std::vector<std::string> listed;   // ids, in the order of the menu

    // What each Generate listed, oldest first, and which one Undo has the
    // list at (decision 0049). The first is the empty list before any.
    struct Listed
    {
        std::vector<GeneratedResult> results;
        std::string generator;
        int seed = 1;
        int row = -1;
        juce::String message;
    };
    std::vector<Listed> history { Listed {} };
    size_t shown = 0;
    bool quiet = false;                // a list brought back is not auditioned
    void showListed (int direction);

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    int getNumRows() override { return static_cast<int> (found.size()); }
    void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
    void selectedRowsChanged (int row) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
    juce::String getTooltipForRow (int row) override;

    void rebuildSettings();
    void run (bool more);
    void insertSelected();
    bool needsSelection() const;
};

} // namespace nt
