# 0021 - MusicXML in and out, from the core, with a parser of our own

- **Date:** 2026-10-07
- **Status:** Accepted

## Context

MusicXML is how scores move between Dorico, MuseScore, Sibelius, Finale and
Notion. The user asked for import and export. JUCE has an XML parser, but the
core (`Source/Core`) has no JUCE in it, which is what makes it testable in
seconds; putting the format in the app would put it out of reach of the core
tests.

## Decision

- A small XML reader and writer in the core (`Xml.*`: elements, attributes,
  text, entities, CDATA, comments and DOCTYPE skipped) and the format on top of
  it (`MusicXml.*`).
- **Export is written from the engraver's layout**, not re-derived: the notes,
  ties, beams, tuplets, voices, clefs and spelled accidentals are the ones on
  the page. Transposing instruments are written at written pitch with a
  `<transpose>`, as MusicXML expects; drums as unpitched notes with an
  instrument per kit piece. Whole score or the selected bars.
- **Import** reads partwise files (what every program writes), including
  `.mxl` - the zip is opened in the app with JUCE and the inner file handed to
  the core. Parts become instruments by name, then by MIDI program; a pickup
  bar is moved to the end of bar one; ties join into one note; grace and cue
  notes are skipped.

## Consequences

- Round trips are tested in the core. An exported file has been checked with
  music21 and a Bach chorale imported from it.
- Timewise files are refused with a reason; so is anything that is not
  MusicXML. Dynamics, articulations, lyrics and text are not carried yet -
  the score has nowhere to keep them.
