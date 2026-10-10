#include "Check.h"

#include "AutoCC.h"
#include "Edit.h"
#include "Instruments.h"
#include "ScoreFile.h"
#include "Spelling.h"

using namespace nt;

namespace
{
Score twoParts()
{
    Score s;
    Part a; a.id = s.newId(); a.name = "Violin"; a.instrument = "vln1";
    Part b; b.id = s.newId(); b.name = "Cello"; b.instrument = "vc";
    s.parts = { a, b };
    return s;
}
} // namespace

TEST ("time: bars through a change of meter")
{
    Score s;
    s.meters = { { 0, 4, 4 }, { 2, 3, 4 }, { 4, 6, 8 } };
    s.bars = 8;
    CHECK_EQ (s.barStart (0), Tick (0));
    CHECK_EQ (s.barStart (2), 8 * PPQ);
    CHECK_EQ (s.barStart (3), 11 * PPQ);
    CHECK_EQ (s.barStart (4), 14 * PPQ);
    CHECK_EQ (s.barStart (5), 17 * PPQ);
    CHECK_EQ (s.barAt (8 * PPQ), 2);
    CHECK_EQ (s.barAt (11 * PPQ - 1), 2);
    CHECK_EQ (s.barAt (11 * PPQ), 3);
    CHECK_EQ (s.barAt (100 * PPQ), 4 + (100 - 14) / 3);
    CHECK_EQ (s.meterAtBar (5).beatTicks(), PPQ * 3 / 2);   // 6/8 beats in dotted quarters
}

TEST ("time: seconds through a change of tempo")
{
    Score s;
    s.tempos = { { 0, 120.0 }, { 4 * PPQ, 60.0 } };
    CHECK (std::abs (s.secondsAt (4 * PPQ) - 2.0) < 1e-9);
    CHECK (std::abs (s.secondsAt (5 * PPQ) - 3.0) < 1e-9);
    CHECK_EQ (s.tickAtSeconds (3.0), 5 * PPQ);
    CHECK_EQ (s.tickAtSeconds (1.0), 2 * PPQ);
}

TEST ("instruments: every one is whole, and a violin is not a viola")
{
    for (const auto& i : instruments())
    {
        CHECK (i.low < i.high);
        CHECK (i.sweetLow >= i.low && i.sweetHigh <= i.high);
        CHECK (! i.staves.empty());
        CHECK (i.program >= 0 && i.program < 128);
    }
    const auto& vln = instrumentById ("vln1");
    const auto& vla = instrumentById ("vla");
    CHECK (vln.staves.front() == Clef::treble);
    CHECK (vla.staves.front() == Clef::alto);
    CHECK (vla.low < vln.low);
    CHECK (vla.role != vln.role);
    CHECK_EQ (instrumentById ("no such thing").id, std::string ("pno"));
    CHECK_EQ (instrumentById ("cl").writtenPitch (60, true), 62);   // a clarinet in Bb reads a tone up
    CHECK_EQ (instrumentById ("cl").writtenPitch (60, false), 60);
    CHECK_EQ (instrumentById ("cb").writtenPitch (40, false), 52);  // the double bass always an octave up
}

TEST ("spelling: F# major spells its seventh E#, harmonic minor keeps the minor signature")
{
    const auto fs = keyContext (rootIndexByName ("F#"), scaleIndexByName ("Major"));
    CHECK_EQ (fs.signature.count, 6);
    CHECK (! fs.signature.flats);
    CHECK_EQ (pitchName (65, fs), std::string ("E#4"));

    const auto db = keyContext (rootIndexByName ("Db"), scaleIndexByName ("Major"));
    CHECK_EQ (db.signature.count, 5);
    CHECK (db.signature.flats);

    const auto am = keyContext (rootIndexByName ("A"), scaleIndexByName ("Harmonic Minor"));
    CHECK_EQ (am.signature.count, 0);
    CHECK_EQ (pitchName (68, am), std::string ("G#4"));

    // Cb4 is filed under the C it is written on, not the B below.
    const auto cb = keyContext (rootIndexByName ("Cb"), scaleIndexByName ("Major"));
    CHECK_EQ (pitchName (59, cb), std::string ("Cb4"));
    CHECK_EQ (spell (59, cb).step, middleCStep);
}

TEST ("edit: writing a note overwrites its voice, a chord adds to it")
{
    auto s = twoParts();
    const auto pid = s.parts[0].id;
    writeNote (s, pid, 0, 2 * PPQ, 60, 0);
    writeNote (s, pid, PPQ, PPQ, 62, 0);          // cuts the half note to a quarter
    CHECK_EQ (s.parts[0].notes.size(), size_t (2));
    CHECK_EQ (s.parts[0].notes[0].length, PPQ);
    addToChord (s, pid, PPQ, PPQ * 4, 65, 0);     // takes the chord's length, not its own
    CHECK_EQ (s.parts[0].notes.size(), size_t (3));
    CHECK_EQ (s.parts[0].notes[2].length, PPQ);
    writeNote (s, pid, PPQ, PPQ, 67, 1);          // another voice leaves it alone
    CHECK_EQ (s.parts[0].notes.size(), size_t (4));
    writeRest (s, pid, 0, 4 * PPQ, 0);
    CHECK_EQ (s.parts[0].notes.size(), size_t (1));
}

TEST ("edit: transposing refuses to leave the MIDI range")
{
    auto s = twoParts();
    const auto id = writeNote (s, s.parts[0].id, 0, PPQ, 120, 0);
    CHECK (! transposeNotes (s, { id }, 12));
    CHECK_EQ (s.parts[0].notes[0].pitch, 120);
    CHECK (transposeNotes (s, { id }, -12));
    CHECK_EQ (s.parts[0].notes[0].pitch, 108);
}

TEST ("edit: deleting bars pulls the music after them back")
{
    auto s = twoParts();
    writeNote (s, s.parts[0].id, 0, PPQ, 60, 0);
    writeNote (s, s.parts[0].id, 4 * PPQ, PPQ, 62, 0);
    writeNote (s, s.parts[0].id, 8 * PPQ, PPQ, 64, 0);
    s.bars = 4;
    deleteBars (s, 1, 1);
    CHECK_EQ (s.parts[0].notes.size(), size_t (2));
    CHECK_EQ (s.parts[0].notes[1].start, 4 * PPQ);
    CHECK_EQ (s.parts[0].notes[1].pitch, 64);
    insertBars (s, 0, 2);
    CHECK_EQ (s.parts[0].notes[0].start, 8 * PPQ);
}

TEST ("edit: a chord's recorded root moves with inserted bars and goes with deleted ones (0046)")
{
    Score s;
    s.bars = 4;
    s.chordRoots = { { 0, 4 * PPQ, 0, { 0, 4, 7 } }, { 8 * PPQ, 12 * PPQ, 7, { 2, 7, 11 } } };
    insertBars (s, 0, 1);
    CHECK_EQ (s.chordRoots[0].start, 4 * PPQ);
    CHECK_EQ (s.chordRoots[1].end, 16 * PPQ);
    deleteBars (s, 1, 1);                                    // the first chord's bar
    CHECK_EQ (s.chordRoots.size(), size_t (1));
    CHECK_EQ (s.chordRoots[0].start, 8 * PPQ);
    CHECK_EQ (s.chordRoots[0].root, 7);
}

TEST ("file: a score survives saving and loading")
{
    auto s = twoParts();
    s.title = "Test \"quoted\"";
    s.keys = { { 0, rootIndexByName ("Eb"), scaleIndexByName ("Dorian") }, { 4, rootIndexByName ("F#"), 0 } };
    s.meters = { { 0, 3, 4 }, { 2, 7, 8 } };
    s.tempos = { { 0, 96.5 } };
    writeNote (s, s.parts[0].id, 0, PPQ, 61, 0, 90);
    writeNote (s, s.parts[1].id, PPQ / 3, PPQ * 2 / 3, 40, 1);
    s.parts[1].mute = true;
    s.chordRoots = { { 0, 4 * PPQ, 9, { 0, 4, 7, 9 } } };   // decision 0046
    const auto text = saveScore (s);
    const auto back = loadScore (text);
    CHECK (back.ok);
    CHECK_EQ (back.score.title, s.title);
    CHECK_EQ (back.score.parts.size(), size_t (2));
    CHECK_EQ (back.score.parts[1].instrument, std::string ("vc"));
    CHECK (back.score.parts[1].mute);
    CHECK_EQ (back.score.parts[0].notes[0].velocity, 90);
    CHECK_EQ (back.score.parts[1].notes[0].start, PPQ / 3);
    CHECK_EQ (back.score.parts[1].notes[0].voice, 1);
    CHECK (back.score.keys[0] == s.keys[0]);
    CHECK (back.score.keys[1] == s.keys[1]);
    CHECK (back.score.meters[1] == s.meters[1]);
    CHECK (std::abs (back.score.tempos[0].bpm - 96.5) < 1e-9);
    CHECK_EQ (back.score.chordRoots.size(), size_t (1));
    CHECK (back.score.chordRoots == s.chordRoots);
    CHECK (! loadScore ("{ \"format\": \"something else\" }").ok);
    CHECK (! loadScore ("not json").ok);
}

TEST ("autocc: a held string note swells to the preset's peak and falls back to the floor")
{
    Score s;
    Part p; p.id = s.newId(); p.instrument = "vln1";
    Note n; n.start = 0; n.length = 8 * PPQ; n.pitch = 67; n.velocity = 127; n.id = s.newId();
    p.notes = { n };
    s.parts = { p };
    const auto events = autoCC (s, s.parts[0]);
    CHECK (! events.empty());
    int peak = 0, last = -1;
    for (const auto& e : events)
        if (e.controller == 1) { peak = std::max (peak, e.value); last = e.value; }
    // Strings CC1: floor 0, peak 102, sustain 89, velocity scaling 35% at full velocity.
    CHECK (peak >= 100 && peak <= 102);
    CHECK_EQ (last, 0);

    Part piano = p;
    piano.instrument = "pno";
    CHECK (autoCC (s, piano).empty());
}
