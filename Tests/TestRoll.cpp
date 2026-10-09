#include "Check.h"

#include "Roll.h"

using namespace nt;

namespace
{
Score flute()
{
    Score s;
    Part p; p.id = s.newId(); p.name = "Flute"; p.instrument = "fl";
    s.parts = { p };
    return s;
}

Note note (Tick start, Tick length, int pitch)
{
    Note n;
    n.start = start;
    n.length = length;
    n.pitch = pitch;
    return n;
}

void add (Score& s, std::initializer_list<Note> notes)
{
    for (auto n : notes)
    {
        n.id = s.newId();
        s.parts[0].notes.push_back (n);
    }
    s.sortNotes();
    s.fitBars();
}
} // namespace

TEST ("roll: the grid snaps from each bar line, and a triplet grid fits three in two")
{
    Score s;
    s.meters = { { 0, 7, 8 } };
    roll::Grid g;
    g.base = PPQ;   // quarters in 7/8: the bar line falls between two of them
    const Tick bar = 7 * PPQ / 2;
    CHECK_EQ (roll::snap (s, PPQ + 100, g), PPQ);
    CHECK_EQ (roll::snap (s, PPQ + 600, g), 2 * PPQ);
    CHECK_EQ (roll::snap (s, bar - 100, g), bar);              // the bar line, not the quarter past it
    CHECK_EQ (roll::snap (s, bar + PPQ - 100, g), bar + PPQ);  // counted from bar 2's line
    CHECK_EQ (roll::snapDown (s, bar + PPQ - 1, g), bar);
    g.snap = false;
    CHECK_EQ (roll::snap (s, 1234, g), Tick (1234));
    g = {};
    g.base = PPQ / 2;
    g.triplet = true;
    CHECK_EQ (g.step(), PPQ / 3);
    CHECK_EQ (g.name(), std::string ("1/8T"));
}

TEST ("roll: drawing a note only takes the place of the same pitch")
{
    auto s = flute();
    add (s, { note (0, 4 * PPQ, 60), note (PPQ, PPQ, 67) });
    const auto id = roll::drawNote (s, s.parts[0].id, PPQ, PPQ, 60, 90);
    CHECK (id != 0);
    const auto& n = s.parts[0].notes;
    CHECK_EQ (n.size(), size_t (3));
    CHECK_EQ (n[0].pitch, 60);
    CHECK_EQ (n[0].length, PPQ);                 // the held C cut where the new C starts
    CHECK_EQ (n[1].pitch, 60);
    CHECK_EQ (n[1].velocity, 90);
    CHECK_EQ (n[2].pitch, 67);                   // the G untouched
    CHECK_EQ (roll::drawNote (s, s.parts[0].id, 0, PPQ, 128), uint32_t (0));
    // Drawing past the end makes room.
    roll::drawNote (s, s.parts[0].id, 20 * 4 * PPQ, PPQ, 72);
    CHECK_EQ (s.bars, 21);
}

TEST ("roll: notes move in time and pitch together, or not at all")
{
    auto s = flute();
    add (s, { note (0, PPQ, 60), note (PPQ, PPQ, 64) });
    const Selection both { s.parts[0].notes[0].id, s.parts[0].notes[1].id };
    CHECK (roll::moveNotes (s, both, PPQ / 2, 2));
    CHECK_EQ (s.parts[0].notes[0].start, PPQ / 2);
    CHECK_EQ (s.parts[0].notes[0].pitch, 62);
    CHECK (! roll::moveNotes (s, both, -PPQ, 0));     // past the start
    CHECK (! roll::moveNotes (s, both, 0, 70));       // past 127
    CHECK_EQ (s.parts[0].notes[1].pitch, 66);

    const auto copies = roll::copyNotesBy (s, both, 4 * PPQ, -12);
    CHECK_EQ (copies.size(), size_t (2));
    CHECK_EQ (s.parts[0].notes.size(), size_t (4));
    CHECK_EQ (s.parts[0].notes[2].start, 4 * PPQ + PPQ / 2);
    CHECK_EQ (s.parts[0].notes[2].pitch, 50);
}

TEST ("roll: stretching keeps a note at least the shortest length, from either end")
{
    auto s = flute();
    add (s, { note (PPQ, PPQ, 60) });
    const Selection one { s.parts[0].notes[0].id };
    roll::resizeNotes (s, one, PPQ / 2, PPQ / 4);
    CHECK_EQ (s.parts[0].notes[0].length, PPQ * 3 / 2);
    roll::resizeNotes (s, one, -4 * PPQ, PPQ / 4);
    CHECK_EQ (s.parts[0].notes[0].length, PPQ / 4);
    roll::resizeStarts (s, one, -PPQ / 2, PPQ / 4);    // the front pulled earlier
    CHECK_EQ (s.parts[0].notes[0].start, PPQ / 2);
    CHECK_EQ (s.parts[0].notes[0].end(), PPQ + PPQ / 4);
    roll::resizeStarts (s, one, -4 * PPQ, PPQ / 4);    // never before the start
    CHECK_EQ (s.parts[0].notes[0].start, Tick (0));
}

TEST ("roll: quantise pulls starts and ends to the grid and merges what lands together")
{
    auto s = flute();
    add (s, { note (10, PPQ - 30, 60), note (PPQ - 40, PPQ / 2 + 50, 62), note (PPQ + 20, 100, 62), note (2 * PPQ + 5, PPQ, 64) });
    Selection first3;
    for (size_t i = 0; i < 3; ++i) first3.insert (s.parts[0].notes[i].id);
    roll::quantise (s, first3, PPQ / 2);
    const auto& n = s.parts[0].notes;
    CHECK_EQ (n.size(), size_t (3));             // the two Ds on beat 2 are one
    CHECK_EQ (n[0].start, Tick (0));
    CHECK_EQ (n[0].length, PPQ);
    CHECK_EQ (n[1].start, PPQ);
    CHECK_EQ (n[1].pitch, 62);
    CHECK_EQ (n[1].length, PPQ / 2);             // never shorter than a step
    CHECK_EQ (n[2].start, 2 * PPQ + 5);          // not selected, not moved
}

TEST ("roll: velocities are set note by note and kept in range")
{
    auto s = flute();
    add (s, { note (0, PPQ, 60), note (PPQ, PPQ, 62) });
    roll::setVelocities (s, { { s.parts[0].notes[0].id, 30 }, { s.parts[0].notes[1].id, 200 } });
    CHECK_EQ (s.parts[0].notes[0].velocity, 30);
    CHECK_EQ (s.parts[0].notes[1].velocity, 127);
}

TEST ("roll: the warnings say what the flute cannot do")
{
    auto s = flute();   // 120 bpm: a sixteenth is 0.125 s, the flute needs 0.09
    add (s, {
        note (0, PPQ, 50),                           // below its range
        note (PPQ, PPQ, 62),                         // playable, below where it sings
        note (2 * PPQ, PPQ, 72), note (2 * PPQ, PPQ, 76),   // a chord on one line
        note (4 * PPQ, PPQ / 8, 72), note (4 * PPQ + PPQ / 8, PPQ / 8, 74),   // thirty-seconds: too fast
        note (5 * PPQ, PPQ / 2, 72), note (5 * PPQ + PPQ / 2, PPQ / 2, 89),   // a leap of 17
        note (8 * PPQ, PPQ, 72),                     // after a rest: nothing to say
    });
    const auto w = roll::check (s);
    auto of = [&] (size_t i) { const auto it = w.find (s.parts[0].notes[i].id); return it == w.end() ? roll::Warning {} : it->second; };
    CHECK (of (0).outOfRange);
    CHECK (! of (0).outsideSweet);
    CHECK (of (1).outsideSweet);
    CHECK (of (2).tooManyNotes);
    CHECK (of (3).tooManyNotes);
    CHECK (! of (4).tooFast);
    CHECK (of (5).tooFast);
    CHECK (of (7).tooWide);
    CHECK (! of (6).tooWide);
    CHECK (! of (8).any());
    CHECK (w.count (s.parts[0].notes[8].id) == 0);
}

TEST ("roll: a drum kit has no range to warn about, and its keys are named")
{
    Score s;
    Part p; p.id = s.newId(); p.instrument = "kit";
    s.parts = { p };
    add (s, { note (0, PPQ, 36), note (0, PPQ, 38), note (0, PPQ, 42), note (0, PPQ, 49), note (0, PPQ, 51), note (0, PPQ, 20) });
    CHECK (roll::check (s).empty());
    CHECK_EQ (roll::drumName (36), std::string ("Kick"));
    CHECK_EQ (roll::drumName (38), std::string ("Snare"));
    CHECK_EQ (roll::drumName (42), std::string ("Closed hi-hat"));
    CHECK_EQ (roll::drumName (81), std::string ("Open triangle"));
    CHECK_EQ (roll::drumName (34), std::string());
    CHECK_EQ (roll::keyName (60), std::string ("C4"));
    CHECK (roll::isBlackKey (61));
    CHECK (! roll::isBlackKey (64));
}
