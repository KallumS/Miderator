# 0006 - Instruments carry their context, and everything reads it

- **Date:** 2026-10-07
- **Status:** Accepted

## Context

"The DAW will know how a violin line differs from a viola one." That is range,
sweet register, clef, the voice it takes in harmony, how fast and how far it
moves comfortably, whether it breathes, how many notes it sounds at once,
what it transposes by, what it sounds like and how it swells.

## Decision

One table (`Instruments.cpp`), with Midi Catalogue's orchestra numbers
unchanged and clefs, transposition, a General MIDI program and an AutoCC shape
added. Everything reads it:

- the engraver chooses clefs and staves, writes transposing instruments
  transposed when asked, and flags what the instrument cannot do: notes out of
  range (red), outside its sweet register (grey), chords on a one-line
  instrument, notes faster than it plays at this tempo, leaps wider than it
  takes (a red mark, explained on hover);
- typing a letter into an empty part starts in that instrument's register;
- Midi Catalogue writes for the part's instrument directly, so the same
  settings give the violin and the viola different lines;
- anything a generator wrote for no particular instrument is moved by octaves
  into the part's sweet register when it is inserted;
- a generated bass under strings becomes a cello part, under brass a trombone;
- playback picks the sound, and AutoCC picks Strings, Brass or Woodwinds.

## Consequences

- Adding an instrument is one row. Renaming an id breaks saved files: ids are
  stored, names are not.
