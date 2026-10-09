# 0004 - One set of columns for every staff, in one long system

- **Date:** 2026-10-07
- **Status:** Accepted

## Context

The brief: notes line up vertically when they sound together and horizontally
with time. Starting Blocks Notation laid out one block on one or two staves.

## Decision

Each measure is spaced once, from every onset on every staff of every part, so
simultaneous notes share an x. The page is a single long system - a galley, as
Dorico calls it and as a DAW's timeline is - with the names, clefs and key held
at the left while the music scrolls.

## Why

- Shared columns are the only way a chord split between a flute and a cello
  reads as one moment.
- A galley maps time to x, which is what a playhead, a chord lane and a click
  at a beat all want, and it is what someone coming from a DAW expects.

## Consequences

- There is no page view yet. Paged layout (systems, margins, a title) is the
  next layout job and is mostly a matter of breaking this one system.
- Spacing is not proportional: each doubling of a duration adds a fixed step
  of space, as engravers space, so a whole bar of semibreves is not mostly air.
