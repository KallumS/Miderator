# 0008 - General MIDI playback gets AutoCC's CC7 and CC11 only; files get all four

- **Date:** 2026-10-07
- **Status:** Accepted

## Context

AutoCC drives CC1, CC11, CC7 and CC21 because orchestral sample libraries read
CC1 as dynamics. General MIDI reads CC1 as vibrato depth: AutoCC's swell
through Apple's synth is an ever-wider wobble.

## Decision

Playback and audio export send AutoCC's expression and volume lanes only. A
MIDI export carries all four, because a .mid is for a sample library.

## Consequences

- What you hear in the app is the swell without the vibrato; what a library
  gets is AutoCC as REAPER would have played it.
