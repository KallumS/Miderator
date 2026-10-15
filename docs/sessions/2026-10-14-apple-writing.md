# 2026-10-14 - The apps' text to Apple's writing guidelines (both apps)

## Asked

"Review the GUI text on both Noterator and Miderator to make them in line
with Apple's suggested guidelines" - the HIG's Writing page.

## Done (0053)

- Read the Writing page and, for the Mac's specifics, the Menus, Buttons and
  Alerts pages. The developer site renders with JavaScript; its data is at
  `developer.apple.com/tutorials/data/design/human-interface-guidelines/<page>.json`.
- `ui()` in `Theme.h` for text beyond ASCII; `tools/check_ui_text.py` to
  find any that is not read through it (tested on a planted mistake).
- Every menu, button, dialog, alert, tooltip, status message and the
  Keyboard Shortcuts window rewritten in both apps; the shared Generate tab
  and export messages in Noterator first, copied across. Shortcuts show as
  the Mac writes them (`key()`: ⇧⌘S, ⌫). The part-removal confirmation
  became an alert with a Remove button.
- A test expected "Bars 2-5": now the en dash.

## What looked broken and was not

- On Linux the menus show "Shift+Ctrl+S": `key()` writes symbols only on the
  Mac.

## Not done yet

- Good Idea's "Make a" setting label (and the other generators' setting
  names) come from the engines; change them there.
- The shared core's file errors ("This is not a MIDI file.") were already
  plain and are left as they are.

# Later - the engines' own words (2026-10-15)

Each generator changed its words in its own repository (Good Idea's "Make a"
is "Kind"; multi-word values title-style, such as "1 a Bar" and "Towards the
End"; em dashes and curly quotes in hints). Noterator copied them at
`7539142` and `tools/sync_from_noterator.sh` brought them here; see
Noterator's session log of the same name.
