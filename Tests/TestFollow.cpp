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
