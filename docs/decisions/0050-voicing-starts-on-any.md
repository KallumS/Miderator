# 0050 - Generate Notes' Voicing starts on Any

- **Date:** 2026-10-21
- **Status:** Accepted (Noterator and Miderator alike)

## Context

Good Idea starts its Voicing on Close. The user asked for Any to be the
default, so each idea chooses its own voicing (Close still the likeliest,
three in nine, then Open, Drop 2, Drop 3, Drop 2 & 4, Shell and Rootless).

## Decision

The adapter's `newState` sets `voicing` to Any after taking Good Idea's own
defaults. `gi_idea.lua` is not edited (0003): the engine is Good Idea's, and
in REAPER it still starts on Close. Every other setting keeps the engine's
default.

## Consequences

- Shared code: made in Noterator, copied to Miderator.
- `TestGenerators.cpp` checks Voicing reads Any for a Measure (the kind with
  chords, where Voicing shows) and that it still makes ideas.
