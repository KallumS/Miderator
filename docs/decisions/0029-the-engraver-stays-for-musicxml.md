# 0029 - The engraver stays, unseen, for MusicXML; the music font goes

- **Date:** 2026-10-09
- **Status:** Accepted - 0009 no longer applies here; 0021 stands

## Context

Nothing on Miderator's screen is notation, but the user asked to keep
MusicXML import and export, for Dorico, Sibelius and MuseScore. Noterator
writes MusicXML from the engraver's layout (0021): its note values, ties,
beams, tuplets, voices and spelled accidentals.

## Decision

- `Engrave.*` stays in the core, unchanged and shared (0026), and runs only
  when MusicXML is exported. Its tests stay too.
- It no longer runs on every edit: the controller re-reads the chords, the
  keys and the instruments' warnings (0032), and that is all.
- The page's renderer and Bravura (0009) are gone from the app.

## Consequences

- A MusicXML export from Miderator is the same file Noterator would write.
- Edits are cheaper than in Noterator: no layout on every change.
