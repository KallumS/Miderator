# 0018 - Starting Blocks is a toolbox of its own, not a generator

- **Date:** 2026-10-07
- **Status:** Accepted; Bass and Drums later taken out by [0024](0024-blocks-without-drums-and-bass.md)

## Context

Starting Blocks was a fifth entry under Generate, producing a list of results
like the others. In REAPER it is something else: a palette. You pick a kind
of block (chord, arpeggio, run, interval, bass note, drum), see every degree
of the key as a button, click one and it lands at the edit cursor, and the
cursor moves on. The user asked for it to work that way here, separate from
Generate.

## Decision

- A **Blocks** tab beside Generate (`BlocksPanel`). Six buttons choose the kind
  (the engine's "Melody" is shown as **Interval**, the user's word for it).
  Every degree of the score's key at the caret is a button showing its numeral
  and note. Clicking one draws it (a one-staff score through the same
  engraver and renderer as the page) and plays it; **Insert** or a
  double-click puts it in.
- A block goes **at the caret, not at the bar's start** - unlike a generated
  idea, which always starts on a downbeat - and the caret moves past it, so
  blocks can be laid one after another. With bars chosen (0019) it fills them
  instead.
- The adapter now offers **every chord family** of Starting Blocks (Triads,
  6ths & 7ths, Extended, Altered, Sus & Add, Quartal, Named) as well as the
  key's own chords; a family's chords are built on each degree.
- The settings menus are one component (`SettingsList`) shared with the
  Generate tab, still drawn from what the adapter says it has.

## Consequences

- The engine is unchanged (0003); only its adapter and a panel are new.
- A block always follows the score's key. The REAPER version lets you pick a
  key of your own; here a key change in the score does that.
