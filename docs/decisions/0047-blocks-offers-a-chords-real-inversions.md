# 0047 - Blocks offers each chord the inversions it has

- **Date:** 2026-10-10
- **Status:** Accepted (Noterator and Miderator alike).

## Context

The Blocks toolbox offered Root, 1st, 2nd and 3rd for every chord. A triad's
"3rd" gave its 2nd again, and a ninth, eleventh or thirteenth could never put
its 9th, 11th or 13th in the bass. The user set out the rule: a triad has two
inversions (the 3rd, then the 5th, in the bass), a seventh three (then the
7th), and an extended chord one for each of its notes after the root.

The engine is Starting Blocks', vendored unchanged (0003), so the change was
made there (its decision 0007), copied into Starting Blocks Notation (its
0015), and vendored here from Notation's `79b71f4`.

## Decision

- The adapter's Inversion setting is `dynamic`: the chord's own list,
  `E.inversionNames` - Root to 2nd for a triad, to 3rd for a seventh, to 6th
  for a thirteenth, Root and 1st for a power chord. Changing the chord pins
  a chosen inversion to the new chord's last (`E.clampState`).
- An inversion puts the next note up the stack in the bass and lifts the notes
  under it by octaves until they sit above it.

## Checked

Every Blocks chord in all 288 keys, every family, type, degree and every
inversion it has - 735,822 chords, 65,574 of them 4th to 6th inversions that
did not exist before - through the real insert path: every one has the right
note in the bass, and every one reads in the Chords lane on Blocks' root over
that bass (0046). The lane test in `TestGenerators.cpp` now walks each
chord's own inversions in four keys.

Root position, and every inversion up to the 3rd of a chord no wider than an
octave, is the notes it was. A saved "3rd" on a triad loads as its 2nd, which
is what it sounded.
