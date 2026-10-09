# 0024 - The Blocks toolbox offers chords, arpeggios, runs and intervals only

- **Date:** 2026-10-07
- **Status:** Accepted - narrows [0018](0018-starting-blocks-is-a-toolbox.md)

## Context

The Blocks toolbox (0018) offered all six of Starting Blocks' kinds: chord,
arpeggio, run, interval, bass note and drum. Using it, the user asked to keep
drums in Good Idea, where a groove is generated whole, and to take Drums and
Bass out of Blocks. A single repeated bass note or a single drum is not a
useful hand-picked block next to what Good Idea writes for both.

## Decision

- The Starting Blocks adapter offers the engine's kinds less Bass and Drums;
  their settings are gone from the adapter, and a saved state that names
  either falls back to Chord. **The engine is unchanged** (0003): it still
  makes both, and nothing here calls them.
- The toolbox's kind buttons are one row: Chord, Arpeggio, Run, Interval.
- Good Idea keeps **Make a: Drums**; drum grooves and bass lines come from it.

## Consequences

- If single drums or bass notes are wanted back, it is the adapter's kind
  list and two buttons; nothing was deleted from the engine.
