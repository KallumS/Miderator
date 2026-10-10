# 0051 - No More button in the Generate tab

- **Date:** 2026-10-10
- **Status:** Accepted (Noterator and Miderator alike). Replaces the More
  button of 0017.

## Context

The Generate tab had Generate, More, Insert and Stop. More added six more
ideas to the list with the same settings. The user found it useless: for
more ideas they press Generate again - and since 0049, Cmd+Z brings back the
list a Generate replaced, so nothing is lost by it.

## Decision

More is gone. The tab has Generate, Insert and Stop, a third of the width
each. Every Generate makes a fresh list with a new random seed.

## Consequences

- A long list built up with More is no longer possible; earlier lists are
  reached with Undo instead (0049).
