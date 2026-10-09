# 0032 - What an instrument cannot play is read from the notes

- **Date:** 2026-10-09
- **Status:** Accepted

## Context

Noterator's warnings (0006) - out of range, outside the sweet register, a
chord on a one-line instrument, too fast, too wide a leap - are worked out
by the engraver, element by element. Miderator does not engrave (0029).

## Decision

`roll::check` reads them straight from each part's notes: notes that start
together are one onset; more of them than the instrument has is too many;
two onsets closer in seconds than it manages are too fast; a single note
leaping further than it takes from the single note before is too wide; a
rest of an eighth or more starts the line again, as a rest does on a page.
Drums have none. The controller re-reads them on every change.

In the roll a note out of range is red, outside the sweet register grey, and
anything else carries a red corner; the tooltip says why. Beside the keys a
bracket shows the instrument's range, thick where it sounds best.

## Consequences

- Very nearly the engraver's answers, from much less work; where they differ
  it is on music the engraver would have split into voices.
