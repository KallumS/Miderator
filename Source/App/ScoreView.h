/*
    ScoreView - the page: the score in one long system (a galley, like a
    DAW's timeline), with the chords and the key read off it in two lanes
    along the top, the instruments' names and clefs held at the left while
    the music scrolls under them, the caret where the next note goes and the
    playhead while it plays.

    Clicking a note plays it; clicking a chord in the lane plays the whole
    harmony there. In note input, clicking a line or space writes the note
    the key gives it.
*/

#pragma once

#include "Controller.h"
#include "ScoreRenderer.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace nt
{

class ScoreView : public juce::Component,
                  public juce::SettableTooltipClient,
                  private juce::ChangeListener,
                  private juce::Timer,
                  private juce::ScrollBar::Listener
{
public:
    explicit ScoreView (Controller& c);
    ~ScoreView() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    void scrollToTick (Tick t);
    void zoomBy (float factor);

private:
    Controller& controller;
    juce::ScrollBar hbar { false }, vbar { true };
    double scrollX = 0, scrollY = 0;   // pixels
    juce::Rectangle<int> rubberBand;
    bool dragging = false, draggingNotes = false;
    // Choosing bars: where the drag began, and whether it spans every part.
    bool selectingBars = false, allParts = false;
    int anchorBar = 0, anchorPart = 0;
    juce::Point<float> dragStart;
    int dragPitchFrom = 0, dragSemitones = 0;
    juce::Point<float> hover { -1, -1 };
    Tick lastPlayhead = -1;
    std::vector<uint32_t> lastSounding;

    static constexpr int lanesHeight = 50;
    static constexpr int gutter = 120;

    float space() const { return controller.zoom; }
    juce::Point<float> origin() const;
    float headerWidthPx() const;
    juce::Rectangle<int> musicArea() const;
    juce::Point<double> toLayout (juce::Point<float> p) const;
    int positionAt (int staff, double layoutY) const;
    Tick snapTick (Tick t) const;
    int staffPart (int staffIndex) const;
    int barAtX (float x) const;
    int partAtY (float y) const;

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void scrollBarMoved (juce::ScrollBar*, double) override;
    void updateScrollbars();

    void paintLanes (juce::Graphics&);
    void paintRange (juce::Graphics&);
    void paintGutter (juce::Graphics&);
    void paintCaret (juce::Graphics&);
    void paintGhost (juce::Graphics&);
    juce::String tooltipAt (juce::Point<float> p) const;
};

} // namespace nt
