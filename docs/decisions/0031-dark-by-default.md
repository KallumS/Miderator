# 0031 - Dark by default, light on request

- **Date:** 2026-10-09
- **Status:** Accepted - replaces 0020 here

## Context

Noterator's page is black on white by default (0020), as paper is. A DAW is
dark, and the user chose a dark look by default.

## Decision

The tracks and the piano roll are dark, in the house greys, with **Light**
(toolbar, View menu) turning them light; the choice is remembered. The
chrome is dark either way. The house scheme holds (0015): yellow for what is
selected, chosen or on, red only for what an instrument cannot play, every
grey blue-shifted.

## Consequences

- The colours of both looks are one table, `theme::rollColours`.
