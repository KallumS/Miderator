# 0048 - Escape lets go of everything, the caret's part too

- **Date:** 2026-10-10
- **Status:** Accepted (Noterator and Miderator alike). Refines 0041 and 0042.

## Context

The user wanted to insert a generated idea onto every instrument at once,
and found no way to stop one instrument being "the" instrument. Escape let
go of the selected notes and the chosen bars, but the caret's part stayed
marked - its track (Miderator) or staff name (Noterator) lit, its row in the
Parts tab lit - so it looked as if the next idea would go to it alone, and a
single line (0042) did. Escape also did only one thing at a time: stop
playing, or else end step/note input, or else clear the selection.

## Decision

**Escape lets go of everything** (`Controller::letGoOfEverything`): playing
and step/note input stop, the selection and the chosen bars are cleared, and
the caret's part is let go (`Controller::noPartChosen`). No part is marked,
and the status line says "Nothing chosen".

With no part chosen the next idea goes in as with nothing chosen (0041):
shared across every part from the caret's bar. **A single line goes to the
top part** (`lineTarget`) - the user chose this over sharing a melody with
every instrument, which 0042 had ruled out. Inserting keeps nothing chosen,
so one idea after another can go to every part.

A part is chosen again by anything that picks one: its name clicked
(`choosePart`), Alt+Up/Down, bars chosen, a note clicked or drawn or
written, a part added, a new or opened score. Moving the caret along the
ruler does not choose one: it only says where the next idea starts.

The caret's part still exists underneath - Miderator's roll goes on showing
it, a block from the toolbox still goes into it at the caret (the Blocks tab
says which) - it is only no longer chosen. Noterator draws the caret down
every staff while no part is chosen.

## Consequences

- Escape while playing now also clears the selection; Space alone stops
  playing and keeps it.
- To send a melody to one instrument after Escape, click its name.
