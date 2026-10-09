/*
    Follow - keeping the playhead on screen while the music plays (decisions
    0033, 0034).

    Two ways, the user's choice:

      - Smooth (the default, 0034): the playhead walks in from the left until
        it is a third of the way across, then stays there and the music moves
        under it, so two thirds of the screen is always what is coming.
      - Page (0033): nothing moves while the playhead crosses the screen;
        shortly before it reaches the right edge the view jumps on, so the
        playhead lands just in from the left.

    Either way a playhead off the screen - the user scrolled away, or
    playback started elsewhere - is gone to.

    The sound reports where it has got to once per audio block, every ten
    milliseconds or so, in steps; scrolled straight from those the music
    would judder. `SmoothClock` runs on a steady clock of its own, drawn
    gently toward the sound.

    Pure arithmetic in pixels and seconds, so it is tested without a window
    or a sound card. Shared by both apps.
*/

#pragma once

#include <algorithm>
#include <cmath>

namespace nt
{

enum class FollowStyle { smooth, page };

// Page: how far before the right edge the page turns, and how far in from
// the left the playhead lands - a little of the screen, never too little or
// too much.
inline double followMargin (double width)
{
    return std::clamp (width * 0.08, 24.0, 120.0);
}

// Smooth: where the playhead stays while the music moves under it.
inline double followAnchor (double width)
{
    return width / 3.0;
}

// Where to scroll to, given the playhead's x and the scroll position (both
// pixels from the start of the music) and the width of the view; `scroll`
// itself when nothing needs to move. The caller keeps the scroll inside the
// music, so near the end the view stops and the playhead runs on to the edge.
inline double followScroll (double playheadX, double scroll, double width, FollowStyle style = FollowStyle::page)
{
    if (style == FollowStyle::smooth)
    {
        const double anchor = followAnchor (width);
        if (playheadX >= scroll && playheadX <= scroll + anchor) return scroll;
        return std::max (0.0, playheadX - anchor);
    }
    const double margin = followMargin (width);
    if (playheadX < scroll || playheadX > scroll + width - margin) return std::max (0.0, playheadX - margin);
    return scroll;
}

// The playhead as the screen should show it. The sound reports its place
// in steps, one audio block at a time, and the screen asks at its own
// uneven moments; taken straight, the two beat against each other and the
// music judders. So the playhead runs on its own steady clock and is drawn
// gently toward the sound each time it is asked: it moves evenly, and is
// never more than a tenth of a second from what is heard. It never moves
// backwards, except when playback starts again somewhere else.
class SmoothClock
{
public:
    // `sound`: the seconds the sound has reached; `now`: a steady clock in
    // seconds. Returns the seconds to draw the playhead at.
    double update (double sound, double now)
    {
        const bool restarted = shown < 0.0 || sound < lastSound - 1e-6 || std::abs (sound - shown) > 0.25;
        lastSound = sound;
        if (restarted)
        {
            shown = sound;
            lastNow = now;
            return shown;
        }
        const double dt = std::clamp (now - lastNow, 0.0, 0.1);
        lastNow = now;
        double next = shown + dt;              // the clock runs on
        next += (sound - next) * 0.15;         // and is drawn gently toward the sound
        next = std::clamp (next, sound - 0.1, sound + 0.1);
        shown = std::max (shown, next);
        return shown;
    }

    void reset() { shown = -1.0; }

private:
    double shown = -1.0, lastSound = -1.0, lastNow = 0.0;
};

} // namespace nt
