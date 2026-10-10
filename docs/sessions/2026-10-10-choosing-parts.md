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

## Then: no More button (0051)

The user found More useless next to Generate, now that Undo brings back a
replaced list. Removed from `GeneratorPanel` (shared): Generate, Insert and
Stop share the row; every Generate is a fresh list.

## Not done yet

Rough edges left from today's three changes (0048-0050), for whoever comes
next:

- A block from the Blocks toolbox still goes into the caret's part even
  with no part or several parts chosen (the Blocks tab names it). It could go
  to the first chosen, or the top one.
- Vary Notes and export with several blocks of bars chosen use the bars from
  the first chosen to the last, not each block.
- Noterator's caret runs from the top chosen staff to the bottom one,
  across staves between that are not chosen.
- Chosen parts are not saved with the song (nothing about the view is, 0002)
  and Undo does not bring a choice back.
- The branch `claude/adoring-feynman-bihxp2` (0048-0050) is not merged yet in
  either app; the user tested 0048 on their Mac and liked it, 0049 and 0050
  only as builds.
