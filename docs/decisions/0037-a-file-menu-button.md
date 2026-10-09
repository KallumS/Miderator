# 0037 - New, Open, Save and Export under one File button

- **Date:** 2026-10-09
- **Status:** Accepted (Noterator and Miderator alike); the menu gained the Score tab's settings in 0038

## Context

The toolbar had grown: four file buttons, Undo and Redo, the transport and
Follow, then each app's own tools. The user found it cluttered and asked for
New, Open, Save and Export to go under one button called File.

## Decision

- One **File** button, first in the toolbar, opens a menu: **New** (the
  templates, grouped, as a sub-menu), **Open...**, **Import MIDI or
  MusicXML...**, **Save**, **Save As...**, and **Export** (the same choices
  as before, as a sub-menu). Their keys (Cmd+N, Cmd+O, Cmd+S, Cmd+E) are
  shown beside them and still work.
- The Mac's own File menu at the top of the screen is unchanged.

## Consequences

- About 170 pixels of toolbar back, for what is used while writing.
- Saving takes a click more with the mouse; Cmd+S is the same.
