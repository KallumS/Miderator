# 2026-10-15: the window against Apple's guidelines

Asked: make Noterator and Miderator follow Apple's design principles, as
Transient Shuffler had just been, from the Human Interface Guidelines'
nineteen Foundations pages the user attached. Done first in Noterator (its
session log of the same day has the whole route and the three reasons Tab
showed no ring), then here, as decision 0048, reviewed page by page in
`docs/apple-hig-review.md`. On `claude/blissful-bohr-lqwr1j` in both.

## What came across, and how

- Shared and synced from Noterator `4f67cb3`: the Generate tab (its message
  under the buttons, a readable red for errors, names for VoiceOver, Return
  inserts) and the settings list (each menu named by its label).
- Miderator's own, made to match Noterator's: the theme (contrast, the ring,
  Increase Contrast), `SystemSettings.*` (a copy), the toolbar folding into
  two rows (the roll's tools on the second), the parts panel, the status line
  under the toolbar, the window's keys, menus, alerts and title bar,
  Blocks' buttons above its preview, Reduce Motion in `Timeline::follow`.
- `Tests/TestWindow.cpp`, Noterator's with the page's colours swapped for
  the tracks' and the roll's, light and dark.

## Miderator's own findings

- **Menu ids:** Interface Size was to take 300 onwards, which is the Grid
  menu's range here. The new ranges are 340 and 360 in both apps.
- **A track's M and S** were 20 by 16 points, under Apple's 20 by 20. Now 22
  by 20, a little further apart.
- **A track's instrument**, written under its name at 60% of the name's
  colour, was 3.7:1 on the chosen track. `RollColours::headerDim` makes it
  6:1 or more, light and dark.
- **A note out of the instrument's range** was only red in the roll; it now
  gets the corner mark the struggling notes already had.
- The tracks and the roll are each one element to VoiceOver, described in
  words; a track's drawn M and S cannot be reached by it (the Parts tab's
  Mute and Solo can).

## Not done yet

- Nothing of 0048 tried on a Mac (VoiceOver, Full Keyboard Access, the
  Mac's settings, alerts and title bar).
- The app icon in Apple's layered Icon Composer format.
- VoiceOver reading notes one by one; the keyboard cannot stretch a note or
  choose bars.
