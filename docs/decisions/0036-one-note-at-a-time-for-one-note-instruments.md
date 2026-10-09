# 0036 - Generated music gives an instrument no more notes at once than it plays

- **Date:** 2026-10-09
- **Status:** Accepted (Noterator and Miderator alike)

## Context

The user found chords generated for instruments that play one note at a
time. The instrument table already says how many notes each plays at once
(0006): piano and guitar 6; harp, celesta, vibraphone, marimba and kit 4;
glockenspiel, xylophone and timpani 2; every string, wind, brass and voice
part 1. But only the extra lines of a result were checked against it, when
they were given parts of their own (0011). Measured over 168 Good Idea
results into a string quartet:

- a **Phrase of chords** put chords on the violin every time the caret was
  in it (24 of 24): the first line of a result always went into the caret's
  part, whatever it was;
- **across chosen bars**, chord phrases gave a string part two notes at once
  in 30 of 48 results: the chords were dealt out one note to a part
  (`spreadChords`, 0019), but held chord notes ran into the next chord;
- motifs, melodic phrases, measures and second voices were already fine.

## Decision

- **`fitToPolyphony`** (`Generators.*`) fits a line to an instrument: where
  more notes start together than it plays, it keeps the top ones - the tune -
  or the bottom ones for an instrument that takes the bass (role B); a note
  still sounding when the next starts is shortened to end there, so a line
  stays legato. Drums are left as they are: a kit's hits are not a chord.
- **Every line is fitted to the part it lands in**, wherever it lands:
  the caret's part, chosen bars, a part of its own, a variation in place.
  (`InsertOptions::fitPolyphony`, on by default.)
- **Blocks are left as they were**, at the user's word: a block from the
  toolbox goes in as it is.
- When a part was given fewer notes than the idea had, the status line says
  so: "Violin I: top notes only".

## Consequences

- The same measurement afterwards: 0 of 168 in every case.
- A chord phrase into a violin is now its top line. To keep the chords,
  choose bars across several parts (they are dealt out) or put the caret in
  a piano part.
- The check runs on what generators make, not on what the user writes by
  hand: a chord typed onto a violin is still allowed, and still marked (0006).
