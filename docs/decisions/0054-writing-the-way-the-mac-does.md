# 0054 - The apps' text follows Apple's writing guidelines

- **Date:** 2026-10-14
- **Status:** Accepted (Noterator and Miderator alike).

## Context

The user asked for every word in both apps to follow Apple's Human Interface
Guidelines on writing
(developer.apple.com/design/human-interface-guidelines/writing), with the
menus, buttons and alerts pages that apply them on the Mac. The apps had
grown their own habits: "Open..." with three full stops, menus in sentence
case beside menus in title case, "caret" (a programmer's word), " - " for
dashes, "Could not save" with no way forward, "this is a bug, please report
it", a confirmation with OK and Cancel.

## Decision

One style for each kind of text, everywhere:

- **Menu items, buttons, tabs and dialog titles** in title-style
  capitalization, a verb first where it acts: "Save As", "Concert Pitch",
  "Insert Bar Before Bar 3", "Detect Key from the Notes".
- **An ellipsis character (…)** after an item that asks for more before it
  acts - a file panel, a dialog: "Open…", "Title and Composer…".
- **Messages, tooltips, the status line and descriptions** in sentence
  case; complete sentences end with a full stop, fragments do not.
- **Alerts** say what happened as a sentence ("“Song.mid” couldn’t be
  opened.") and what to do in the text under it; the buttons say what they
  do ("Remove", "Don’t Save", "Cancel"), never OK for a choice. The save
  prompt uses the Mac's own words.
- **Errors** say what to try, without blame, "we" or "please": "Enter a
  tempo from 30 to 240 beats per minute."; "Report this as a bug".
- **One word per idea**: the **cursor** (where playing, pasting and writing
  start - "caret" is gone from the screen, though the code keeps the name),
  **ideas** (what Generate lists), **parts**, **bars**; "song" in Miderator
  and "score" in Noterator, as before.
- **Typography**: an em dash (—) between phrases, an en dash (–) in ranges
  ("Bars 2–5"), curly quotes and apostrophes in alerts, and the Mac's key
  symbols (⌘ ⇧ ⌥ ⌫) in menus and the Keyboard Shortcuts window. File names
  keep plain hyphens.

**Text beyond ASCII goes through `ui()`** (`Theme.h`): `juce::String` reads a
plain literal as ASCII, garbling "…" and asserting in a Debug build.
`tools/check_ui_text.py` finds any literal that does not, and must pass.
Source files are UTF-8 (GCC and Clang read them so; MSVC will need `/utf-8`
when Windows comes).

## Consequences

- Labels the user knew changed: "Use the key it hears" is "Detect Key from
  the Notes", "This score/song" is "Score/Song Settings", "Paste at the
  Caret" is "Paste", "More notes..." tooltips speak of the cursor.
- The generators' own setting names come from their engines (Good Idea's
  "Make a", for one) and are changed in those repositories, not here.
- New text follows the same rules; the check script catches the encoding.
