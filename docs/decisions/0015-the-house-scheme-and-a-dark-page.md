# 0015 - The house colour scheme, a dark page by default and a light page on request

- **Date:** 2026-10-07
- **Status:** Accepted; the dark default is replaced by [0020](0020-light-page-by-default.md)

## Context

Every app in the family shares one colour scheme (Good Idea's
`docs/COLOUR.md`). Starting Blocks Notation set its page in near-white ink on
dark paper and offered a light page (its 0012).

## Decision

`Theme.*` carries the scheme: the blue-shifted grey ramp (R < G < B on every
grey), light grey controls with dark ink on every button and tab, one yellow
for what is on, red only for warnings. The page follows Starting Blocks
Notation: dark by default, black on white with **Light page**, the accent
shaded down on white. The yellow is spent on the selection, the sounding
note, the caret in note input and the chosen button; red marks what an
instrument cannot do.

## Consequences

- A notation user may expect white paper; it is one click, and the setting is
  a view preference, not part of the score.
