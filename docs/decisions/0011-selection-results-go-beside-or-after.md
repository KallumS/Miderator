# 0011 - Results made from a selection go beside it or after it, never over it

- **Date:** 2026-10-07
- **Status:** Accepted

## Context

Most generators write into the part the caret is in, at the caret's bar. Two
work on music the user has selected: Midi Suggester (chords for a tune, a tune
for chords) and Midi Variator (variations of a passage). Written into the
caret's part, a Suggester result replaced the very tune it was made for.

## Decision

- **Midi Suggester**: the result starts at the selection's first bar and every
  line gets a part that is silent there - an existing one on the right
  instrument if there is one, a new one otherwise. A line with chords never
  goes to an instrument that plays one note at a time: under a string quartet's
  tune, the bass goes to the cello and the chords to a new piano part.
- **Midi Variator**: the variation goes straight after the selected bars, in
  the same part, the way the REAPER script makes a new item beside the old.
- Auditioning either plays it in place with the music around it, so chords are
  heard under their tune.

## Consequences

- A result never overwrites the music it came from. Undo still takes it out.
