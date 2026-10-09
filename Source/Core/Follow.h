/*
    Follow - keeping the playhead on screen while the music plays (decision
    0033).

    A page at a time, as DAWs and notation programs turn pages: nothing moves
    while the playhead crosses the screen; shortly before it reaches the right
    edge the view jumps on, so the playhead lands just in from the left and
    the music it is about to play is in view. If the playhead is off the
    screen altogether - the user scrolled away, or playback started
    elsewhere - the view goes to it.

    Pure arithmetic in pixels, so the rule is tested without a window or a
    sound card. Shared by both apps.
*/

#pragma once

#include <algorithm>

namespace nt
{

// How far before the right edge the page turns, and how far in from the
// left the playhead lands: a little of the screen, never too little or too much.
inline double followMargin (double width)
{
    return std::clamp (width * 0.08, 24.0, 120.0);
}

// Where to scroll to, given the playhead's x and the scroll position (both
// pixels from the start of the music) and the width of the view; `scroll`
// itself when nothing needs to move.
inline double followScroll (double playheadX, double scroll, double width)
{
    const double margin = followMargin (width);
    if (playheadX < scroll || playheadX > scroll + width - margin) return std::max (0.0, playheadX - margin);
    return scroll;
}

} // namespace nt
