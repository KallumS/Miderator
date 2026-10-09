# 0013 - AutoCC is computed for a whole part, not performed live

- **Date:** 2026-10-07
- **Status:** Accepted

## Context

AutoCC is a JSFX: it watches notes arrive and draws CC curves as they play.
Noterator knows every note in advance, and has to put the same curves in a
.mid file and an audio bounce as well as in playback.

## Decision

`AutoCC.cpp` ports the JSFX's four instrument presets value for value, its
envelope (`env_tick`: rise, settle, sustain, fall, with its curve shapes and
velocity scaling) and its rule for when a note restarts an arc
(`handle_event`, Legato mode), and runs them once over a part's notes in
seconds, sampling every 10 ms and keeping only the changes. The instrument
table chooses the preset (Strings, Brass, Woodwinds, Default, or none for
piano and percussion), and each part can switch AutoCC off.

## Consequences

- Playback, audio export and MIDI export get identical curves (0007), with
  the General MIDI exception in 0008.
- One-shot and ADR modes, custom lanes and the depth and time-scale controls
  exist in the code's options but have no controls yet. Drawable CC lanes are
  the next step, and AutoCC's curves become their starting point.
