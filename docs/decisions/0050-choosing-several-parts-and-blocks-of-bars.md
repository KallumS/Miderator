# 0050 - Choosing several parts by name, and blocks of bars with Cmd

- **Date:** 2026-10-10
- **Status:** Accepted (Noterator and Miderator alike). Refines 0019, 0041,
  0042 and 0044; Cmd+A changes meaning.

## Context

The user asked for the shortcuts a DAW has for choosing tracks: Cmd+A after
clicking an instrument chooses every instrument; Cmd+A with nothing chosen
chooses every note and every instrument; Shift and a click on another
instrument chooses every one between; Cmd and a click adds one at a time -
"same with selecting MIDI on the timeline", which they confirmed means bars.
Asked where an idea goes with several instruments chosen: shared across
them, **a single line to the instrument chosen first**, and to the top one
when Cmd+A chose them all at once.

## Decision

**Parts chosen by name** (`Controller::chosenParts`, in the order chosen):

- A plain click on a name chooses that part alone, as before (`choosePart`).
- **Cmd** and a click adds a part, or takes away one already chosen.
- **Shift** and a click chooses every part from the one clicked before to
  this one; the one clicked before comes first, so it takes the tune.
- **Cmd+A** with a part chosen chooses every part, the top one first, and
  clears the selected notes. With every part already chosen, or none
  (after Escape, 0048), it selects every note and chooses every part.
- An idea with several parts chosen and no bars goes to them alone, from the
  caret's bar (0041's sharing); a single line goes to the first chosen
  (`lineTarget`). The Generate tab says so: "chords to Violin II and Cello,
  one line to Violin II".
- Choosing by name lets go of chosen bars. Choosing one part again - a plain
  click, Alt+Up/Down, bars, a new part, a new or opened score - ends it;
  a note clicked, drawn or written in one of the parts chosen keeps it
  (`workIn`). Escape lets go of them with everything else.
- The tracks (Miderator), the staff names (Noterator) and the Parts tab
  mark every part chosen; the Parts tab takes Cmd and Shift too.

**Blocks of bars** (`BarRange::more`): **Cmd** and a click (or a drag) on
bars adds them to those chosen instead of replacing them; the same single
bar again with Cmd takes it away. Each block is filled on its own, the idea
from its first bar, once (0042), shared across that block's parts, a single
line to the caret's part among them or its top one. The blocks stay chosen
after Insert, like one block did. Good Idea makes ideas as long as the
longest block. Vary Notes and export use the bars from the first chosen to
the last.

## Consequences

- Cmd+A no longer always selects every note: usually a part is chosen, so
  the first Cmd+A chooses every part and a second selects every note.
- A part chosen by itself still means "the part in the roll / the caret's",
  and chords still go to every part then (0041); only several chosen narrow
  where an idea goes.
