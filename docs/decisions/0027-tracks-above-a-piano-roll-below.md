# 0027 - The tracks above, a piano roll below, on one timeline

- **Date:** 2026-10-09
- **Status:** Accepted - replaces 0004 for what is on the screen

## Context

Noterator lays every part out as a staff in one long galley (0004). A piano
roll can show one part well, or every part at once badly. The user chose,
from three sketches, the layout Cubase and Ableton use: a list of tracks with
each part's notes shown small, and a full piano roll for the track chosen.

## Decision

- **Tracks** (`ArrangeView`): a row per part with its name, family, mute
  and solo, and its notes drawn small, fitted between its lowest and
  highest note. Bar numbers, the Scale lane and the Chords lane run along
  the top, as in Noterator.
- **The piano roll** (`PianoRollView`) shows one part: the caret's part. A
  click on a track's name, a chosen bar, or Alt+Up/Down changes it.
- **One timeline** (`Timeline`): time is proportional, a quarter note the
  same width everywhere, and the tracks and the roll share one scroll bar
  and one zoom, with columns of the same width at either side, so a bar is
  directly above itself. A divider between them can be dragged.
- Bars are chosen in the tracks exactly as they were chosen on the page
  (0019): click a bar, drag across bars and parts, drag along the lanes for
  every part, Shift-click to stretch, Esc to let go.

## Why

- Choosing bars across several parts for Generate Notes (0019) needs every
  part on screen; editing notes needs one part large. This layout gives both.
- Proportional time is what every DAW shows, and what a piano roll needs:
  a note's length is its width.

## Consequences

- Only one part is edited at a time; the others show faintly behind it in
  the roll, so a line can be written against them.
- The engraver's shared columns (0004) still decide MusicXML export (0029).
