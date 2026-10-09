# 0002 - The score is MIDI-shaped; notation is worked out from it

- **Date:** 2026-10-07
- **Status:** Accepted

## Context

A notation program can store either the notation (measures holding chords
holding notes with written durations) or the performance (notes with a start,
a length and a pitch) and derive the other. Most store the notation. Here the
bulk of the music arrives from generators as MIDI, and has to be edited as
notation and leave as MIDI again.

## Decision

A part is a list of notes in ticks (960 to the quarter) and sounding pitches.
Measures, ties, beams, rests, voices, spellings and clefs are never stored:
`Engrave.cpp` works them out from the notes every time the page is drawn.

## Why

- A generator's output goes in unconverted, and a MIDI export is exact,
  because there is only one copy of the music.
- An edit on the page is an edit to the MIDI. There is no second
  representation to fall out of step.
- It is how Dorico works underneath (rhythmic positions, notation derived), and
  it makes changing a time signature or a transposition free: the page is
  drawn again around the same notes.

## Consequences

- The engraver has to make decisions a stored score would have recorded:
  which notes form a chord, how a gated note is written, which voice a
  sustained bass belongs to. Where it cannot know, a note carries the one
  explicit hint it needs (`voice`).
- Anything the MIDI does not hold - articulations, dynamics, slurs - will need
  a field of its own when it comes. That is fine: it is data on a note, not a
  second score.
