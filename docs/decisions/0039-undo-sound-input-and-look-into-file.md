# 0039 - Undo, Sound, the input mode and the look move into the File menu

- **Date:** 2026-10-09
- **Status:** Accepted (Noterator and Miderator alike)

## Context

After 0037 and 0038 the user asked for more of the toolbar to go under File:
Undo and Redo, the light or dark look, Sound, and Step input (Noterator's
Note input).

## Decision

- Their toolbar buttons go. The File button's menu lists, after Export,
  **Undo** and **Redo** (greyed when there is nothing to undo), then the
  song's settings (0038), **Sound**, **Step input (R)** (ticked when on) and
  **Light piano roll and tracks** (ticked when on).
- The keys are unchanged: Cmd+Z, Shift+Cmd+Z, and the input mode's letter.
- The input mode is still shown when it is on: the status line turns yellow
  and says so, as it always has.
- The Mac's own menus at the top of the screen are unchanged; they already
  had all of these.

## Consequences

- The toolbar keeps the transport, Follow and the tools used while writing.
- Undo with the mouse takes two clicks; Cmd+Z is the same.
