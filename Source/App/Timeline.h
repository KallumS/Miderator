/*
    Timeline - the one time axis the tracks and the piano roll share, and the
    drawing both make along it.

    Time runs left to right in proportion, as in every DAW: a quarter note is
    `controller.zoom` pixels wherever it falls, so a bar in the tracks is
    exactly above the same bar in the roll (decision 0027). Both views keep a
    column of the same width at the left (the track names, the keyboard) and
    a scroll bar's width at the right, so their time areas line up.
*/

#pragma once

#include "Controller.h"
#include "Theme.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <set>

namespace nt
{

class Timeline : public juce::ChangeBroadcaster
{
public:
    explicit Timeline (Controller& c) : controller (c) {}

    static constexpr int left = 168;    // the names / keyboard column
    static constexpr int right = 12;    // a vertical scroll bar

    double scrollX = 0;                 // pixels of time scrolled off to the left
    int viewWidth = 800;                // the width of the time area, set by the workspace

    double pixelsPerTick() const { return static_cast<double> (controller.zoom) / static_cast<double> (PPQ); }
    float xOf (Tick t) const { return static_cast<float> (left + static_cast<double> (t) * pixelsPerTick() - scrollX); }
    Tick tickAt (float x) const { return static_cast<Tick> (std::floor ((static_cast<double> (x) - left + scrollX) / pixelsPerTick())); }
    // The score and a few bars after it, so there is room to draw past the end.
    double contentWidth() const;

    void scrollTo (double x);
    void zoomBy (float factor, float anchorX);
    // Brings `t` into view if it is not, a little in from the left.
    void reveal (Tick t);
    // While playing: a page on just before the playhead reaches the right
    // edge (decision 0033).
    void follow (Tick t);

private:
    Controller& controller;
};

// The window takes the keys, so Space and the arrows work after a click in
// a view: the first component up the tree that wants them gets them.
inline void focusWindow (juce::Component& c)
{
    for (auto* p = c.getParentComponent(); p != nullptr; p = p->getParentComponent())
        if (p->getWantsKeyboardFocus()) { p->grabKeyboardFocus(); return; }
}

// Drawing along the timeline, shared by the tracks, the roll and the
// Blocks preview.
namespace paint
{
// Bar, beat and (where there is room) grid lines over `area`.
void grid (juce::Graphics& g, juce::Rectangle<int> area, const Timeline& tl, const Score& score,
           const roll::Grid& grid, const theme::RollColours& c, bool steps);

// Bar numbers and beat ticks along the top, with the caret.
void ruler (juce::Graphics& g, juce::Rectangle<int> area, const Timeline& tl, const Score& score, Tick caret, bool caretLit);

// Notes small, in a box: what a track lane and the Blocks preview show.
// Pitches fit between the lowest and highest note, never closer than an
// octave, so a single line does not fill the lane.
struct Mini
{
    const std::set<uint32_t>* selected = nullptr;
    const std::set<uint32_t>* sounding = nullptr;
    const roll::Warnings* warnings = nullptr;
    float tallest = 6.0f;            // a note's height at most, in pixels
};
void miniNotes (juce::Graphics& g, juce::Rectangle<float> area, const std::vector<Note>& notes,
                const std::function<float (Tick)>& xOf, const theme::RollColours& c, const Mini& opts);
} // namespace paint

} // namespace nt
