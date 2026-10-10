# 2026-10-10 - Choosing several parts, and bars with Cmd (both apps)

## Asked

DAW shortcuts for instruments: Cmd+A (every instrument, or with none chosen
every note and instrument), Shift-click for every instrument between,
Cmd-click to add one - and the same Cmd-click for bars. Asked, the user
said an idea is shared across the instruments chosen, the single line to
the one chosen first (the top one after Cmd+A), and Cmd-click on the
timeline adds bars.

## Built (0050)

- `Controller` (both apps, the same patch): `chosenParts`, `clickPart`,
  `partsChosen`, `isPartChosen`, `partsText`, `workIn`; Cmd+A in
  `selectAll`; `BarRange` became `Bars` plus `more` blocks, with `blocks()`,
  `addRange`, `selectRange (..., keepOthers)`, `notesInBlocks`,
  `lineTargetIn`; `place`, `insertGenerated` and `auditionScore` go block
  by block.
- Views: name clicks with Cmd/Shift (tracks, staff names, Parts tab), Cmd
  on bars (`addingBars` through a drag), every block drawn, every chosen
  part marked. The Generate tab (shared) names the parts chosen.
- A test in both `TestApp.cpp`s covering each shortcut and two blocks
  filled on their own; checked in both windows headless (Cmd is Ctrl
  there).

## What looked broken and was not

- With two parts chosen in Noterator the caret runs from the top chosen staff
  to the bottom one, across a staff between that is not chosen; the marks by
  the names say which are.
