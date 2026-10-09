# 0033 - The page follows the playhead, a page at a time, and can be switched off

- **Date:** 2026-10-09
- **Status:** Accepted (Noterator and Miderator alike)

## Context

While the score played, the page already jumped on when the playhead
reached the right edge - always, and only once it was nearly off the
screen. The user asked for an auto-scroll option, in Noterator and in
Miderator, that moves the view on before the music goes out of view.

Numbers 0026-0032 are Miderator's own; the two apps number their decisions
in one sequence so a number means one thing in both. This record is the
same in both repositories.

## Decision

- **A page at a time**, in `Source/Core/Follow.h`, shared by both apps: the
  view holds still while the playhead crosses it; when the playhead comes
  within a margin of the right edge (8% of the view, between 24 and 120
  pixels), the view jumps so the playhead sits that margin in from the
  left. A playhead off the screen - scrolled away from, or playing
  somewhere else - is gone to.
- **Follow** in the toolbar beside Play, and in the Play menu, switches it
  (and **F** in Miderator, where letters do not write notes). On by
  default, remembered between launches. In Miderator the tracks and the
  piano roll share one timeline, so both turn the page together.
- The rule is plain arithmetic, tested in the core: the container the work
  is done in has no sound card, so the playhead never moves there.

## Why

- Paging rather than scrolling smoothly is what notation programs and most
  DAWs do by default: music that holds still can be read.
- Off is wanted too: to look at, or edit, another passage while it plays.

## Consequences

- Turning the page a little before the edge means the last notes before
  the turn are always visible.
