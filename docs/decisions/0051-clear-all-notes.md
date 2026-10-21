# 0051 - Clear All takes every note out

- **Date:** 2026-10-21
- **Status:** Accepted (Noterator and Miderator alike)

## Context

The user asked for a button to clear the timeline: to start again on the
same ensemble without making a new score.

## Decision

**Clear All** in the toolbar, at the right, and **Clear All Notes...** in
the Edit menu and the File button's menu. Every note goes from every part,
with the chord roots Blocks recorded (0046), since they belong to the notes.
The parts, their instruments and settings, the tempo, keys, time signatures
and bars stay. It asks first, with the Mac's alert, as removing a part with
notes in it does (0048); it is one step to undo, and the status line says
so. With no notes it says there is nothing to clear.

## Consequences

- `Controller::clearAllNotes`; `TestApp.cpp` checks it and its Undo.
- File > New is still the way to a different ensemble.
