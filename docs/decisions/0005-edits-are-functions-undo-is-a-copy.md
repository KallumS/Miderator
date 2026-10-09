# 0005 - Every edit is a function in Edit.h; undo keeps the score from before

- **Date:** 2026-10-07
- **Status:** Accepted

## Context

Undo is usually where an editor's bugs live: each command records how to
reverse itself, and one that records it wrongly corrupts the score.

## Decision

The window never changes a note. It asks the `Controller`, which copies the
whole score onto the undo stack, calls a pure function from `Edit.h`, and lays
the page out again.

## Why

- A score here is a few thousand notes. Copying one is cheaper than the bugs a
  reversible-command design invites.
- Every edit is testable without a window (`Tests/TestScore.cpp`).
- Note input overwrites, as step-time input does everywhere: writing a quarter
  on beat two replaces what that voice had on beat two. Adding to a chord is the
  one input that does not.

## Consequences

- Three hundred steps of undo are kept. A huge score would want a cleverer
  store; nothing near that size exists yet.
