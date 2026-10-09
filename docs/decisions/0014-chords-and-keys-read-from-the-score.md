# 0014 - Chord and key lanes: ScaleView names, Suggester's key finder, and only real harmony named

- **Date:** 2026-10-07
- **Status:** Accepted

## Context

The brief asks for chords and scales detected along the timeline. The family
already has the answers: ScaleView names any set of notes, and Midi Suggester
finds a key from notes. What neither does is decide where one chord stops and
the next begins across a whole score.

## Decision

- **Where:** beat by beat, a template fit (triads and sevenths, less what each
  leaves out), merging beats while the best chord holds.
- **What it is called:** `scaleview::chordName`, unchanged, on the notes that
  sound through at least a fifth of the span plus the chord's own tones, the
  lowest note kept as the bass so inversions read as slash chords.
- **Only harmony is named.** A segment gets a name where notes sound together,
  or where three or more pitch classes all belong to the chord found (an
  arpeggio). A tune on its own is not "C D" and a scale run is not a chord.
- **The key:** Suggester's `detectKey` with its weights, over the piece and in
  eight-bar windows; a change has to last four bars. The written key signature
  adds 0.12 to its own key, which settles near ties - four notes of C major
  are as much A minor - without overruling music that is clearly elsewhere.

## Consequences

- Melody notes held over a chord count as its extensions, so a tune's long A
  over C reads C6. That is a true reading of everything sounding; a mode that
  names the accompaniment only is a possible later option.
- The lanes are recomputed on every edit, from the whole score.
