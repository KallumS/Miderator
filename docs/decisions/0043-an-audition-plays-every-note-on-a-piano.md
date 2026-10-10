# 0043 - An audition plays every note, on a piano

- **Date:** 2026-10-09
- **Status:** Accepted (Noterator and Miderator alike). Narrows 0036 (fitting
  is for what goes in, not for what is heard first) and 0041's audition.

## Context

The user found that auditioning Generate Notes, and the Blocks preview, had
become one note at a time. Since 0036 every line is fitted to the part it
goes to, and the audition played the result as it would be placed: chords
into a violin were heard as the violin would get them, one note. The Blocks
preview was fitted to the caret's instrument too, though the block itself
goes in as it is (0036's one exception). The user asked for every note back,
and for auditions always to sound on a piano rather than the chosen
instrument.

## Decision

- **An audition is the result itself** (`addAudition`, shared): every note of
  every line, on a piano; drums on a kit. Nothing is fitted to an instrument
  or moved into a register. It is soloed if any part is, so it is always
  heard.
- Generate Notes is still heard **where it would go** (0041), with the music
  around it; the parts it would go to are silent there, and the piano plays
  it, cut where it would be cut (`Controller::auditionScore`).
- A block is heard **on its own** (`Controller::auditionAlone`); the Blocks
  preview draws it as it goes in, thinned for no instrument.
- What goes in is unchanged: still fitted to each part (0036, 0040-0042).

## Consequences

- What is heard first is the idea as it was made, not one instrument's share
  of it; after Insert, playback is the instruments themselves.
- A line that will be moved an octave to suit its part is auditioned where
  it was written.
- The note-at-a-time previews (clicking a note, the caret's part when
  selecting) still use the part's own instrument: they are about the part,
  not a generated idea.
