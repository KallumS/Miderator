# 0046 - Blocks chords keep the root they were made on

- **Date:** 2026-10-09
- **Status:** Accepted (Noterator and Miderator alike). The Chords lane (0014)
  still reads everything else with ScaleView Pro's reader.

## Context

The user found the Chords lane disagreeing with the Blocks toolbox over
inversions, and asked for every Blocks chord in every key to be checked,
Blocks being right wherever the two disagree. Measured: every Blocks chord in
all 288 keys, eight families, every type, degree and inversion - 670,248 -
through ScaleView's reader. Not one name claimed notes that were not played,
but 48% came back on a root other than the one Blocks built them on. And
19% of all of them **cannot** come back right from the notes alone: they are
note for note, bass included, another Blocks chord on another root. vi7 in
first inversion is C E G A over C, exactly I6; Blocks' "7th on C" and
"6th on E, third inversion" are both C E G B over C.

## Decision

A chord from Blocks carries its root into the score. The adapter reports it
(`root`, a pitch class, for the Chord block only - an arpeggio is a line and
is read like one); insert records it (`ChordRoot`: a span, the root, the
chord's pitch classes - saved in the project, moved and dropped with bars);
the lane names the notes sounding there **from that root**, in ScaleView's
own words (`nameFromRoot`: ScaleView's reading from that root, its bass after
a slash), for as long as they are still exactly the chord's pitch classes.
Rests after it go on holding its name, as they hold any chord.

## Consequences

- Every one of the 670,248 reads on Blocks' root and bass, each name
  accounting for exactly the notes (checked with ScaleView's own
  `check_symbol.py`). Four keys of that are a core test.
- A Blocks chord edited - a note moved, added or taken away, or another part
  sounding with it - is read by the reader like any other music.
- Music that did not come from Blocks is untouched by this; making the reader
  itself agree with Blocks is ScaleView Pro's work, done there first.
