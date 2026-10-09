# 0017 - Midi Catalogue leaves the Generate tab; Good Idea leads it

- **Date:** 2026-10-07
- **Status:** Accepted

## Context

The Generate tab listed all five engines. Testing the first version, the user
found the Catalogue redundant there: Good Idea already writes better motifs
and phrases, and two menus that both say "make me a line" are one too many.
They asked for Good Idea as the main generator, Midi Suggester as the backup
when they want ideas for chords or a melody they already have, and Midi
Variator to change music already generated.

## Decision

- Each adapter says which panel it belongs to (`panel = "generate"`,
  `"toolbox"`, or nothing). The Generate tab lists only `"generate"`: Good
  Idea first, then Suggester, then Variator.
- **The Catalogue is still loaded**, unlisted. Its instrument table already
  lives in C++ (0006), and the core tests and `NoteratorRender generated` still
  use it, so it stays a working, tested engine that can come back if wanted.
  Nothing in the app depends on it.

## Consequences

- The menu is three items, in the order a user should reach for them.
- A future engine chooses where it appears in its adapter, not in C++.
