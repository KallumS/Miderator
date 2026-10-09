# 0009 - Bravura for the symbols, where Starting Blocks Notation drew its own

- **Date:** 2026-10-07

- **Status:** Accepted. Departs from Starting Blocks Notation's 0004 on purpose.

## Context

Starting Blocks Notation draws every symbol from lines and polygons because a
ReaScript cannot count on the user having a music font.

## Decision

Bravura, the SMuFL reference font (SIL Open Font License), ships inside the
app and draws every symbol. Its published metrics (`Bravura.json`: stem
thickness, beam thickness, notehead width, stem anchors) are the layout's
numbers.

## Why

That reason does not hold for an app that carries its own resources, and a
notation program is judged on how its page looks. Glyphs are cached as paths
calibrated so a black notehead is exactly one staff space, which keeps the
font's size independent of how a platform measures fonts.

## Consequences

- The licence text ships beside the font (`Resources/Fonts/Bravura-OFL.txt`).
