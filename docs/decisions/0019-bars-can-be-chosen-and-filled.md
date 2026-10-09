# 0019 - Bars can be chosen on the page, and generated music fills them

- **Date:** 2026-10-07
- **Status:** Accepted; how a result is shared across the chosen parts replaced by 0040 and 0041

## Context

A generator wrote into the caret's part from the caret's bar, for as long as
its result happened to be. The user asked to drag across a number of bars
and generate into exactly that selection - the way Dorico and MuseScore let
you select a passage and act on it.

## Decision

- The controller holds a **bar range**: first and last bar and the parts it
  covers, top to bottom. While it is set, the selection is every note in it.
- On the page: a click on empty paper chooses that bar in that part; a drag
  chooses more bars and more parts; a drag along the lanes chooses every part;
  Shift and a click stretches the choice. Shift and a drag still picks out
  notes (MuseScore's lasso). Escape lets go. The range is drawn as a yellow
  tint with an outline.
- **Good Idea fills the range exactly.** Its length setting, when Any, becomes
  the longest it has that fits; the result is then repeated or cut to the span
  (`fitToSpan`). With one part chosen, everything goes there. With several,
  `insertIntoRange` orchestrates: the tune to the top part, the bass to the
  bottom, chords to the parts between - whole to one that plays chords, or
  dealt out one note to a part (`spreadChords`) where none does - and drums to
  a kit. Only the parts that receive music are cleared.
- **Midi Variator with bars chosen replaces them** with the variation, in the
  part it came from. Without a range it still goes after (0011). Suggester is
  unchanged: beside its source.
- After an insert the bars stay chosen, so another idea can go straight in.

## Consequences

- Generated music can be placed anywhere and at any length without editing.
- A four-bar phrase into five bars repeats its first bar; the alternative,
  asking the engine for a length it does not offer, would change the engine.
