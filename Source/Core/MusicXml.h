/*
    MusicXml - scores in and out of the format every notation program shares:
    Dorico, MuseScore, Sibelius, Finale, Notion (decision 0021).

    Writing reads the engraver's layout rather than the notes, so the file says
    what Noterator's page says: the same note values, ties, triplets, beams,
    voices, stems, accidentals, clefs and staves. Pitches are written as the
    player reads them, with <transpose> telling the reader how they sound, as
    MusicXML expects. Drums are unpitched notes with a MIDI instrument each.

    Reading takes the notes, their timing, ties, voices, keys, time
    signatures, tempo and transposition, and turns them back into Noterator's
    MIDI-shaped score; the notation is then worked out afresh (decision 0002).
    Dynamics, articulations, slurs, lyrics and layout are not read yet.
    Partwise files only - timewise ones are almost never written.
*/

#pragma once

#include "Score.h"

#include <string>
#include <vector>

namespace nt
{

std::string writeMusicXml (const Score& score);

struct MusicXmlResult
{
    bool ok = false;
    std::string error;
    Score score;
};

MusicXmlResult readMusicXml (const std::string& text);

// The instrument a MusicXML part is, from its name and its General MIDI program
// (1-based in the file, -1 when absent).
std::string instrumentForPartName (const std::string& name, int program, bool percussion, int staves);

} // namespace nt
