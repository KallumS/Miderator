# 0030 - A velocity lane under the roll, and quantise

- **Date:** 2026-10-09
- **Status:** Accepted

## Context

Offered DAW extras for the first version, the user chose a velocity lane,
quantise and a dark look (0031), and left drawable CC lanes for later.

## Decision

- **Velocity lane**: a stem per note under the roll. A drag paints every
  stem the pointer crosses to its height; a drag on a selected note's stem,
  with more than one selected, moves all the selected by the same amount.
  Louder notes are also drawn stronger in the roll.
- **Quantise** (Q, the toolbar, the right-click menu): starts and ends to
  the nearest grid line, never shorter than a step, notes landing on the
  same pitch and start merged. With nothing selected, the whole part in the
  roll - as a DAW quantises a clip.
- **Duplicate** (Cmd+D): a copy straight after the selection, rounded up to
  the grid.

## Consequences

- Velocity was already in the score and in every file; only now can it be
  seen and changed.
