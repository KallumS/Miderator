#include "Check.h"

#include "Follow.h"

using namespace nt;

TEST ("follow: the page holds while the playhead crosses it, and turns before the edge")
{
    const double w = 1000.0;                       // the margin is 80 pixels
    CHECK_EQ (followMargin (w), 80.0);
    CHECK_EQ (followScroll (100.0, 0.0, w), 0.0);  // on the page: nothing moves
    CHECK_EQ (followScroll (919.0, 0.0, w), 0.0);
    CHECK_EQ (followScroll (921.0, 0.0, w), 841.0);   // nearly off the right: the next page, playhead 80 in
    CHECK_EQ (followScroll (1500.0, 841.0, w), 841.0);
    CHECK_EQ (followScroll (1762.0, 841.0, w), 1682.0);
}

TEST ("follow: a playhead off the screen is gone to, never before the start")
{
    CHECK_EQ (followScroll (300.0, 2000.0, 1000.0), 220.0);   // scrolled past it: back to it
    CHECK_EQ (followScroll (5000.0, 0.0, 1000.0), 4920.0);    // far ahead: straight there
    CHECK_EQ (followScroll (10.0, 500.0, 1000.0), 0.0);
}

TEST ("follow: the margin stays sensible on small and large screens")
{
    CHECK_EQ (followMargin (200.0), 24.0);
    CHECK_EQ (followMargin (4000.0), 120.0);
}

TEST ("follow smoothly: the playhead walks in to a third of the way, then the music moves under it")
{
    const double w = 900.0;                        // the anchor is 300 pixels in
    const auto smooth = FollowStyle::smooth;
    CHECK_EQ (followAnchor (w), 300.0);
    CHECK_EQ (followScroll (0.0, 0.0, w, smooth), 0.0);
    CHECK_EQ (followScroll (299.0, 0.0, w, smooth), 0.0);       // still walking in
    CHECK_EQ (followScroll (301.0, 0.0, w, smooth), 1.0);       // now the view moves with it
    CHECK_EQ (followScroll (1000.0, 699.0, w, smooth), 700.0);  // a pixel a pixel
    CHECK_EQ (followScroll (100.0, 2000.0, w, smooth), 0.0);    // scrolled past it: back to it
    CHECK_EQ (followScroll (5000.0, 0.0, w, smooth), 4700.0);   // far ahead: straight there, at the third
}

TEST ("follow smoothly: the playhead starts where the sound is, and goes back when it starts again")
{
    SmoothClock c;
    CHECK_EQ (c.update (1.0, 10.0), 1.0);
    CHECK_EQ (c.update (0.0, 11.0), 0.0);       // played again from the start
    CHECK_EQ (c.update (8.0, 12.0), 8.0);       // or from much further on
    c.reset();
    CHECK_EQ (c.update (4.0, 13.0), 4.0);
}

TEST ("follow smoothly: steps of sound and uneven frames still give an even glide")
{
    // The sound moves on in blocks of 512 samples at 44.1 kHz; the screen
    // asks every 10 to 27 ms, as a busy message thread does. The playhead
    // must move with the clock, not with either.
    SmoothClock c;
    const double block = 512.0 / 44100.0;
    const double frames[] = { 0.016, 0.011, 0.019, 0.027, 0.016, 0.010, 0.022, 0.017, 0.016, 0.018 };
    double now = 0.0, shown = c.update (0.0, now), worst = 0.0;
    for (int i = 0; i < 600; ++i)
    {
        const double dt = frames[i % 10];
        now += dt;
        const double sound = std::floor (now / block + 1.0) * block;   // the end of the block being played
        const double next = c.update (sound, now);
        CHECK (next >= shown);                                       // never backwards
        CHECK (std::abs (next - sound) <= 0.1 + 1e-9);               // never far from what is heard
        if (i > 60) worst = std::max (worst, std::abs ((next - shown) - dt));
        shown = next;
    }
    CHECK (worst < 0.003);   // each frame moves by its own time, within 3 ms
}

TEST ("follow smoothly: when the sound stops, the playhead waits a little ahead of it")
{
    SmoothClock c;
    c.update (2.0, 0.0);
    double shown = 0.0;
    for (int i = 1; i < 200; ++i) shown = c.update (2.0, i / 60.0);
    CHECK (shown > 2.0);
    CHECK (shown <= 2.1 + 1e-9);
}
