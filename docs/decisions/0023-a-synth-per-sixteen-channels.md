# 0023 - A synth for every sixteen channels, so every part has its own

- **Date:** 2026-10-07
- **Status:** Accepted

## Context

General MIDI has sixteen channels, and channel 10 is the drums. Parts were
given channels in order and wrapped round after fifteen, so in a big score
two parts shared a channel and the last program change won: in a full
orchestra the flutes would have played as violins. Channels 15 and 16 were
also used by the MIDI keyboard and by previews, so clicking a note could
change the sound of the parts on them.

## Decision

- A part's channel is a **bank** of sixteen and a channel within it. Every
  drum part plays on channel 10 of the first bank; every other part has a
  channel of its own, through the first bank (less 10, 15 and 16, kept for
  drums, the keyboard and previews), then the next, up to four banks
  (58 pitched parts). Only past that are channels shared.
- A `SynthRack` holds one synth per bank - Apple's General MIDI Audio Unit
  where there is one, the built-in synth otherwise - and mixes them. The
  sequence carries each event's bank; the audio thread sends each bank's
  MIDI to its synth.
- More synths are made when a score needs them, on the message thread (an
  Audio Unit must be), before the sequence that needs them is swapped in.
  Audio export makes its rack's synths before its thread starts, and refuses
  rather than render a bank it has no synth for.
- A .mid or MusicXML file has sixteen channels, so files get the channel
  within the bank; each part is still its own track or part with its own
  program.

## Consequences

- A 28-part orchestra plays with every instrument on its own sound and its
  own AutoCC curve. The app tests render a full orchestra's double basses
  (in the second bank) and check they are not silent, on the Mac through
  Apple's synth.
- A second Audio Unit costs memory and some CPU, only for scores that need it.
