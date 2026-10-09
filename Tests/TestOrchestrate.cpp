#include "Check.h"
#include "Orchestrate.h"
#include "Templates.h"

#include <algorithm>
#include <cstdlib>

using namespace nt;

namespace
{
Score ensemble (std::initializer_list<const char*> ids)
{
    Score s;
    for (const char* id : ids)
    {
        Part p; p.id = s.newId(); p.instrument = id; p.name = instrumentById (id).name;
        s.parts.push_back (p);
    }
    return s;
}

std::vector<uint32_t> idsOf (const Score& s)
{
    std::vector<uint32_t> ids;
    for (const auto& p : s.parts) ids.push_back (p.id);
    return ids;
}

// Block chords, a bar each, held for the bar.
GeneratedResult blockChords (std::initializer_list<std::initializer_list<int>> chords)
{
    GeneratedResult r;
    GeneratedPart c; c.name = "Chords";
    Tick at = 0;
    for (const auto& chord : chords)
    {
        for (int p : chord) { Note n; n.start = at; n.length = 4 * PPQ; n.pitch = p; c.notes.push_back (n); }
        at += 4 * PPQ;
    }
    r.parts = { c };
    r.length = at;
    return r;
}

int pcOf (int p) { return ((p % 12) + 12) % 12; }

const Note* firstNote (const Part& p)
{
    const Note* first = nullptr;
    for (const auto& n : p.notes) if (first == nullptr || n.start < first->start) first = &n;
    return first;
}

const Part& partOf (const Score& s, const char* id)
{
    for (const auto& p : s.parts) if (p.instrument == id) return p;
    return s.parts.front();
}

LuaEngine& engine()
{
    static LuaEngine e;
    return e;
}

void choose (LuaEngine& e, const GeneratorContext& ctx, const char* id, const char* value)
{
    for (const auto& st : e.settings ("good-idea", ctx))
        if (st.id == id)
            for (size_t i = 0; i < st.names.size(); ++i)
                if (st.names[i] == value) e.set ("good-idea", st.id, static_cast<int> (i), ctx);
}
} // namespace

TEST ("orchestrate: a triad in a string quintet - the top note in the violins in octaves, the middle in the viola, the bass in octaves")
{
    // C major, E on top: C3 G3 E4.
    auto s = ensemble ({ "vln1", "vln2", "vla", "vc", "cb" });
    insertIntoRange (s, blockChords ({ { 48, 55, 64 } }), idsOf (s), 0, s.barStart (1));
    for (const auto& p : s.parts) CHECK (firstNote (p) != nullptr);
    if (std::any_of (s.parts.begin(), s.parts.end(), [] (const Part& p) { return p.notes.empty(); })) return;
    const int v1 = firstNote (partOf (s, "vln1"))->pitch, v2 = firstNote (partOf (s, "vln2"))->pitch;
    const int va = firstNote (partOf (s, "vla"))->pitch;
    const int vc = firstNote (partOf (s, "vc"))->pitch, cb = firstNote (partOf (s, "cb"))->pitch;
    CHECK_EQ (pcOf (v1), 4);              // E, the top note
    CHECK_EQ (v2, v1 - 12);               // the same an octave down
    CHECK_EQ (pcOf (va), 7);              // G, the middle note
    CHECK_EQ (pcOf (vc), 0);              // C, the bass
    CHECK_EQ (cb, vc - 12);               // the bass an octave down
    CHECK (v2 > va && va > vc);
}

TEST ("orchestrate: a string quartet with seventh chords voices all four notes")
{
    // G7: G2 F3 B3 D4 - every note wanted; nobody doubles the tune.
    auto s = ensemble ({ "vln1", "vln2", "vla", "vc" });
    insertIntoRange (s, blockChords ({ { 43, 53, 59, 62 } }), idsOf (s), 0, s.barStart (1));
    std::vector<int> pcs;
    for (const auto& p : s.parts) if (const auto* n = firstNote (p)) pcs.push_back (pcOf (n->pitch));
    std::sort (pcs.begin(), pcs.end());
    CHECK (pcs == (std::vector<int> { 2, 5, 7, 11 }));
}

TEST ("orchestrate: the inner parts move to the nearest notes, between the tune and the bass")
{
    // I - IV - V - I in C, in a quintet: the viola, alone in the middle,
    // takes the note each chord lacks, never leaping further than a fifth,
    // always under the tune and over the bass.
    auto s = ensemble ({ "vln1", "vln2", "vla", "vc", "cb" });
    const auto r = blockChords ({ { 48, 55, 64, 72 }, { 53, 57, 65, 72 }, { 43, 55, 62, 71 }, { 48, 55, 64, 72 } });
    insertIntoRange (s, r, idsOf (s), 0, s.barStart (4));
    const auto& vla = partOf (s, "vla").notes;
    CHECK_EQ (vla.size(), size_t (4));
    for (size_t i = 1; i < vla.size(); ++i) CHECK (std::abs (vla[i].pitch - vla[i - 1].pitch) <= 7);
    const int lacks[] = { 4, 9, 2, 4 };                      // E, A, D, E: the third of each chord
    for (size_t i = 0; i < vla.size() && i < 4; ++i) CHECK_EQ (pcOf (vla[i].pitch), lacks[i]);
    const auto& top = partOf (s, "vln1").notes;
    const auto& bass = partOf (s, "vc").notes;
    for (size_t i = 0; i < vla.size() && i < top.size() && i < bass.size(); ++i)
        CHECK (vla[i].pitch < top[i].pitch && vla[i].pitch > bass[i].pitch);
}

TEST ("orchestrate: a choir sings four real parts, nobody doubling the tune")
{
    auto s = ensemble ({ "sop", "alto", "ten", "bass" });
    insertIntoRange (s, blockChords ({ { 48, 55, 64, 72 } }), idsOf (s), 0, s.barStart (1));
    const auto* a = firstNote (partOf (s, "alto"));
    const auto* t = firstNote (partOf (s, "ten"));
    const auto* top = firstNote (partOf (s, "sop"));
    CHECK (a != nullptr && t != nullptr && top != nullptr);
    if (a == nullptr || t == nullptr || top == nullptr) return;
    // C E G with C on top: the alto and tenor between them have E and G.
    std::vector<int> inner { pcOf (a->pitch), pcOf (t->pitch) };
    std::sort (inner.begin(), inner.end());
    CHECK (inner == (std::vector<int> { 4, 7 }));
}

TEST ("orchestrate: a full orchestra, every part chosen, every part plays, each in its range and one note at a time")
{
    auto& e = engine();
    for (const char* content : { "Both", "Chords" })
        for (int seed = 1; seed <= 3; ++seed)
        {
            auto s = scoreFromTemplate (*templateByName ("Full Orchestra"));
            auto ctx = contextFor (s, s.parts.front().id, 0, {});
            ctx.rangeBars = 4;
            e.reset ("good-idea");
            choose (e, ctx, "kind", "Phrase");
            choose (e, ctx, "content", content);
            const auto out = e.generate ("good-idea", ctx, seed, 1);
            CHECK (! out.results.empty());
            if (out.results.empty()) continue;
            const size_t before = s.parts.size();
            insertIntoRange (s, out.results.front(), idsOf (s), 0, s.barStart (4));
            CHECK_EQ (s.parts.size(), before);                     // nothing left over for new parts
            for (const auto& p : s.parts)
            {
                const auto& inst = instrumentById (p.instrument);
                CHECK (! p.notes.empty());
                if (p.notes.empty()) std::printf ("  (%s %d: %s has nothing)\n", content, seed, p.name.c_str());
                CHECK (polyphonyOf (p.notes) <= std::max (1, inst.poly));
                for (const auto& n : p.notes) CHECK (n.pitch >= inst.low && n.pitch <= inst.high);
            }
            // The double basses an octave under the cellos, where both play.
            const auto& vc = partOf (s, "vc").notes;
            const auto& cb = partOf (s, "cb").notes;
            CHECK_EQ (vc.size(), cb.size());
        }
    e.reset ("good-idea");
}

TEST ("orchestrate: a tune alone is played in octaves by everyone chosen")
{
    GeneratedResult r;
    GeneratedPart m; m.name = "Melody";
    for (int i = 0; i < 4; ++i) { Note n; n.start = i * PPQ; n.length = PPQ; n.pitch = 72 + i; m.notes.push_back (n); }
    r.parts = { m };
    r.length = 4 * PPQ;
    auto s = ensemble ({ "fl", "cl", "vln1", "vc" });
    insertIntoRange (s, r, idsOf (s), 0, s.barStart (1));
    for (const auto& p : s.parts)
    {
        CHECK_EQ (p.notes.size(), size_t (4));
        if (! p.notes.empty()) CHECK_EQ (pcOf (p.notes.front().pitch), 0);
    }
}

TEST ("orchestrate: a tune whose notes overlap by a hair is still the tune, not chords")
{
    // Generate Notes' melodies are legato: each note can run a tick into the next.
    GeneratedResult r = blockChords ({ { 48, 55, 64 } });
    GeneratedPart m; m.name = "Melody";
    for (int i = 0; i < 4; ++i) { Note n; n.start = i * PPQ; n.length = PPQ + 10; n.pitch = 76 + i; m.notes.push_back (n); }
    r.parts.insert (r.parts.begin(), m);
    auto s = ensemble ({ "glock", "vln1", "vln2", "vla", "vc" });
    const auto report = insertIntoRange (s, r, idsOf (s), 0, s.barStart (1));
    CHECK (report.thinnedParts.empty());                     // nobody given chords they cannot play
    const auto& v1 = partOf (s, "vln1").notes;
    CHECK_EQ (v1.size(), size_t (4));                        // the tune, note for note
    for (size_t i = 0; i < v1.size(); ++i) CHECK_EQ (pcOf (v1[i].pitch), pcOf (76 + static_cast<int> (i)));
    CHECK_EQ (partOf (s, "glock").notes.size(), size_t (4));
}

TEST ("orchestrate: one chosen part takes all of the idea, and no part is added")
{
    // A tune over chords, into a violin alone: one line, the top, in its
    // register; nothing put anywhere else.
    GeneratedResult r = blockChords ({ { 48, 55, 64 }, { 53, 57, 65 } });
    GeneratedPart m; m.name = "Melody";
    for (int i = 0; i < 8; ++i) { Note n; n.start = i * PPQ; n.length = PPQ; n.pitch = 72 + (i % 3); m.notes.push_back (n); }
    r.parts.insert (r.parts.begin(), m);
    auto s = ensemble ({ "vln1", "vc" });
    insertIntoRange (s, r, { s.parts[0].id }, 0, s.barStart (2));
    CHECK_EQ (s.parts.size(), size_t (2));
    CHECK (partOf (s, "vc").notes.empty());
    const auto& v = partOf (s, "vln1").notes;
    CHECK_EQ (v.size(), size_t (8));                          // the tune, the top line
    CHECK_EQ (polyphonyOf (v), 1);
    for (const auto& n : v) CHECK (n.pitch >= 72);

    // One bar chosen: one bar of it.
    auto one = ensemble ({ "vln1" });
    insertIntoRange (one, r, { one.parts[0].id }, 0, one.barStart (1));
    for (const auto& n : one.parts[0].notes) CHECK (n.end() <= one.barStart (1));

    // A piano takes all of it.
    auto p = ensemble ({ "pno" });
    insertWhole (p, r, p.parts[0].id, 0, 0);
    CHECK_EQ (p.parts[0].notes.size(), size_t (14));
}

TEST ("orchestrate: drums with no kit chosen are named, not given a part of their own")
{
    GeneratedResult r = blockChords ({ { 48, 55, 64 } });
    GeneratedPart d; d.name = "Drums"; d.drums = true;
    for (int i = 0; i < 4; ++i) { Note n; n.start = i * PPQ; n.length = PPQ / 2; n.pitch = 36; d.notes.push_back (n); }
    r.parts.push_back (d);
    auto s = ensemble ({ "vln1", "vla", "vc" });
    const auto report = insertIntoRange (s, r, idsOf (s), 0, s.barStart (1));
    CHECK_EQ (s.parts.size(), size_t (3));
    CHECK (std::find (report.unplaced.begin(), report.unplaced.end(), std::string ("Drums")) != report.unplaced.end());
    // And into a kit alone, only the drums.
    auto k = ensemble ({ "kit" });
    const auto kr = insertIntoRange (k, r, { k.parts[0].id }, 0, k.barStart (1));
    CHECK_EQ (k.parts[0].notes.size(), size_t (4));
    CHECK (std::find (kr.unplaced.begin(), kr.unplaced.end(), std::string ("Chords")) != kr.unplaced.end());
}

TEST ("orchestrate: a single line is never shared out; a tune and a second voice are split")
{
    GeneratedResult tune;
    GeneratedPart m; m.name = "Melody";
    for (int i = 0; i < 4; ++i) { Note n; n.start = i * PPQ; n.length = PPQ + 5; n.pitch = 72 + i; m.notes.push_back (n); }
    tune.parts = { m };
    tune.length = 4 * PPQ;
    CHECK (isSingleLine (tune));                              // legato, but one line
    auto withChords = blockChords ({ { 48, 55, 64 } });
    CHECK (! isSingleLine (withChords));
    withChords.parts.push_back (m);
    CHECK (! isSingleLine (withChords));

    // A tune and a second voice across a quartet: the top two the tune, the
    // bottom two the second voice.
    GeneratedPart second; second.name = "Second voice";
    for (const auto& n : m.notes) { Note t = n; t.pitch -= 4; second.notes.push_back (t); }
    auto two = tune;
    two.parts.push_back (second);
    CHECK (! isSingleLine (two));
    auto s = ensemble ({ "vln1", "vln2", "vla", "vc" });
    insertIntoRange (s, two, idsOf (s), 0, s.barStart (1));
    CHECK_EQ (pcOf (firstNote (partOf (s, "vln1"))->pitch), 0);
    CHECK_EQ (pcOf (firstNote (partOf (s, "vln2"))->pitch), 0);
    CHECK_EQ (pcOf (firstNote (partOf (s, "vla"))->pitch), 8);
    CHECK_EQ (pcOf (firstNote (partOf (s, "vc"))->pitch), 8);
}
