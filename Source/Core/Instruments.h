/*
    Instruments - what Noterator knows about who is playing a part.

    This is what makes the app aware of context: a violin line and a viola
    line differ in clef, in range, in the register where each sounds like
    itself, in which voice of the harmony it naturally takes, and in how fast
    and how far it moves comfortably. Every one of those is a field here, and
    the editor, the checks, the generators and the playback all read them from
    this one table (decision 0006).

    The orchestral rows are Midi Catalogue's `mc_orchestra.lua`, unchanged in
    every number it has - ranges, sweet spots, roles, speeds and leaps - so the
    generators and the editor agree about an instrument. `tests` assert they
    still match. What this table adds is what a score needs and the catalogue
    did not: clefs, transposition, a General MIDI sound, and which AutoCC
    shape suits it.
*/

#pragma once

#include <string>
#include <vector>

namespace nt
{

enum class Clef
{
    treble,
    bass,
    alto,
    tenor,
    treble8vb,    // a treble clef with an 8 under it: guitar, tenor voice
    percussion
};

// Which of AutoCC's instrument presets shapes this instrument's CC curves.
enum class CCShape
{
    none,         // piano, percussion: nothing to swell
    neutral,      // AutoCC's Default
    strings,
    brass,
    woodwinds
};

struct Instrument
{
    std::string id;           // stored in files: never rename one
    std::string name;
    std::string shortName;    // beside the staff after the first system
    std::string family;       // Strings, Woodwind, Brass, Percussion, Keys & Harp, Voices, Band
    int low = 0, high = 127;  // the practical range, sounding
    int sweetLow = 0, sweetHigh = 127;  // where it sounds like itself
    char role = 'A';          // S A T B: the voice of four-part harmony it takes
    int poly = 1;             // notes it sounds at once
    double fast = 0.08;       // the shortest gap between two notes it plays cleanly, seconds
    int leap = 12;            // the widest leap it takes happily in passing, semitones
    bool breath = false;      // wind and brass: phrases leave room to breathe
    int program = 0;          // General MIDI program, 0-based
    bool drums = false;       // General MIDI channel 10, percussion staff
    std::vector<Clef> staves { Clef::treble };  // one clef per staff, top first
    int splitPitch = 60;      // on a two-staff instrument, this and above go on top
    int octave = 0;           // written = sounding + octave, always (double bass +12, piccolo -12)
    int transposition = 0;    // written = sounding + this as well, in a transposed score only
    std::string transposedName;  // "in Bb", shown when the score is transposed
    CCShape cc = CCShape::none;
    std::string catalogueId;  // the mc_orchestra id, where Midi Catalogue knows it

    bool monophonic() const { return poly <= 1; }
    int writtenPitch (int sounding, bool transposedScore) const
    {
        return sounding + octave + (transposedScore ? transposition : 0);
    }
};

const std::vector<Instrument>& instruments();
// Never fails: an id nobody knows comes back as the piano, so a file from a
// newer version still opens.
const Instrument& instrumentById (const std::string& id);
bool hasInstrument (const std::string& id);
const std::vector<std::string>& instrumentFamilies();

} // namespace nt
