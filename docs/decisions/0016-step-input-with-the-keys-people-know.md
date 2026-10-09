# 0016 - Step-time input, with the keys notation users already know

- **Date:** 2026-10-07
- **Status:** Accepted

## Context

Notes go in by mouse, computer keyboard and MIDI keyboard. Real-time
recording against a click is a project of its own (quantising, tempo
following); step-time input is what every notation program does first.

## Decision

- **Step time.** A caret marks where the next note goes; writing a note
  overwrites that voice there and moves the caret on (0005).
- **MuseScore's keys**, the most widely used: letters A-G write the note the
  key signature gives that letter, nearest the last note (Shift adds it to the
  chord above); 1-7 choose the value with 5 a quarter; `.` dots, T triplets,
  0 rests; N or Return toggles note input; arrows move and transpose; Space
  plays.
- In an empty part the first letter lands in the middle of the instrument's
  sweet register (0006), so a viola's C is not a violin's.
- A MIDI keyboard writes in note input - keys pressed within 60 ms are one
  chord - and is always heard.
- The mouse writes the note on the line or space clicked, with the key's
  accidental; a ghost notehead shows where.

## Consequences

- No real-time recording yet, and no way to type an accidental on a letter
  other than by transposing with the arrows afterwards.
