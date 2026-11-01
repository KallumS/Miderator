# 0058 - AutoCC's curves follow Lorne Balfe's, as AutoCC 1.3 does

- **Date:** 2026-10-22
- **Number:** made as 0049 alongside another session's 0049-0057 and
  renumbered 0058 when the two met, in Noterator too (`110e897`).
- **Status:** Accepted (Noterator and Miderator alike). Updates 0013's
  presets; 0013's approach (computed once per part) is unchanged.

## Context

The AutoCC JSFX (KallumS/AutoCC) measured how professional mock-ups shape CC
around each held note: 21 of Lorne Balfe's cues as the reference, 17 other
scores for comparison (AutoCC's `docs/research/per-note-analysis.md`). The
user's rule is that Balfe's way is the right one. AutoCC 1.3 was re-fitted to
it, and `AutoCC.cpp` ports the JSFX value for value (0013), so the port is
brought up to 1.3.

What the measurements showed, and the port now does:

- A note **starts at the dynamic, not at 0**, and swells modestly with a
  fast-early rise: strings 85 → 116 over 2.2 s, woodwinds 46 → 63 within
  0.8 s, brass 102 → 109 then easing back to 106. The 1.2 presets swung
  0 → ~100 on every note.
- **CC11 follows CC1** at about 0.84 of its swell (they correlate 0.97),
  replacing "half of CC1".
- **CC7 and CC21 are held levels**, 94 and 92: Balfe sets each per track and
  leaves it. The old CC7 breathing and late CC21 vibrato swell are gone.
- **Velocity** scales the swell by 10% on CC1 and not at all on CC11: within
  a track his swell barely depends on velocity.
- **A note within 0.4 s of the last key-up carries the line on** (Legato):
  a lane that has started to fall holds where it is until the keys come up
  again, instead of swelling afresh. Notes Balfe starts that soon after the
  last get no swell of their own. Retrigger is unchanged.
- **Dynamic** (`AutoCCOptions::dynamic`) shifts CC1 and CC11's floor, peak
  and sustain together, keeping the swell, as the JSFX's slider 20 does. It
  has no control yet.

## Decision

`AutoCC.cpp` takes AutoCC 1.3's presets, `carry_env` and Dynamic. The four
tests in `TestScore.cpp` hold the Strings curve (85 → 116 and back), the held
CC7/CC21 levels, velocity on CC11, the 0.4 s rule both ways, and Dynamic.
Checked against the JSFX run in ysfx: Strings CC1/CC11 at 0, 0.5, 1, 2 and
3 s are 85/94, 97/106, 103/111, 114/118 and 116/118 in both, to the nearest
whole value.

## Consequences

- General MIDI playback (0008) gets CC7 at 94 × the part's volume instead of
  roughly 111-121. That is about 3 dB quieter through Apple's synth, which
  the user accepted.
- Exported .mid files carry Balfe's curves for sample libraries.
- When AutoCC's presets change again, this port changes with them, and the
  JSFX's `tools/ccstudy.py` is the place the numbers come from.
