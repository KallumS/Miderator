/*
    Spelling - which letter a pitch is written on, and with what accidental.

    The keys, scales and the spelling of their notes are ScaleView's
    (ScaleModel.h, copied unchanged), so F# major spells its seventh E# here as
    it does in every other app in the family. The key signature is scored, not
    looked up, and that is Starting Blocks Notation's `chooseSignature`, ported:
    a table answers for major and minor and has nothing to say about the other
    fourteen scales, and harmonic minor has to come out as the plain minor's
    signature with the seventh as an accidental.
*/

#pragma once

#include "Instruments.h"
#include "ScaleModel.h"

#include <array>
#include <string>

namespace nt
{

struct KeySignature
{
    int count = 0;            // 0..7
    bool flats = false;
    std::array<int, 7> map {};  // the accidental the signature puts on each letter, C..B

    int fifths() const { return flats ? -count : count; }
};

struct KeyContext
{
    int root = 0, scale = 0;          // scaleview indices
    KeySignature signature;
    std::array<bool, 12> inScale {};
    std::array<int, 12> letter {};    // for the scale's own notes
    std::array<int, 12> accidental {};
    bool leansFlat = false;
    std::string label;                // "F# Major"
};

// A written pitch. `step` counts staff degrees, octave * 7 + letter, so middle
// C is 28 and one step is one line-or-space whatever accidental it wears.
struct Spelled
{
    int step = 28;
    int letter = 0;
    int accidental = 0;   // -2..2
    int octave = 4;
};

inline constexpr int middleCStep = 28;

KeySignature signatureFromFifths (int fifths);
KeySignature chooseSignature (const std::vector<std::pair<int, int>>& letterAndAccidental, int tonicAccidental);
KeyContext keyContext (int rootIndex, int scaleIndex);
Spelled spell (int writtenPitch, const KeyContext& key);
std::string stepName (int step);
std::string pitchName (int pitch, const KeyContext& key);   // "F#4"

// The staff degree of the bottom line of a five-line staff in this clef.
int clefBottomStep (Clef clef);

int rootIndexByName (const std::string& name);
int scaleIndexByName (const std::string& name);

} // namespace nt
