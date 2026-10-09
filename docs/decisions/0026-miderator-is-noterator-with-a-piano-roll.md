# 0026 - Miderator is Noterator with a piano roll, in a repository of its own

- **Date:** 2026-10-09
- **Status:** Accepted

## Context

Noterator shows the score as notation. The user asked for a version that
shows the same music on a piano roll, as Cubase and Ableton do, with every
other thing it does unchanged. Decisions 0001-0025 are Noterator's and came
across with its code; their numbers are kept so the code's references still
point somewhere.

## Decision

- **A copy, not a mode.** Miderator starts from Noterator at `1cb4015` in its
  own repository (KallumS/Miderator), and replaces only what is about
  notation: the page, its renderer, the music font, and note input by
  letters and note values.
- **The music underneath is shared, byte for byte.** The score, editing,
  instruments, templates, detection, AutoCC, performance, files, MusicXML,
  the generator bridge, the engines and their adapters, the Generate tab,
  the export code and the core tests are Noterator's files, unchanged.
  `tools/sync_from_noterator.sh` copies them across from a Noterator
  checkout (or, with `--check`, lists the ones that differ), and
  `docs/SHARED.md` records which Noterator commit they match.
- What the piano roll needs from the music lives beside them, in
  `Source/Core/Roll.*`, so the shared files never need editing here.
- A project is a `.miderator` file in Noterator's own format: each app opens
  the other's projects.

## Why

- Notation is a third of Noterator's code and woven through its window; a
  switch between the two views in one app would double every mouse rule.
  Two apps on one core keep each simple.
- The music core was written without a page in it (0002): the score is MIDI,
  so a piano roll is just another way of drawing it.

## Consequences

- A fix to the shared code is made in Noterator first, then copied here, the
  way the engines are copied from their own repositories (0003). Editing a
  shared file here means the copies drift; `--check` shows it.
- `AudioEngine.cpp` differs by one name (the built-in synth is
  "Miderator's"), so it is not in the shared list.
