# 0052 - Vary Notes on several parts goes after the music, across those parts

- **Date:** 2026-10-10
- **Status:** Accepted (Noterator and Miderator alike). Refines 0011 and 0019
  for Vary Notes; one part's chosen bars are still replaced by a variation.

## Context

The user chose bars 1-4 across every instrument and pressed Insert in Vary
Notes, expecting a variation of the passage after bar 4. Instead it replaced
bars 1-4 of the top instrument with the whole ensemble's notes squeezed into
it: the variator reads every selected note as one passage, and the result
went into one part. With the shortcuts of 0050 they could not see how to make
it act "as if the instruments were chosen".

## Decision

When the music Vary Notes works on is in **more than one part** - bars chosen
across several parts, or notes selected in several - the variation goes
**after the last bar of it**, from the next bar line, **shared across those
parts** as an idea is shared across parts chosen by name (0040, 0050:
`insertIntoRange`, orchestrated by section and register). The bars it came
from are left as they are (`Controller::variesAcrossParts`, at the top of
`Controller::place`). The Generate tab says "Into: after bar 4, across the
4 parts it varies"; the audition plays it there.

One part's music behaves as before: its chosen bars are replaced by the
variation (0019), its selected notes get the variation after them (0011).

## Consequences

- Like any shared insert (0041), the variation replaces what those parts had
  for its length after the bars; one Undo puts it back.
- The variation is one passage shared out, not each instrument varied on its
  own: the variator sees the parts together.
