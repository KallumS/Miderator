# 0010 - Verify the engraving by rendering it

- **Date:** 2026-10-07

- **Status:** Accepted. Starting Blocks Notation's 0011, carried over.

## Context

An engraving bug is usually obvious on the page and invisible in the numbers:
a dot on a line instead of a space, a stem from the wrong head.

## Decision

`NoteratorRender` draws any score - the built-in test page, a .mid, a
.noterator - into a PNG through the same renderer the app uses, with no
window. Every drawing change is checked by looking at its render. The Linux CI
job renders the test page and keeps it as an artifact.

## Consequences

- The layout itself stays testable as values (`Tests/TestEngrave.cpp`); the
  render is what catches what the values cannot say.
