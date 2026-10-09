# 0040 - Generated music is orchestrated across the chosen parts

- **Date:** 2026-10-09
- **Status:** Accepted (Noterator and Miderator alike; the code is shared).
  Extends 0019.

## Context

With bars chosen across several parts (0019), a result went in as: the tune
to the top part, the bass to the bottom one, the chords dealt out one note
each to the parts between, in score order. In a full orchestra that put the
tune in the piccolo alone, the bass in the double basses alone, and chord
notes in whichever parts happened to come between - by their place in the
score, not by what they can play. The user asked for every chosen part to be
filled by its range: bass instruments the bass, high ones the tune, the
others the chord between, as an orchestrator would. Their example: a triad
in a string quintet - the top note in Violin I, the same an octave down in
Violin II, the middle note in the viola, the bottom note in the cello and an
octave down in the double bass.

## Decision

`Orchestrate.*` (shared, in `Source/Engines`) does it, by Rimsky-Korsakov's
rules (Principles of Orchestration, chapter III), which the books the user
sent repeat (Belkin, The Idiomatic Orchestra, Orchestration Analysis):

- **Four real parts, the rest doublings**: the tune, inner parts, the bass.
- **Each section carries the whole harmony** in a tutti: strings, woodwind,
  brass and voices each get the tune on their top instrument, the bass on
  their bottom one, the chord's other notes between. Sections of one
  instrument are pooled into a section of their own. Top and bottom are by
  where each instrument sounds best (`sweetLow`-`sweetHigh`), not by score
  order.
- **The bass is doubled an octave below** by the double bass, contrabassoon
  or tuba, when the section has another bass instrument; otherwise they take
  the bass themselves.
- **The tune is doubled at the octave** by a spare string or woodwind part
  once the chord is complete: Violin II under Violin I for triads, a flute
  under the piccolo. Brass and saxophones voice the chord instead, and voices
  sing four real parts.
- **The chord's notes**: the ones the tune and the bass leave over, most
  wanted first - the third and seventh, then the root, the fifth last; a
  doubling prefers the root, then the fifth, the third last.
- **Voice leading**: each inner part starts where the generated chords have
  its note, then moves to the nearest note it can, below the tune and above
  the bass, close at the top, with no close intervals low down.
- **The harmony is read** with the Chords lane's own reader (`detectChords`),
  because Generate Notes' chords are often broken; inner parts strike where
  the chords are struck, or hold through a stretch of broken chords.
- Chord instruments (piano, harp, guitar, marimba, vibraphone, celesta) take
  the chords whole; timpani the bass at each change of harmony; glockenspiel
  and xylophone the tune; a kit the drums. A result with only a tune is
  played by everyone in octaves; one with a tune and a bass splits them.
- A result's parts are known by name first ("Melody", "Chords", "Bass",
  "Second voice"); only unnamed ones by how many notes they sound at once.

## Consequences

- Choosing every part of a template and generating fills every part (the
  full-orchestra test checks all 28), each in its range, one note at a time
  where it plays one.
- Voicing is greedy per chord over at most six inner parts (all orders tried),
  more by the cheapest note left: good, not a counterpoint teacher.
- Big band voicing is not from the books (none covers it): its sections are
  handled as brass and saxophones are, close, without octave doublings.
