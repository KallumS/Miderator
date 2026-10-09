/*
    ArrangeView - the tracks: every part as a row, its notes drawn small in
    its bars, with the bar numbers, the scale and the chords along the top
    (decision 0027). It is where bars are chosen for a generator to fill
    (decision 0019) and where the piano roll is pointed at a part.

    A click on a track's name shows that part in the piano roll; a click in a
    track chooses that bar in that part, a drag chooses more bars and more
    parts, a drag along the lanes chooses every part, Shift and a click
    stretches the choice. A chord in the lane plays when clicked, the scale
    plays itself. The caret is set by clicking the ruler.
*/

#pragma once

#include "Timeline.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace nt
{

class ArrangeView : public juce::Component,
                    public juce::SettableTooltipClient,
                    private juce::ChangeListener,
                    private juce::Timer,
                    private juce::ScrollBar::Listener
{
public:
    ArrangeView (Controller& c, Timeline& t);
    ~ArrangeView() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    static constexpr int rulerHeight = 20, scaleHeight = 20, chordHeight = 26;
    static constexpr int lanesHeight = rulerHeight + scaleHeight + chordHeight;
    static constexpr int trackHeight = 44;

    // Keeps the caret's part in view, as the parts change.
    void revealPart (uint32_t partId);

private:
    Controller& controller;
    Timeline& timeline;
    juce::ScrollBar vbar { true };
    double scrollY = 0;
    bool selectingBars = false, allParts = false, settingCaret = false;
    int anchorBar = 0, anchorPart = 0;
    Tick lastPlayhead = -1;
    std::vector<uint32_t> lastSounding;

    juce::Rectangle<int> tracksArea() const;
    float trackTop (int index) const;
    int trackAt (float y) const;               // clamped to the parts there are
    int barAt (float x) const;
    juce::Rectangle<float> muteBox (int index) const;
    juce::Rectangle<float> soloBox (int index) const;

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void scrollBarMoved (juce::ScrollBar*, double) override;
    void updateScrollbar();

    void paintLanes (juce::Graphics&, const theme::RollColours&);
    void paintHeaders (juce::Graphics&, const theme::RollColours&);
    juce::String tooltipAt (juce::Point<float> p) const;
};

} // namespace nt
