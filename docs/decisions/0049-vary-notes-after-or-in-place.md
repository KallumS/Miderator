# 0049 - Vary Notes goes after the selection, or in its place

- **Date:** 2026-10-21
- **Status:** Accepted (Noterator and Miderator alike). Adds a choice to
  0011 and 0019.

## Context

Vary Notes put a variation after the notes selected (0011: a result never
overwrites the music it came from), but in the place of bars chosen (0019).
The user asked for the choice: replace the selected music, or add the
variation after it.

## Decision

A **Put it** menu in the Generate tab, shown for Vary Notes where "Use the
score's key" is for the others (Vary Notes hears the key in the music, so
that toggle meant nothing to it): **After the selection** or **In place of
the selection**. The same choice for notes selected and for bars chosen.
Remembered between launches (`varyReplaces` in the settings), off at first:
nothing is overwritten unless asked.

Decided in `Controller::place`, as every placement is:

- **After, notes selected:** in the part they came from, from the bar after
  the last of them (as before).
- **In place, notes selected:** the selected notes go, and the variation
  starts on the bar where they began (where the variation's own time
  starts, `contextFor`'s origin). Anything else in those bars stays.
- **After, bars chosen:** in as many bars after them, in the same part; the
  variation is then selected, so it can be varied again.
- **In place, bars chosen:** in those bars, as before; they stay chosen.

An audition is heard where the insert would go, for as long as the bars
chosen (it was measured from where it went, which after the bars would have
been nothing).

## Consequences

- One undo step either way; Undo brings back what a variation replaced.
- The Into line says which: "goes after them", "goes in their place".
- `TestApp.cpp` checks all four.
