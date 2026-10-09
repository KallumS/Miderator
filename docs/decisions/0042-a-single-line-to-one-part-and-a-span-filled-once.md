# 0042 - A single line goes to one part; chosen bars are filled once

- **Date:** 2026-10-09
- **Status:** Accepted (Noterator and Miderator alike). Refines 0041; replaces
  0019's "repeated until it fills them".

## Context

Trying 0041, the user asked for two more rules. A melody or motif - one
line - was shared across every part (played in octaves by everyone), which
is not what one line is for. And chosen bars longer than the idea had the
idea repeated to fill them.

## Decision

- **One line is never shared out** (`isSingleLine`: one pitched part, no
  chords, no drums). With nothing chosen it goes into the caret's part; with
  bars of several parts chosen, into the caret's part among them, or the top
  one (`Controller::lineTarget`). Chords, a tune with chords, and a tune with
  a second voice are shared; a tune and a second voice split - the upper
  parts the tune, the lower the second voice.
- **Chosen bars are filled once** (`fitToSpan`): an idea longer than the bars
  is cut where they end; one shorter plays once and the rest of the bars are
  left empty. Nothing goes outside the chosen bars.
- The Generate tab says both: "Into: from bar 3 - chords to every part, one
  line to Violin I".

## Consequences

- Twelve bars chosen for an eight-bar idea: eight bars filled, four empty
  (cleared, in the parts that take music).
- To fill a long stretch with one idea twice, insert it twice.
