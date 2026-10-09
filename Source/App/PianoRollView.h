/*
    PianoRollView - one part's notes as a DAW shows them: a keyboard down the
    left, a row for every key, each note a bar as long as it sounds, the grid
    behind, and the velocity of every note in a lane underneath
    (decisions 0027, 0028, 0030).

    Select (the default): click a note to choose and hear it, drag it to move
    it in time and pitch (Alt drags a copy), drag its right or left end to
    stretch it, drag on empty space to lasso notes, double-click empty space
    to draw a note. Draw (D): click to draw a note and drag to make it
    longer; click a note to delete it. Everything snaps to the grid unless
    snapping is off. The other parts' notes show faintly behind, so a line
    can be written against them.

    Down the side, beside the keys, the part's instrument shows its range
    and where it sounds best (decision 0006); notes it cannot play are red,
    notes outside its best register grey, and a note it would struggle with
    (a chord on a one-line instrument, too fast, too wide a leap) carries a
    red mark - hover to see why.
*/

#pragma once

#include "Timeline.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <map>

namespace nt
{

class PianoRollView : public juce::Component,
                      public juce::SettableTooltipClient,
                      private juce::ChangeListener,
                      private juce::Timer,
                      private juce::ScrollBar::Listener
{
public:
    PianoRollView (Controller& c, Timeline& t);
    ~PianoRollView() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    static constexpr int rulerHeight = 22, velocityHeight = 78, velocityGap = 5;
    static constexpr int keysWidth = 54;   // the keys, at the right of the left column

    // Scrolls so the part's music, or where its instrument sounds best, is in view.
    void centreOnPart();

private:
    enum class Mode { none, caret, keys, move, stretchEnd, stretchStart, lasso, draw, velocity };

    Controller& controller;
    Timeline& timeline;
    juce::ScrollBar vbar { true };
    double scrollY = 0;
    uint32_t shownPart = 0;

    Mode mode = Mode::none;
    juce::Point<float> downAt;
    uint32_t dragNote = 0;
    Tick dragStart = 0, dragEnd = 0;     // the clicked note's, before the drag
    int dragPitch = 60;
    Tick dragBy = 0;                     // move, or stretch
    int dragSemitones = 0;
    bool dragCopy = false, dragged = false;
    juce::Rectangle<float> lasso;
    Tick drawStart = 0, drawLength = 0;
    int drawPitch = 60;
    int keyPitch = -1;
    std::map<uint32_t, int> velocityEdits;   // the velocity lane's drag, before it is kept
    std::map<uint32_t, int> velocityFrom;
    bool velocityRelative = false;
    float lastVelocityX = 0;

    juce::Point<float> hover { -1, -1 };
    Tick lastPlayhead = -1;
    std::vector<uint32_t> lastSounding;

    const Part* part() const { return controller.caretPartPtr(); }
    bool drums() const;
    juce::Rectangle<int> gridArea() const;
    juce::Rectangle<int> velocityArea() const;
    float rowHeight() const { return controller.rowHeight; }
    float yOf (int pitch) const;
    int pitchAt (float y) const;
    juce::Rectangle<float> noteRect (Tick start, Tick end, int pitch) const;
    // The note under the pointer and which part of it: 0 the body, 1 the
    // right end, -1 the left end.
    const Note* noteAt (juce::Point<float> p, int* zone = nullptr) const;
    const Note* stemAt (float x) const;
    Tick minLength() const;

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void scrollBarMoved (juce::ScrollBar*, double) override;
    void updateScrollbar();

    void paintRows (juce::Graphics&, const theme::RollColours&);
    void paintNotes (juce::Graphics&, const theme::RollColours&);
    void paintKeys (juce::Graphics&, const theme::RollColours&);
    void paintVelocities (juce::Graphics&, const theme::RollColours&);
    void velocityDrag (juce::Point<float> p);
    void showMenu (const Note* note);
    juce::String tooltipAt (juce::Point<float> p) const;
};

} // namespace nt
