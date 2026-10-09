# 0038 - The Score tab's settings move into the File menu

- **Date:** 2026-10-09
- **Status:** Accepted (Noterator and Miderator alike)

## Context

After the File button (0037) the user asked for the Score tab's contents to
go into it too: the side panel had four tabs, and the Score tab was
visited rarely - a title, a tempo, signatures, bars and the sound, set once
or twice in a piece.

## Decision

- The **Score** tab is gone. Under **File**, after Export, a section headed
  **This song** holds what it had:
  - **Title and composer...** and **Tempo (N bpm)...** open a small box to
    type in (a tempo outside 30-240 is refused with a message, as the old
    slider's ends did).
  - **Time signature at bar N (n/d)** is a sub-menu of the common ones,
    the current one ticked, and **Other...** for any other (a box with two
    lists, as the tab had).
  - **Key at bar N (...)** is a sub-menu: **Use the key it hears** first,
    then each root with its scales.
  - **Bars (N in all)**: insert a bar at the caret, delete the chosen bars,
    add four at the end.
  - **Sound**: the two synths and **Audio and MIDI devices...** - the same
    as the toolbar's Sound button, which stays.
- Signatures are set at the caret's bar, as the tab's buttons did. Each menu
  item names that bar and what is there now, so the menu shows the state the
  tab used to.
- The Mac's own File menu at the top of the screen gets the same section.
- "Use the key it hears" moved from the panel into
  `Controller::useHeardKey`, so it is tested.

## Consequences

- The side panel is Generate, Blocks and Parts only.
- Title and tempo take a click more (open the box); everything else is as
  quick as it was.
- Menu ids are now in several ranges (`MainComponent.cpp`): a range check
  must name its own end. Miderator's grid check took 300-999 and ate the new
  items at 400 until it was bounded to the grid's own choices.
