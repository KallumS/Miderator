/*
    MidiFile - standard MIDI files in and out, with no JUCE in it so the round
    trip can be tested on its own.

    Writing: type 1, a conductor track (tempo, time signatures, key
    signatures, the title) and one track per part named for it, carrying
    whatever `perform` produced - so a file holds exactly what playback plays.

    Reading: any type 0 or 1 file. Each track becomes a part, or each channel
    of a track where one track carries several (a type 0 file, or Good Idea's
    one-item export with its parts on separate channels). Channel 10 becomes a
    drum kit.
*/

#pragma once

#include "Perform.h"
#include "Score.h"

#include <cstdint>
#include <string>
#include <vector>

namespace nt
{

std::vector<uint8_t> writeMidiFile (const Score& score, const PerformOptions& options = {});

struct MidiReadResult
{
    bool ok = false;
    std::string error;
    Score score;
};

// `fallbackInstrument` is what a part becomes when the file does not say
// (General MIDI programs are matched to the instrument table where they can be).
MidiReadResult readMidiFile (const std::vector<uint8_t>& bytes, const std::string& fallbackInstrument = "pno");

// The instrument whose General MIDI program this is, or the fallback.
std::string instrumentForProgram (int program, const std::string& fallback);

} // namespace nt
