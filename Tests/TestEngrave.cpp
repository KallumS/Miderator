#include "Check.h"

#include <chrono>

#include "Engrave.h"
#include "Spelling.h"

using namespace nt;
using namespace nt::engrave;

namespace
{
Score oneStaff (const std::string& instrument = "fl")
{
    Score s;
    Part p; p.id = s.newId(); p.name = "Part"; p.instrument = instrument;
    s.parts = { p };
    s.bars = 2;
    return s;
}

void add (Score& s, Tick start, Tick length, int pitch, size_t part = 0, int voice = 0)
{
    Note n; n.start = start; n.length = length; n.pitch = pitch; n.voice = voice; n.id = s.newId();
    s.parts[part].notes.push_back (n);
    s.sortNotes();
}

std::vector<const Element*> notesOf (const Staff& st)
{
    std::vector<const Element*> out;
    for (const auto& el : st.elements) if (! el.rest) out.push_back (&el);
    return out;
}
} // namespace

TEST ("engrave: an empty bar is one whole-bar rest")
{
    auto s = oneStaff();
    const auto lay = layout (s);
    CHECK_EQ (lay.staves.size(), size_t (1));
    int restsInBar0 = 0;
    for (const auto& el : lay.staves[0].elements)
        if (el.measure == 0) { CHECK (el.measureRest); ++restsInBar0; }
    CHECK_EQ (restsInBar0, 1);
}

TEST ("engrave: four quarters, unbeamed, stems by position")
{
    auto s = oneStaff();
    for (int i = 0; i < 4; ++i) add (s, i * PPQ, PPQ, i < 2 ? 64 : 84);
    const auto lay = layout (s);
    const auto notes = notesOf (lay.staves[0]);
    CHECK_EQ (notes.size(), size_t (4));
    for (const auto* el : notes) { CHECK_EQ (el->den, 4); CHECK_EQ (el->beam, -1); }
    CHECK (notes[0]->stem == Stem::up);    // E4, the bottom line
    CHECK (notes[3]->stem == Stem::down);  // C6, above the staff
    CHECK_EQ (notes[0]->heads[0].pos, 0);
}

TEST ("engrave: eighths are beamed a beat at a time")
{
    auto s = oneStaff();
    for (int i = 0; i < 8; ++i) add (s, i * PPQ / 2, PPQ / 2, 72);
    const auto lay = layout (s);
    CHECK_EQ (lay.staves[0].beams.size(), size_t (4));
    for (const auto& b : lay.staves[0].beams) CHECK_EQ (b.elements.size(), size_t (2));
}

TEST ("engrave: sixteenths carry a second beam")
{
    auto s = oneStaff();
    for (int i = 0; i < 4; ++i) add (s, i * PPQ / 4, PPQ / 4, 72);
    const auto lay = layout (s);
    CHECK_EQ (lay.staves[0].beams.size(), size_t (1));
    CHECK_EQ (lay.staves[0].beams[0].segments.size(), size_t (1));
    CHECK_EQ (lay.staves[0].beams[0].segments[0].level, 1);
}

TEST ("engrave: a note across the bar line is tied")
{
    auto s = oneStaff();
    add (s, 3 * PPQ, 2 * PPQ, 72);
    const auto lay = layout (s);
    const auto notes = notesOf (lay.staves[0]);
    CHECK_EQ (notes.size(), size_t (2));
    CHECK (notes[0]->tieOut);
    CHECK (notes[1]->tieIn);
    CHECK_EQ (notes[1]->measure, 1);
    CHECK_EQ (lay.staves[0].ties.size(), size_t (1));
}

TEST ("engrave: a gated note is written to the next one")
{
    auto s = oneStaff();
    add (s, 0, PPQ * 9 / 10, 72);      // 90% gate
    add (s, PPQ, PPQ * 9 / 10, 74);
    add (s, 2 * PPQ, 2 * PPQ * 9 / 10, 76);
    const auto lay = layout (s);
    const auto notes = notesOf (lay.staves[0]);
    CHECK_EQ (notes.size(), size_t (3));
    CHECK_EQ (notes[0]->den, 4);
    CHECK_EQ (notes[2]->den, 2);   // the last rounds up to the bar's end
    for (const auto& el : lay.staves[0].elements) if (el.measure == 0) CHECK (! el.rest);
}

TEST ("engrave: triplet eighths are found and numbered")
{
    auto s = oneStaff();
    for (int i = 0; i < 3; ++i) add (s, i * PPQ / 3, PPQ / 3, 72);
    add (s, PPQ, PPQ, 72);
    const auto lay = layout (s);
    const auto notes = notesOf (lay.staves[0]);
    CHECK_EQ (notes[0]->tuplet, 3);
    CHECK_EQ (notes[0]->den, 8);
    CHECK_EQ (lay.staves[0].tuplets.size(), size_t (1));
    CHECK (! lay.staves[0].tuplets[0].bracket);   // the beam already shows the group
}

TEST ("engrave: rests show the beat")
{
    auto s = oneStaff();
    add (s, 0, PPQ / 2, 72);   // an eighth, then three and a half beats of rest
    const auto lay = layout (s);
    std::vector<int> rests;
    for (const auto& el : lay.staves[0].elements)
        if (el.measure == 0 && el.rest) rests.push_back (el.den);
    // An eighth rest to finish the beat, a quarter for beat two, a half for three and four.
    CHECK_EQ (rests.size(), size_t (3));
    if (rests.size() == 3) { CHECK_EQ (rests[0], 8); CHECK_EQ (rests[1], 4); CHECK_EQ (rests[2], 2); }
}

TEST ("engrave: an accidental lasts to the bar line and no further")
{
    auto s = oneStaff();
    add (s, 0, PPQ, 66);         // F#
    add (s, PPQ, PPQ, 66);       // F# again: no sharp
    add (s, 2 * PPQ, PPQ, 65);   // F natural: a natural
    add (s, 4 * PPQ, PPQ, 66);   // next bar: the sharp again
    const auto lay = layout (s);
    const auto notes = notesOf (lay.staves[0]);
    CHECK_EQ (notes.size(), size_t (4));
    CHECK (notes[0]->heads[0].showAccidental);
    CHECK (! notes[1]->heads[0].showAccidental);
    CHECK (notes[2]->heads[0].showAccidental);
    CHECK_EQ (notes[2]->heads[0].accidental, 0);
    CHECK (notes[3]->heads[0].showAccidental);
}

TEST ("engrave: the key signature takes the accidentals off its own notes")
{
    auto s = oneStaff();
    s.keys = { { 0, rootIndexByName ("D"), 0 } };
    add (s, 0, PPQ, 66);   // F# in D major
    add (s, PPQ, PPQ, 61); // C#
    const auto lay = layout (s);
    const auto notes = notesOf (lay.staves[0]);
    CHECK (! notes[0]->heads[0].showAccidental);
    CHECK (! notes[1]->heads[0].showAccidental);
    CHECK_EQ (lay.measures[0].key.count, 2);
}

TEST ("engrave: simultaneous notes on different staves share a column")
{
    Score s;
    Part a; a.id = s.newId(); a.instrument = "fl";
    Part b; b.id = s.newId(); b.instrument = "vc";
    s.parts = { a, b };
    s.bars = 1;
    add (s, 0, PPQ / 2, 72, 0);
    add (s, PPQ / 2, PPQ / 2, 74, 0);
    add (s, PPQ, 3 * PPQ, 76, 0);
    add (s, 0, 2 * PPQ, 48, 1);
    add (s, 2 * PPQ, 2 * PPQ, 43, 1);
    const auto lay = layout (s);
    CHECK_EQ (lay.staves.size(), size_t (2));
    const auto fl = notesOf (lay.staves[0]);
    const auto vc = notesOf (lay.staves[1]);
    CHECK (std::abs (fl[0]->x - vc[0]->x) < 1e-9);
    CHECK (lay.staves[1].top > lay.staves[0].top + 4.0);
    // The cello's half note on beat three lines up with where the flute
    // would be at beat three, between its notes.
    CHECK (std::abs (lay.xForTick (2 * PPQ) - vc[1]->x) < 1e-9);
}

TEST ("engrave: a piano splits at middle C onto two staves")
{
    auto s = oneStaff ("pno");
    add (s, 0, PPQ, 72);
    add (s, 0, PPQ, 48);
    const auto lay = layout (s);
    CHECK_EQ (lay.staves.size(), size_t (2));
    CHECK (lay.staves[0].clef == Clef::treble);
    CHECK (lay.staves[1].clef == Clef::bass);
    CHECK_EQ (notesOf (lay.staves[0]).size(), size_t (1));
    CHECK_EQ (notesOf (lay.staves[1]).size(), size_t (1));
    CHECK_EQ (notesOf (lay.staves[1])[0]->heads[0].pos, 3);   // C3, the second space of the bass staff
}

TEST ("engrave: the viola reads in the alto clef, the cello in the bass")
{
    auto s = oneStaff ("vla");
    add (s, 0, PPQ, 60);   // middle C is the middle line of the alto clef
    const auto lay = layout (s);
    CHECK_EQ (notesOf (lay.staves[0])[0]->heads[0].pos, 4);
}

TEST ("engrave: notes the instrument cannot play are flagged")
{
    auto s = oneStaff ("vln1");
    add (s, 0, PPQ, 50);   // below the G string
    add (s, PPQ, PPQ, 60);
    add (s, PPQ * 2, PPQ, 98);   // high, outside the sweet register
    add (s, PPQ * 2, PPQ, 60);   // and a chord, on a section that plays one line
    const auto lay = layout (s);
    const auto notes = notesOf (lay.staves[0]);
    CHECK (notes[0]->heads[0].outOfRange);
    CHECK (! notes[1]->heads[0].outOfRange);
    CHECK (notes[2]->heads.back().outsideSweet);
    CHECK (notes[2]->tooManyNotes);
}

TEST ("engrave: a crossed head and its accidental do not collide")
{
    auto s = oneStaff();
    add (s, 0, PPQ, 73);
    add (s, 0, PPQ, 74);   // C# and D: a second, so one head crosses the stem
    const auto lay = layout (s);
    const auto notes = notesOf (lay.staves[0]);
    CHECK_EQ (notes[0]->heads.size(), size_t (2));
    int crossed = 0;
    for (const auto& h : notes[0]->heads) crossed += h.side;
    CHECK_EQ (crossed, 1);
    for (const auto& h : notes[0]->heads)
        if (h.showAccidental) CHECK (notes[0]->x + h.accidentalX + 0.9 <= std::min (notes[0]->heads[0].x, notes[0]->heads[1].x) + 1e-9);
}

TEST ("engrave: splitting durations")
{
    // A half note on beat two of 3/4 is a half note.
    auto v = splitDuration (PPQ, 2 * PPQ, 3 * PPQ, false, PPQ);
    CHECK_EQ (v.size(), size_t (1));
    // Five eighths have no symbol: a half tied to an eighth.
    v = splitDuration (0, 5 * PPQ / 2, 4 * PPQ, false, PPQ);
    CHECK_EQ (v.size(), size_t (2));
    // A rest of three beats from beat two of 4/4: a quarter, then a half.
    v = splitDuration (PPQ, 3 * PPQ, 4 * PPQ, true, PPQ);
    CHECK_EQ (v.size(), size_t (2));
    if (v.size() == 2) { CHECK_EQ (v[0].den, 4); CHECK_EQ (v[1].den, 2); }
}

TEST ("engrave: clicking a line gives the key's pitch on it")
{
    const auto d = keyContext (rootIndexByName ("D"), 0);
    const auto& fl = instrumentById ("fl");
    CHECK_EQ (pitchAtPosition (1, Clef::treble, d, fl, false), 66);   // F# in the first space
    CHECK_EQ (pitchAtPosition (-2, Clef::treble, d, fl, false), 61);  // the ledger line under the staff is C# in D
    const auto c = keyContext (0, 0);
    CHECK_EQ (pitchAtPosition (-2, Clef::treble, c, fl, false), 60);  // and plain middle C in C
    const auto& cl = instrumentById ("cl");
    CHECK_EQ (pitchAtPosition (-2, Clef::treble, c, cl, true), 58);   // a written C on a Bb clarinet sounds Bb
}

TEST ("engrave: a unison of two spellings puts the heads side by side")
{
    auto s = oneStaff();
    add (s, 0, PPQ, 72);
    add (s, 0, PPQ, 73);   // C and C#, both on the C space
    const auto lay = layout (s);
    const auto notes = notesOf (lay.staves[0]);
    CHECK_EQ (notes[0]->heads.size(), size_t (2));
    CHECK_EQ (notes[0]->heads[0].pos, notes[0]->heads[1].pos);
    CHECK (std::abs (notes[0]->heads[0].x - notes[0]->heads[1].x) > 1.0);
    CHECK (notes[0]->heads[0].showAccidental && notes[0]->heads[1].showAccidental);
}

TEST ("engrave: two hundred bars of eight busy parts lay out in well under a second")
{
    Score s;
    for (const char* id : { "fl", "ob", "cl", "bsn", "vln1", "vln2", "vla", "pno" })
    {
        Part p; p.id = s.newId(); p.instrument = id;
        s.parts.push_back (p);
    }
    for (size_t pi = 0; pi < s.parts.size(); ++pi)
        for (int i = 0; i < 200 * 16; ++i)
        {
            Note n; n.start = i * PPQ / 4; n.length = PPQ / 4; n.pitch = 60 + (i * 7 + static_cast<int> (pi)) % 24; n.id = s.newId();
            s.parts[pi].notes.push_back (n);
        }
    s.normalise();
    const auto t0 = std::chrono::steady_clock::now();
    const auto lay = layout (s);
    const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    std::printf ("  (200 bars x 8 parts, 25600 notes: %.3f s)\n", secs);
    CHECK_EQ (lay.measures.size(), size_t (200));
    CHECK (secs < 1.0);
}
