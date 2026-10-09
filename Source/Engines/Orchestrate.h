/*
    Orchestrate - a generated result shared out across the parts the user
    chose, by what each instrument is (decision 0040).

    The rules are Rimsky-Korsakov's (Principles of Orchestration, chapter
    III), the ones every orchestration book since repeats:

    - Harmony is four real parts - the tune, two inner parts, the bass -
      and every other instrument doubles one of them.
    - In a tutti each section (strings, woodwind, brass, voices) carries the
      whole harmony on its own: its top instrument the tune, its bottom one
      the bass, the ones between the chord's other notes.
    - The bass is doubled an octave below, by the double bass, contrabassoon
      or tuba, which never play alone when something can take the bass.
    - Upper parts are doubled at the octave: once the chord is complete, the
      next instrument down plays the tune an octave lower (Violin II under
      Violin I, a flute under the piccolo). Brass, saxophones and voices
      fill out the chord instead.
    - The notes a chord lacks come first - its third and seventh - then the
      root, then the fifth; a doubling prefers the root, then the fifth,
      the third last.
    - Each inner part moves to the nearest note it can, below the part above
      it and above the bass, in its own best register.

    Instruments that play chords (piano, harp, guitar, marimba...) take the
    chords whole; timpani the bass on each change of harmony; glockenspiel
    and xylophone the tune; a kit the drums.

    No JUCE: tested with the core.
*/

#pragma once

#include "Generators.h"

namespace nt
{

enum class Layer { none, melody, melodyDouble, second, inner, bass, bassDouble, chords, drums, timpani };

struct Assignment
{
    uint32_t partId = 0;
    Layer layer = Layer::none;
};

// The layers a result offers.
struct Layers
{
    bool melody = false, second = false, bass = false, harmony = false, drums = false;
    int innerNeeded = 1;      // the chord notes the tune and the bass leave over, in most chords
};

// Who plays what among the chosen parts, top part of each section first.
std::vector<Assignment> planOrchestra (const Score& score, const std::vector<uint32_t>& parts, const Layers& layers);

// One stretch of unchanging harmony.
struct Harmony
{
    Tick start = 0, end = 0;
    std::vector<int> pcs;     // the chord's pitch classes
    int root = 0, bass = 0;   // pitch classes
    struct Hit { Tick start, length; int velocity; };
    std::vector<Hit> hits;    // where the chord is struck within the stretch; one held note if none
};

// The harmony of a chords part (and a bass, if the result has one), read
// off it with the Chords lane's own reader. Times are the notes' own.
std::vector<Harmony> readHarmony (const Score& score, Tick at, const std::vector<Note>& chords, const std::vector<Note>& bass);

// The inner parts of one section, top first, voiced between the tune above
// them and the bass below: each starts where the generated chords (`source`)
// have its note, then moves to the nearest note it can.
std::vector<std::vector<Note>> voiceInner (const std::vector<Harmony>& harmony, const std::vector<const Instrument*>& voices,
                                           const std::vector<Note>& above, const std::vector<Note>& below,
                                           const std::vector<Note>& source);

// Shares a result made to fit [from, to) across the chosen parts. Lines it
// has no place for are named in the report's `unplaced`; no part is added.
InsertReport orchestrate (Score& score, const GeneratedResult& fitted, const std::vector<uint32_t>& parts,
                          Tick from, Tick to, const InsertOptions& options);

} // namespace nt
