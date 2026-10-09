# 0041 - Where generated music goes: everywhere, one part, or the parts chosen

- **Date:** 2026-10-09
- **Status:** Accepted (Noterator and Miderator alike). Replaces how 0019 and
  0040 place a result with nothing chosen or one part chosen; 0040's sharing
  is unchanged.

## Context

Trying 0040, the user found where a result went confusing: with nothing
chosen it went into the caret's part and new parts appeared for its other
lines; with bars of one part chosen the same; only bars across several parts
were shared out. They asked for four plain cases.

## Decision

`Controller::place`, the same in both apps:

| Chosen | The result goes |
| --- | --- |
| Nothing | Shared across **every** part (0040), from the caret's bar, all of it |
| Bars of **one** part (one bar or more) | All of it into that part alone, filling exactly those bars |
| Bars of **several** parts | Shared across those parts (0040), filling exactly those bars |
| Notes, for Suggest or Vary Notes | As before (0011) |
| A block from the toolbox | As before, at the caret (0018) |

- **One part takes all of it** (`insertWhole`): its lines together, thinned to
  what the instrument plays at once (the top notes - the bottom for a bass),
  then moved into the instrument's register. Drums go only to a drum kit and
  a kit takes only drums.
- **No part is ever added.** A line with nowhere to go - drums with no kit
  among the parts - is left out and named in the status line.
- The Generate tab says where it will go: "Into: every part, from bar 5".
- An idea is heard in place before it goes in, on the parts it would go to.

## Consequences

- Inserting with nothing chosen replaces what every part had for the idea's
  length from the caret's bar - one Undo puts it back.
- With one bar chosen the generator makes ideas of a bar (Generate Notes
  fits its length to the chosen bars, as before).
