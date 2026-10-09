# 0035 - Space plays from bar 1, Shift+Space from the caret; buttons to the start and the end

- **Date:** 2026-10-09
- **Status:** Accepted (Noterator and Miderator alike)

## Context

Space played from the caret, or from the selection's first bar (Noterator)
or the chosen bars (Miderator). The user asked for Space to play from bar 1
and Shift+Space from the current location, and for a "return to start" and
a "skip to end" button beside Play, in both apps. Most DAWs play from the
cursor on Space; that is a choice someone could make the other way, which
is why this is written down.

## Decision

- **Space** plays from bar 1; **Shift+Space** plays from the caret. Either
  stops the music if it is playing. The **Play** button plays from the caret,
  as Shift+Space - with **|◀** beside it, bar 1 is one click away.
- "The current location" is the caret: where the last click on a note, a
  bar or the ruler put it. The selection and the chosen bars no longer
  decide where playing starts; they put the caret where they begin.
- **|◀ Return to start** (and Home): the caret to bar 1, and the view with it;
  if it is playing, it plays on from bar 1.
- **▶| Skip to end** (and End): playing stops, and the caret goes to the end
  of the music - the bar line after the last note, or the end of the score
  when there are none - with the view showing the last bars.
- The Play menu lists all four with their keys.
- The two buttons are drawn (a bar and a triangle), not typed, so they look
  the same whatever fonts the Mac has.

## Consequences

- Playing a passage means clicking where it starts and pressing
  Shift+Space (or Play), rather than choosing bars and pressing Space.
