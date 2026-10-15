# 0048 - The window follows Apple's Human Interface Guidelines

- **Date:** 2026-10-15
- **Status:** Accepted (Noterator and Miderator alike, and Transient Shuffler
  before them)

## Context

The user asked for Noterator and Miderator to follow Apple's design
principles, as Transient Shuffler had just been made to, and supplied the
nineteen Foundations pages of the Human Interface Guidelines (June 2026).
The window was checked against each page; [the review](../apple-hig-review.md)
lists every guideline, what was already met, what changed and what could
not be done.

## Decision

The house look stays (0015): the dark chrome, dark ink on light controls,
one yellow for "on". Within it:

1. **Contrast.** Text is at least 7:1 against what it sits on, secondary
   text included (`theme::textDim` went from 4.7:1 to 7.3:1, the page's bar
   numbers in Noterator from 3.1:1 to 6.6:1, a track's instrument here to
   6:1 or more). The house red is 3.4:1 on the ground, so it
   only marks things; an error is written in `theme::warnText` (7.5:1), the
   value Transient Shuffler uses.
2. **The Mac's own settings.** Increase Contrast lifts secondary text,
   rules and outlines further (`theme::useIncreasedContrast`); Reduce Motion
   makes Follow turn a page at a time instead of scrolling
   (`Controller::followStyleNow`), keeping the user's choice for when it is
   off. Both are read from NSWorkspace through the Objective-C runtime
   (`SystemSettings.cpp`), every second and a half.
3. **The keyboard.** Clicking a button, menu or slider leaves the keyboard
   with the roll, as on the Mac (`theme::clicksKeepFocus`): before, a
   clicked button kept it, and the arrows changed the grid menu instead of
   moving notes. Tab and Shift+Tab go from the roll through every control
   and back, each wearing a ring in the text colour - never the yellow,
   which means "on" - Space or Return presses it, Esc hands the keys back.
   Choosing a side tab no longer hands the keyboard to the first button in
   it; Cmd+1 to 3 choose the tabs. The window, given the keyboard as it
   opens, hands it to the roll. Return in the results inserts the chosen
   one, as its tooltip said.
4. **VoiceOver.** Every control has a name to read out ("Mute", not "M";
   "Move Up", not "^"; a setting's menu its label); toggles and choices say
   whether they are on; the tracks and the roll are each one element
   described in words (`Controller::describe`); a new status message is
   read out.
5. **Layout.** Nothing that matters along the window's bottom edge, which
   is often below the screen: the status line is under the toolbar, Add
   Instrument at the top of the Parts tab, Blocks' Play and Insert above its
   preview, the Generate message under its buttons.
6. **Larger text.** View > Interface Size, 100% to 200%, remembered: the
   whole window drawn larger (`Desktop::setGlobalScaleFactor`), the toolbar
   folding into two rows, the smallest window shrinking so 200% still fits a
   laptop.
7. **Familiar things.** The Mac's own alerts for questions and errors, a
   destructive button named for what it does ("Remove"); Cancel left of OK;
   the title bar as a Mac document's (its name, a dot while unsaved, the
   file's icon); title case and a real ellipsis in menus and buttons;
   drawn icons for move up, move down, remove, Play and Stop; the smallest
   controls at least 20 points (a track's M and S were 20 by 16); a note out
   of an instrument's range marked by a shape as well as red.
8. **Words.** Errors say what to do next; empty places say what to do
   ("Press Generate to make some ideas..."); tooltips promise only keys that
   work (Generate's "G" never did - G writes a note).

## Consequences

- `Tests/TestWindow.cpp` checks the colours (with and without Increase
  Contrast, the tracks and the roll light and dark), that every control has
  a name and leaves the keys with the roll, the toolbar's two rows, Reduce
  Motion, and the words for VoiceOver.
- Anything new in the window follows the same rules: a title for anything
  whose face is not a word, `clicksKeepFocus` for a control made later,
  `focusRing` in a custom-drawn one, text colours from the theme.
- An app-specific appearance setting is something Apple advises against.
  The page's Dark page (0020) and Miderator's Light (0031) stay: they are
  the colour of the document, as Pages' and Mail's own options are, and the
  chrome is always dark as a media app's may be.
- Not done: the app icon in Apple's newest layered format (it needs Icon
  Composer on a Mac); VoiceOver reading the notes one by one, or reaching a
  track's M and S (the Parts tab's do the same); right-to-left
  languages (the window is in English only). Nothing here was tried with
  VoiceOver on a Mac: only the names it would read were checked.
