# 2026-10-10 - Undo brings back generated ideas (both apps)

## Asked

After trying Escape (0048): "it'd be great if Undo could also go back to
previous generations after a user has clicked Generate" - an idea liked was
lost to one Generate too many.

## Built (0049)

- `Controller`'s undo stack became a stack of `Step`s, a score or a
  Generate mark; `generated()` pushes a mark, `stepResults` lets the
  Generate tab follow Undo and Redo. The same in both controllers.
- `GeneratorPanel` (shared) keeps every list with its generator, seed and
  chosen row, steps through them on Undo and Redo without auditioning, and
  truncates them on a new Generate as Redo is.
- `TestApp.cpp`, both apps: the real Generate tab, two Generates around an
  edit, Undo and Redo through all of them. The app tests now build the tab
  (`GeneratorPanel`, `SettingsList`, `Theme`). Fails with the tab's
  `controller.generated()` taken out.

## What looked broken and was not

- In the test, Generate did nothing at first: the tab's menu chooses Generate
  Notes asynchronously (`ComboBox::setSelectedId` notifies later), and a test
  has no message loop. The test calls the menu's `onChange` itself.
