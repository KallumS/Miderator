/*
    BlocksPanel - the Starting Blocks toolbox (decision 0018).

    The smallest useful pieces, picked by hand rather than generated: choose
    what kind of block (a chord, an arpeggio, a run, an interval), and every
    degree of the key is laid out as a button (drums and bass are Good
    Idea's, decision 0024). Click
    one to see it written and hear it; Insert (or a double-click) puts it at
    the caret and moves the caret past it, so blocks can be laid one after
    another the way Starting Blocks lays them in REAPER. With bars chosen on
    the page, the block fills them instead.
*/

#pragma once

#include "Controller.h"
#include "SettingsList.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace nt
{

class BlocksPanel : public juce::Component,
                    private juce::ChangeListener
{
public:
    explicit BlocksPanel (Controller& c);
    ~BlocksPanel() override;
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    class DegreeButton;
    class Preview;

    Controller& controller;
    juce::Label key;
    juce::OwnedArray<juce::TextButton> kinds;
    SettingsList settings { controller };
    juce::OwnedArray<DegreeButton> degrees;
    std::unique_ptr<Preview> preview;
    juce::TextButton playButton { "Play" }, insertButton { "Insert" };
    juce::Label target;

    std::vector<GeneratedResult> blocks;   // one per degree
    int chosen = 0;
    int keyRoot = -1, keyScale = -1;       // what the blocks were made in

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void setKind (int index);
    void remake();                         // the blocks again, from the settings and the key
    void choose (int index, bool play);
    void insertChosen();
    GeneratorContext context() const;
};

} // namespace nt
