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

namespace
{
// One violin part of the given notes, as { start, length } in ticks (120 bpm,
// so PPQ is half a second), all at one velocity.
Score violinNotes (std::vector<std::pair<Tick, Tick>> spans, int velocity = 127)
{
    Score s;
    Part p; p.id = s.newId(); p.instrument = "vln1";
    for (const auto& [start, length] : spans)
    {
        Note n; n.start = start; n.length = length; n.pitch = 67; n.velocity = velocity; n.id = s.newId();
        p.notes.push_back (n);
    }
    s.parts = { p };
    return s;
}

std::vector<CCEvent> lane (const std::vector<CCEvent>& events, int controller)
{
    std::vector<CCEvent> out;
    for (const auto& e : events) if (e.controller == controller) out.push_back (e);
    return out;
}
} // namespace

TEST ("autocc: a held string note starts at the dynamic, swells to the preset's peak and falls back")
{
    auto s = violinNotes ({ { 0, 8 * PPQ } });
    const auto cc1 = lane (autoCC (s, s.parts[0]), 1);
    CHECK (! cc1.empty());
    int peak = 0;
    for (const auto& e : cc1) peak = std::max (peak, e.value);
    // Strings CC1 (Lorne Balfe's, AutoCC 1.3): floor 85, peak 116, held.
    CHECK_EQ (cc1.front().value, 85);
    CHECK_EQ (peak, 116);
    CHECK_EQ (cc1.back().value, 85);

    Part piano = s.parts[0];
    piano.instrument = "pno";
    CHECK (autoCC (s, piano).empty());
}

TEST ("autocc: CC7 and CC21 are held levels, and CC11 ignores velocity")
{
    auto loud = violinNotes ({ { 0, 8 * PPQ } }, 127);
    auto soft = violinNotes ({ { 0, 8 * PPQ } }, 30);
    const auto a = autoCC (loud, loud.parts[0]), b = autoCC (soft, soft.parts[0]);
    for (const auto& e : lane (a, 7)) CHECK_EQ (e.value, 94);
    for (const auto& e : lane (a, 21)) CHECK_EQ (e.value, 92);
    const auto la = lane (a, 11), lb = lane (b, 11);
    CHECK_EQ (la.size(), lb.size());
    for (size_t i = 0; i < std::min (la.size(), lb.size()); ++i) CHECK_EQ (la[i].value, lb[i].value);
    // CC1 still feels velocity a little: 10% of the swell.
    int peakSoft = 0;
    for (const auto& e : lane (b, 1)) peakSoft = std::max (peakSoft, e.value);
    CHECK (peakSoft >= 113 && peakSoft <= 115);
}

TEST ("autocc: a note within 0.4 s of the last key-up carries the line on; after a longer gap it swells again")
{
    const auto peakAfter = [] (const Score& s, Tick from)
    {
        int startValue = -1, peak = 0;
        for (const auto& e : lane (autoCC (s, s.parts[0]), 1))
        {
            if (e.at <= from) startValue = e.value;
            else peak = std::max (peak, e.value);
        }
        return std::make_pair (startValue, peak);
    };
    // 3 s note, 0.2 s gap, 3 s note: the second note holds where the first left off.
    const Tick second = 6 * PPQ + 384;
    auto close = violinNotes ({ { 0, 6 * PPQ }, { second, 6 * PPQ } });
    const auto [heldAt, heldPeak] = peakAfter (close, second);
    CHECK (heldPeak <= heldAt);
    // The same with a 1 s gap: a new swell.
    const Tick later = 6 * PPQ + 2 * PPQ;
    auto apart = violinNotes ({ { 0, 6 * PPQ }, { later, 6 * PPQ } });
    const auto [restartAt, restartPeak] = peakAfter (apart, later);
    CHECK (restartPeak >= restartAt + 10);
    // Retrigger still swells on every note.
    AutoCCOptions retrigger;
    retrigger.retrigger = true;
    int retrigPeak = 0, retrigAt = -1;
    for (const auto& e : lane (autoCC (close, close.parts[0], retrigger), 1))
    {
        if (e.at <= second) retrigAt = e.value;
        else retrigPeak = std::max (retrigPeak, e.value);
    }
    CHECK (retrigPeak > retrigAt);
}

TEST ("autocc: Dynamic moves CC1 and CC11 together and leaves CC7 alone")
{
    auto s = violinNotes ({ { 0, 8 * PPQ } });
    AutoCCOptions quiet;
    quiet.dynamic = -40;
    const auto e = autoCC (s, s.parts[0], quiet);
    const auto cc1 = lane (e, 1);
    int peak = 0;
    for (const auto& x : cc1) peak = std::max (peak, x.value);
    CHECK_EQ (cc1.front().value, 45);
    CHECK_EQ (peak, 76);                       // the same +31 swell
    const auto cc11 = lane (e, 11);
    CHECK_EQ (cc11.front().value, 54);
    for (const auto& x : lane (e, 7)) CHECK_EQ (x.value, 94);
}
