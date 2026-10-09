# 2026-10-07 (evening) - Templates for sections, orchestra and big band

The user asked for templates: the full string, brass and woodwind sections,
all of the sections together, and a jazz big band.

## The route

- **Templates moved into the core** (0022) so they can be tested: every
  instrument a template names must exist. The New menu is now grouped under
  headings (small ensembles, orchestral sections, orchestras and bands) with
  how many parts each starts with. Added: Full Strings, Full Woodwinds, Full
  Brass, Orchestral Percussion, Full Orchestra (28 parts), Big Band (17 parts)
  and Jazz Combo. Baritone sax, vibraphone and upright bass joined the
  instrument table for them.
- **The real problem was sound, not the menu** (0023). General MIDI has
  sixteen channels; parts past the fifteenth wrapped round and shared a
  channel, so in a full orchestra the last program change on a channel won
  and instruments played each other's sounds. Previews and the MIDI keyboard
  also used channels 15 and 16, which parts could be on. Now every part has a
  channel of its own across up to four synths (a `SynthRack`), each made on
  the message thread before it is needed. The test plays only the double
  basses of a full orchestra - the 28th part, on the second synth - and checks
  the WAV is not silent; on the Mac runner that goes through Apple's synth.
- **Names spilled out of the margin** in the Big Band ("Baritone Saxophone"
  ran off the left edge). A name that does not fit now takes the instrument's
  short name with its number: T. Sax. 2, Bari. Sax.
- Tried in the window: Big Band from the menu, bars 2-5 chosen along the
  chord lane across all seventeen parts, a Good Idea phrase inserted.

## What looked wrong and was not

- **The phrase went to the first alto alone.** That result was a melody only
  (Good Idea's "Phrase (melody)"); a melody goes to the top part of the chosen
  bars (0019). A result with chords and bass spreads across the band.

## Then: Bass and Drums out of Blocks (0024)

The user asked to keep drums in Good Idea and take Drums and Bass out of the
Blocks toolbox. The adapter now offers the engine's kinds less those two (the
engine is untouched), the four remaining kind buttons sit in one row, and a
core test checks both halves: Blocks lists Chord, Arpeggio, Run and Melody
(shown as Interval), and Good Idea still offers Drums. The test failed
against the old adapter first. Every doc was then brought up to date and the
handover prompt rewritten.

## Then: names for what they do (0025)

In the app, Good Idea became **Generate Notes**, Midi Suggester **Suggest
Notes** and Midi Variator **Vary Notes** - display names in the adapters only.
Good Idea's result titles began "Good Idea 63797 - ", its REAPER file name;
the adapter now drops that, so the list reads "Motif, 2 bars, C Major". A
core test pins the three names and checks no title says Good Idea.

## Not done yet

Everything in the earlier logs' lists, plus:

- Templates are fixed; a user's own ensemble cannot be saved as one yet.
- Section sizes are standard; there is no "a2" or divisi (one staff for two
  flutes).
- Over 58 pitched parts, channels are shared again.
