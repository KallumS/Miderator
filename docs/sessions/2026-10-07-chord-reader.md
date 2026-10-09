# 2026-10-07 - ScaleView Pro's chord reader, re-vendored

Done from ScaleView for REAPER's repository, not in a Noterator session.

## Asked

ScaleView Pro had two chord-naming changes made and measured that day, and
the user asked for every repository using its code to take them, with
**ScaleView Pro as the reference** - not the VST3/CLAP plugin.

- An altered dominant on its own root keeps its alterations whatever its
  fifth: C7#5b9 reads `Caug7b9` (was `A#min9b5/C`), C7b5#9 reads `C7b5#9`.
- A draw goes to the reading that needs no slash, after the key and before
  the commoner quality: C D G Bb over C reads `C7sus2` (was `GminAdd11/C`).

## Done

The order follows the rule that copies are never edited here:

| | from | at |
| --- | --- | --- |
| `Source/Core/ScaleModel.h` | KallumS/ScaleView `Source/ScaleModel.h` | `da943f1` |
| `Engines/midi-suggester/` | KallumS/Midi-Suggester (`tools/sync_engines.sh`) | `6ed412b` |
| `Engines/midi-variator/` | KallumS/Midi-Variator (`tools/sync_engines.sh`) | `eb926a0` |

Good Idea, Midi Catalogue and Starting Blocks were synced too and came across
unchanged: they carry ScaleView's scales and roots, not its chord reader.

**Those three commits are on branches named `claude/amazing-brahmagupta-33qdfm`
in each repository, not yet merged.** Merge them first, or the hashes in
`Engines/VENDORED.md` point at work that is not on `main`.

## Checked

- The plugin's `ScaleModel.h` was diffed against `ScaleView Pro.lua` itself:
  60,212 voicings in nine keys, 541,908 names, byte-identical. This file is
  that one, byte for byte.
- A new `TestDetect` case for the four changed names failed on the old
  `ScaleModel.h` with the old names, and passes now. 66 tests pass, no
  warnings, and `tools/try_generators.lua` prints exactly what it did before.

## What it changes here

Chord names in the score. And through Midi Variator's engine, which changes a
chord from the root the reader finds, its chord-quality changes: suspended
chords now change like suspensions, and an altered dominant such as C7#5b9
becomes `DbminMaj7/C`, which is worse than before. That is recorded in Midi
Variator's CLAUDE.md as an open question for its rule table, and would arrive
here by the same sync.
