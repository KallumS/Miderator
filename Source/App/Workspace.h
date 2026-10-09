/*
    Workspace - the tracks above, the piano roll below, a divider to drag
    between them, and one scroll bar along the bottom that moves both, so a
    bar is always in the same place in each (decision 0027).

    While the music plays it reads the playhead once a frame, sixty times a
    second, scrolls to follow it (decisions 0033, 0034) and hands both views
    the same place, so the tracks and the roll move as one.
*/

#pragma once

#include "ArrangeView.h"
#include "PianoRollView.h"
#include "Timeline.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace nt
{

class Workspace : public juce::Component,
                  private juce::ChangeListener,
                  private juce::Timer,
                  private juce::ScrollBar::Listener
{
public:
    explicit Workspace (Controller& c);
    ~Workspace() override;

    void resized() override;
    void paint (juce::Graphics&) override;

    Timeline timeline;
    ArrangeView arrange;
    PianoRollView roll;

    // How much of the height the tracks take, 0.15 to 0.85.
    float split = 0.38f;

private:
    class Divider : public juce::Component
    {
    public:
        explicit Divider (Workspace& w) : owner (w) { setMouseCursor (juce::MouseCursor::UpDownResizeCursor); }
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override { from = owner.split; }
        void mouseDrag (const juce::MouseEvent& e) override;
    private:
        Workspace& owner;
        float from = 0.4f;
    };

    Controller& controller;
    Divider divider { *this };
    juce::ScrollBar hbar { false };

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    bool wasPlaying = false;
    void scrollBarMoved (juce::ScrollBar*, double) override;
    void updateScrollbar();
};

} // namespace nt
