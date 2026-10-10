#include "Check.h"

#include "Detect.h"
#include "Spelling.h"

#include <algorithm>

using namespace nt;

namespace
{
Score withChords (const std::vector<std::vector<int>>& chords, Tick each)
{
    Score s;
    Part p; p.id = s.newId(); p.instrument = "pno";
    Tick t = 0;
    for (const auto& c : chords)
    {
        for (int pitch : c)
        {
            Note n; n.start = t; n.length = each; n.pitch = pitch; n.id = s.newId();
            p.notes.push_back (n);
        }
        t += each;
    }
    s.parts = { p };
    s.normalise();
    return s;
}
} // namespace

TEST ("detect: a progression a bar at a time")
{
    const auto s = withChords ({ { 48, 60, 64, 67 }, { 45, 60, 64, 69 }, { 41, 60, 65, 69 }, { 43, 59, 62, 67 } }, 4 * PPQ);
    const auto chords = detectChords (s, 0, s.endTick());
    CHECK_EQ (chords.size(), size_t (4));
    if (chords.size() == 4)
    {
        CHECK_EQ (chords[0].name, std::string ("C"));
        CHECK_EQ (chords[1].name, std::string ("Amin"));
        CHECK_EQ (chords[2].name, std::string ("F"));
        CHECK_EQ (chords[3].name, std::string ("G"));
        CHECK_EQ (chords[1].start, 4 * PPQ);
    }
}

TEST ("detect: a chord held over a moving tune stays one chord")
{
    auto s = withChords ({ { 48, 52, 55 } }, 4 * PPQ);
    // A tune over it: C D E D, quarters.
    int pitches[] = { 72, 74, 76, 74 };
    for (int i = 0; i < 4; ++i)
    {
        Note n; n.start = i * PPQ; n.length = PPQ; n.pitch = pitches[i]; n.id = s.newId();
        s.parts[0].notes.push_back (n);
    }
    s.sortNotes();
    const auto chords = detectChords (s, 0, s.endTick());
    CHECK_EQ (chords.size(), size_t (1));
    if (! chords.empty()) CHECK_EQ (chords[0].name, std::string ("C"));
}

TEST ("detect: an inversion reads as a slash chord")
{
    const auto s = withChords ({ { 52, 60, 67 } }, 4 * PPQ);
    const auto chords = detectChords (s, 0, s.endTick());
    CHECK_EQ (chords.size(), size_t (1));
    if (! chords.empty()) CHECK_EQ (chords[0].name, std::string ("C/E"));
}

TEST ("detect: the key of a tune")
{
    // Twinkle in G: G G D D E E D, C C B B A A G.
    std::vector<TimedPitch> tune;
    int ps[] = { 67, 67, 74, 74, 76, 76, 74, 72, 72, 71, 71, 69, 69, 67 };
    for (int i = 0; i < 14; ++i) tune.push_back ({ ps[i], i * PPQ, PPQ });
    const auto ranked = rankKeys (tune, 7, 7);
    CHECK_EQ (std::string (scaleview::roots[static_cast<size_t> (ranked.front().root)].name), std::string ("G"));
    CHECK_EQ (ranked.front().scale, 0);
}

TEST ("detect: a minor key over a whole score")
{
    // i iv V i in A minor.
    const auto s = withChords ({ { 45, 57, 60, 64 }, { 50, 57, 62, 65 }, { 52, 56, 59, 64 }, { 45, 57, 60, 64 } }, 4 * PPQ);
    const auto keys = detectKeys (s);
    CHECK_EQ (keys.size(), size_t (1));
    if (! keys.empty()) CHECK_EQ (keys[0].label, std::string ("A minor"));
}

TEST ("detect: naming what sounds at one moment")
{
    CHECK_EQ (nameSounding ({ 60, 64, 67, 70 }, 0, 0), std::string ("C7"));
    CHECK_EQ (nameSounding ({ 62, 65, 69, 72 }, 0, 0), std::string ("Dmin7"));
}

//  ScaleView Pro, the reference, October 2026: an altered dominant on its own
//  root keeps its alterations, and a draw goes to the reading with no slash.
//  Arrives through ScaleModel.h, unchanged from the plugin, which ports Pro.
TEST ("detect: ScaleView Pro's altered dominants and no-slash draws")
{
    CHECK_EQ (nameSounding ({ 48, 64, 68, 70, 73 }, 0, 0), std::string ("Caug7b9"));   // C7#5b9
    CHECK_EQ (nameSounding ({ 48, 64, 66, 70, 75 }, 0, 0), std::string ("C7b5#9"));
    CHECK_EQ (nameSounding ({ 48, 62, 67, 70 }, 0, 0), std::string ("C7sus2"));        // was GminAdd11/C
    CHECK_EQ (nameSounding ({ 55, 57, 62, 65, 67 }, 0, 0), std::string ("G7sus2"));    // was DminAdd11/G
}

TEST ("detect: the key signature settles a near tie")
{
    // C E A G: as much A minor as C major.
    Score s;
    Part p; p.id = s.newId(); p.instrument = "fl";
    int ps[] = { 72, 76, 69, 67 };
    for (int i = 0; i < 4; ++i) { Note n; n.start = i * 2 * PPQ; n.length = 2 * PPQ; n.pitch = ps[i]; n.id = s.newId(); p.notes.push_back (n); }
    s.parts = { p };
    s.normalise();
    const auto leaning = detectKeys (s, true);
    CHECK_EQ (leaning.front().label, std::string ("C major"));
    // A clear minor tune is still heard as minor, whatever the signature says.
    const auto minor = withChords ({ { 45, 57, 60, 64 }, { 52, 56, 59, 64 }, { 45, 57, 60, 64 } }, 4 * PPQ);
    const auto minorKeys = detectKeys (minor, true);   // kept: a reference into a temporary would dangle
    CHECK (! minorKeys.empty());
    if (! minorKeys.empty()) CHECK_EQ (minorKeys.front().label, std::string ("A minor"));
}

TEST ("detect: a tune on its own is not named as chords")
{
    Score s;
    Part p; p.id = s.newId(); p.instrument = "fl";
    int ps[] = { 72, 74, 76, 77, 79, 77, 76, 74 };
    for (int i = 0; i < 8; ++i) { Note n; n.start = i * PPQ / 2; n.length = PPQ / 2; n.pitch = ps[i]; n.id = s.newId(); p.notes.push_back (n); }
    s.parts = { p };
    s.normalise();
    for (const auto& c : detectChords (s, 0, s.endTick())) CHECK_EQ (c.name, std::string());
}

TEST ("detect: an arpeggio is a chord, a scale run is not")
{
    Score s;
    Part p; p.id = s.newId(); p.instrument = "pno";
    int arp[] = { 48, 52, 55, 60 };
    for (int i = 0; i < 4; ++i) { Note n; n.start = i * PPQ; n.length = PPQ / 2; n.pitch = arp[i]; n.id = s.newId(); p.notes.push_back (n); }
    int run[] = { 60, 62, 64, 65, 67, 69, 71, 72 };
    for (int i = 0; i < 8; ++i) { Note n; n.start = 4 * PPQ + i * PPQ / 2; n.length = PPQ / 2; n.pitch = run[i]; n.id = s.newId(); p.notes.push_back (n); }
    s.parts = { p };
    s.normalise();
    const auto chords = detectChords (s, 0, s.endTick());
    CHECK (! chords.empty());
    if (! chords.empty()) CHECK_EQ (chords.front().name, std::string ("C"));
    for (const auto& c : chords) if (c.start >= 4 * PPQ) CHECK_EQ (c.name, std::string());
}

TEST ("detect: a chord made on a known root is named from it, until its notes change (0046)")
{
    // C E G A over C: the reader calls it C6. Made as vi7 in first inversion,
    // it is Amin7/C; made as I with an added sixth, C6.
    auto s = withChords ({ { 60, 64, 67, 69 }, { 60, 64, 67, 69 } }, 4 * PPQ);
    const auto plain = detectChords (s, 0, 8 * PPQ);                // held: no references into temporaries
    if (! plain.empty()) CHECK_EQ (plain.front().name, std::string ("C6"));
    s.chordRoots.push_back ({ 0, 4 * PPQ, 9, { 0, 4, 7, 9 } });
    const auto named = detectChords (s, 0, 8 * PPQ);
    CHECK_EQ (named.size(), size_t (2));
    if (named.size() != 2) return;
    CHECK_EQ (named[0].name, std::string ("Amin7/C"));
    CHECK_EQ (named[0].end, 4 * PPQ);
    CHECK_EQ (named[1].name, std::string ("C6"));           // the second bar has no root recorded

    // Rests after a chord hold it: the same chord with nothing after reads
    // Amin7/C to the end, in one piece.
    auto alone = withChords ({ { 60, 64, 67, 69 } }, 4 * PPQ);
    alone.bars = 4;
    alone.chordRoots.push_back ({ 0, 4 * PPQ, 9, { 0, 4, 7, 9 } });
    const auto held = detectChords (alone, 0, alone.endTick());
    CHECK_EQ (held.size(), size_t (1));
    if (! held.empty()) CHECK_EQ (held.front().name, std::string ("Amin7/C"));

    // Edited - the A taken out - it is read like any other chord.
    auto& notes = s.parts[0].notes;
    notes.erase (std::remove_if (notes.begin(), notes.end(), [] (const Note& n) { return n.start == 0 && n.pitch == 69; }), notes.end());
    const auto edited = detectChords (s, 0, 8 * PPQ);
    CHECK (! edited.empty());
    if (! edited.empty()) CHECK_EQ (edited.front().name, std::string ("C"));

    // Named from a root in ScaleView's words, the bass after a slash.
    CHECK_EQ (nameFromRoot ({ 64, 67, 71, 72 }, 4, 0, 0), std::string ("Eminb6"));
    CHECK_EQ (nameFromRoot ({ 64, 67, 71, 72 }, 0, 0, 0), std::string ("Cmaj7/E"));
    CHECK_EQ (nameFromRoot ({ 62, 67, 72 }, 0, 0, 0), std::string ("Csus2/D"));
    CHECK_EQ (nameFromRoot ({ 60, 67 }, 0, 0, 0), std::string ("C5"));
}
