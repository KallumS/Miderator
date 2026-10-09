# 0025 - In the app, the generators are named for what they do

- **Date:** 2026-10-07
- **Status:** Accepted

## Context

The Generate tab listed the engines by their own names: Good Idea, Midi
Suggester, Midi Variator. Those mean something to whoever wrote them in
REAPER, not to someone opening a notation app. The user asked for **Generate
Notes**, **Suggest Notes** and **Vary Notes**.

## Decision

- The adapters' display names change; nothing else does. The engines, their
  files, their ids (`good-idea`, `midi-suggester`, `midi-variator`) and the
  code and docs keep the engines' own names, because those are the names of
  the repositories they are copied from (0003).
- Good Idea titles its ideas "Good Idea 63797 - Phrase (melody), 4 bars,
  C Major" - its REAPER file name, with the seed. The adapter drops the
  "Good Idea 63797 - " so a result reads "Phrase (melody), 4 bars, C Major".
- Starting Blocks keeps its name inside the Blocks tab's code; the tab itself
  was already called Blocks.

## Consequences

- Docs for developers say Good Idea, Suggester, Variator; docs for the user
  say Generate Notes, Suggest Notes, Vary Notes. CLAUDE.md says which is which.
- A result's seed no longer shows. It was only useful for finding the same
  idea again in REAPER.
