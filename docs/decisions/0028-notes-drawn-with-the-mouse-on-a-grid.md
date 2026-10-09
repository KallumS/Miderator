# 0028 - Notes are drawn, moved and stretched with the mouse, on a grid

- **Date:** 2026-10-09
- **Status:** Accepted - replaces 0016 here

## Context

Noterator writes notes the way notation programs do (0016): a caret, letters
A-G, numbers for note values, dots and triplets. A piano roll is written
with the mouse. Asked, the user chose to drop the notation keys and keep a
MIDI keyboard.

## Decision

- **Select** (the default): click a note to choose and hear it, Shift-click
  for more, drag on empty space to lasso, drag a note to move it in time
  and pitch (Alt drags a copy), drag either end to stretch it, double-click
  empty space to draw a note one grid step long, right-click for Delete,
  Duplicate and Quantise.
- **Draw** (D, or the toolbar): click to draw a note, drag to make it
  longer, click a note to delete it.
- **A grid** of 1 bar to 1/32 (keys 1-6), with triplets (T) and Snap. Grid
  lines are counted from each bar line, so they meet every bar line in any
  meter. A dragged note lands on the grid; the others selected keep their
  places around it.
- **A drawn note only replaces the same pitch where they overlap**
  (`roll::drawNote`), unlike step input, which overwrites the voice: a held
  note and a moving line can be drawn over each other.
- **Step input** (R): a MIDI keyboard writes a grid step at the caret, keys
  pressed together are a chord, and the caret moves on - Noterator's MIDI
  input, with the grid for a note value.
- The keys follow DAWs: Space plays, arrows move and transpose (Shift or Cmd
  for an octave), Cmd+D duplicates, Q quantises, Delete deletes.

## Consequences

- No typing of notes by letter. A MIDI keyboard or the mouse writes them.
- Every move, stretch, copy and velocity drag is one undo step: the views
  show the drag and ask the controller once, when the mouse is let go.
