# 0049 - Undo brings back the ideas a Generate replaced

- **Date:** 2026-10-10
- **Status:** Accepted (Noterator and Miderator alike).

## Context

The user had an idea they liked in the Generate tab's list, pressed Generate
once more before inserting it, and lost it: each Generate replaced the list,
and nothing could bring the old one back. They asked whether Undo could.

## Decision

**A Generate (or More) is a step in Undo, among the edits, in the order they
were made.** Cmd+Z right after a Generate brings back the list from before
it, with the same result chosen; Cmd+Z again undoes whatever came before
that, a Generate or an edit. Redo goes forward the same way. A Generate after
an Undo drops what Redo had, as an edit does.

- The controller's undo stack holds `Step`s: the score before an edit, or a
  mark for a Generate (`Controller::generated`). Undoing a mark asks the
  Generate tab, through `Controller::stepResults`, to show the list before;
  the score is not touched and the song is not marked changed.
- The Generate tab keeps every list it made (`GeneratorPanel::history`,
  up to 300 like the undo stack), with the generator that made it, its seed
  and the row chosen. A list from another generator brings that generator
  back with it, so Insert places the idea as it would have.
- A list brought back is not auditioned: Undo is quiet. Click the idea to
  hear it.

Kept in the controller's one undo stack rather than a Back button in the tab
because the user asked for Undo, and one Cmd+Z that always means "the last
thing I did" is easier to trust than two histories.

## Consequences

- Undoing an insert now may first undo the Generates made after it.
- Choosing another generator still empties the list without a step; Undo
  then goes to the list before the last Generate, and Redo to that one.
- A new or opened score clears Undo, so lists from before it cannot come back.
