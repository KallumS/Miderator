# 0044 - Clicking a part's name lets go of bars chosen elsewhere

- **Date:** 2026-10-09
- **Status:** Accepted (Noterator and Miderator alike). Refines 0019 and 0041.

## Context

The user chose bars in one part, inserted an idea, then clicked another
part's name to work on it - and could not insert anything there "until you
click off it". A name click only moved the caret; the chosen bars stayed
chosen, so Insert filled the old bars again, over what had just gone in,
and seemed to do nothing.

## Decision

A click on a part's name - a track's name, a staff's name, a row in the
Parts tab - goes through `Controller::choosePart`: the caret goes to that
part and, **if bars are chosen that do not include it, they are let go**.
The next idea then goes in as with nothing chosen (0041, 0042): a single
line into the part clicked, chords shared from the caret's bar. Bars chosen
that include the part clicked stay chosen - looking at one of the chosen
parts in the roll does not undo the choice.

## Consequences

- Clicking a name is how to say "this instrument now", whatever was chosen.
- To fill bars of the part clicked, choose them again in its track.
