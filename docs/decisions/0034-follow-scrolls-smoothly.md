# 0034 - Following the playhead scrolls smoothly, by default

- **Date:** 2026-10-09
- **Status:** Accepted (Noterator and Miderator alike) - changes the default chosen in 0033

## Context

0033 made the view follow the playhead a page at a time, switchable. The
user asked instead for the view to move with the playhead: no sharp flip to
the next page, a smooth scroll along with the music.

## Decision

- **Smooth is the default** (`FollowStyle::smooth`, `Follow.h`): the playhead
  walks in from the left until it is a third of the way across, then stays
  there and the music moves under it. A third, not the middle, so two thirds
  of the screen is what is about to play. Near the end of the music the view
  stops and the playhead runs on to the edge.
- **Turning pages stays a choice** in the Play menu ("Scroll Along with the
  Music" / "Turn a Page at a Time"), remembered with Follow on or off.
- **The playhead is smoothed** (`SmoothClock`): the sound reports its place
  once per audio block, in steps of about 12 ms, and the screen asks every
  10-27 ms; scrolled straight from those, measured in the window, the music
  lurched between half and one and a half times its speed. The playhead now
  runs on a steady clock of its own, drawn gently toward the sound (never
  more than a tenth of a second from it), and moves within a few per cent of
  an even speed.
- **The screen redraws at 60 a second** while playing (it was 30).
- **The scroll is in whole pixels**, so the ink stays crisp as it moves.
- **Noterator:** the playhead's x is `Layout::playheadX`, which glides
  across bar lines where the page's own `xForTick` jumps the bar line and
  its spacing, which would have hitched the page once a bar.
- **Miderator:** the workspace reads the playhead once a frame and hands
  the same place to the tracks and the roll, which had a timer each; read
  twice, the two halves could have been a frame apart.

## Consequences

- The whole page is redrawn every frame while it plays. A very large score
  may not keep up on a slow Mac; Turn a Page at a Time costs less.
- Checked in the container through a silent PulseAudio sink, with the
  scrolling recorded and measured frame by frame (see the session log).
