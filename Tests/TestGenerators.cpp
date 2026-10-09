#include "Check.h"

#include "Generators.h"
#include "LuaEngine.h"

#include <algorithm>
#include <map>

using namespace nt;

namespace
{
LuaEngine& engineInstance()
{
    static LuaEngine e;
    return e;
}

Score withViola()
{
    Score s;
    Part p; p.id = s.newId(); p.name = "Viola"; p.instrument = "vla";
    s.parts = { p };
    return s;
}
} // namespace

TEST ("generators: all five load")
{
    auto& e = engineInstance();
    CHECK_EQ (e.error(), std::string());
    CHECK_EQ (e.generators().size(), size_t (5));
}

TEST ("generators: every one makes music, and every setting can be chosen")
{
    auto& e = engineInstance();
    auto s = withViola();
    // A tune to give the selection-based ones something to read.
    Selection sel;
    const int tune[] = { 60, 60, 67, 67, 69, 69, 67 };
    for (int i = 0; i < 7; ++i) sel.insert (writeNote (s, s.parts[0].id, i * PPQ, i == 6 ? 2 * PPQ : PPQ, tune[i], 0));

    for (const auto& g : e.generators())
    {
        auto ctx = contextFor (s, s.parts[0].id, 0, g.needsSelection ? sel : Selection {});
        const auto settings = e.settings (g.id, ctx);
        CHECK (! settings.empty());
        for (const auto& st : settings)
            for (int i = 0; i < static_cast<int> (st.names.size()); ++i)
            {
                e.set (g.id, st.id, i, ctx);
                CHECK_EQ (e.error(), std::string());
            }
        e.reset (g.id);
        e.useKey (g.id, ctx.root, ctx.scale);
        const auto out = e.generate (g.id, ctx, 1, 3);
        CHECK_EQ (out.error, std::string());
        CHECK (out.ok);
        CHECK (! out.results.empty());
        for (const auto& r : out.results)
        {
            CHECK (! r.parts.empty());
            CHECK (r.length > 0);
        }
    }
}

TEST ("generators: the catalogue writes a viola line in the viola's range")
{
    auto& e = engineInstance();
    auto s = withViola();
    const auto ctx = contextFor (s, s.parts[0].id, 0, {});
    e.reset ("midi-catalogue");
    const auto out = e.generate ("midi-catalogue", ctx, 1, 6);
    CHECK (! out.results.empty());
    const auto& vla = instrumentById ("vla");
    for (const auto& r : out.results)
        for (const auto& p : r.parts)
            for (const auto& n : p.notes)
                CHECK (n.pitch >= vla.low && n.pitch <= vla.high);
}

TEST ("generators: a result goes into the part, fitted to its instrument")
{
    auto s = withViola();
    GeneratedResult r;
    r.length = 4 * PPQ;
    GeneratedPart melody;
    melody.name = "Melody";
    for (int i = 0; i < 4; ++i) { Note n; n.start = i * PPQ; n.length = PPQ; n.pitch = 88 + i; melody.notes.push_back (n); }
    GeneratedPart bass;
    bass.name = "Bass";
    { Note n; n.start = 0; n.length = 4 * PPQ; n.pitch = 36; bass.notes.push_back (n); }
    r.parts = { melody, bass };
    const auto report = insertResult (s, r, s.parts[0].id, 4 * PPQ);
    CHECK_EQ (s.parts.size(), size_t (2));
    CHECK_EQ (report.newNotes.size(), size_t (5));
    const auto& vla = instrumentById ("vla");
    for (const auto& n : s.parts[0].notes)
    {
        CHECK (n.pitch >= vla.sweetLow && n.pitch <= vla.sweetHigh);
        CHECK (n.start >= 4 * PPQ);
    }
    CHECK_EQ (s.parts[1].instrument, std::string ("vc"));   // a bass under strings is a cello
}

TEST ("generators: fitting moves by octaves only")
{
    const auto& vc = instrumentById ("vc");
    std::vector<Note> notes;
    for (int p : { 72, 76, 79 }) { Note n; n.pitch = p; notes.push_back (n); }
    const auto fitted = fitToInstrument (notes, vc);
    for (size_t i = 0; i < notes.size(); ++i)
    {
        CHECK_EQ ((fitted[i].pitch - notes[i].pitch) % 12, 0);
        CHECK (fitted[i].pitch >= vc.sweetLow && fitted[i].pitch <= vc.sweetHigh);
    }
}

TEST ("generators: chords made for a tune go beside it, never over it")
{
    // A string quartet with a tune in the first violin.
    Score s;
    for (const char* id : { "vln1", "vln2", "vla", "vc" })
    {
        Part p; p.id = s.newId(); p.instrument = id; p.name = instrumentById (id).name;
        s.parts.push_back (p);
    }
    writeNote (s, s.parts[0].id, 0, 4 * PPQ, 72, 0);
    GeneratedResult r;
    r.length = 4 * PPQ;
    GeneratedPart chords; chords.name = "Chords";
    for (int p : { 60, 64, 67 }) { Note n; n.start = 0; n.length = 4 * PPQ; n.pitch = p; chords.notes.push_back (n); }
    GeneratedPart bass; bass.name = "Bass";
    { Note n; n.start = 0; n.length = 4 * PPQ; n.pitch = 36; bass.notes.push_back (n); }
    r.parts = { chords, bass };
    InsertOptions o;
    o.contextInstrument = "vln1";
    insertResult (s, r, 0, 0, o);
    CHECK_EQ (s.parts[0].notes.size(), size_t (1));            // the tune is untouched
    CHECK_EQ (s.parts[0].notes[0].pitch, 72);
    CHECK_EQ (s.parts.size(), size_t (5));
    CHECK_EQ (s.parts[4].instrument, std::string ("pno"));    // three notes at once: not a one-line string part
    CHECK_EQ (s.parts[3].notes.size(), size_t (1));            // the bass went to the cello, which was free
    CHECK_EQ (polyphonyOf (chords.notes), 3);
}

TEST ("generators: a result in a span is cut where the span is shorter, played once where it is longer")
{
    GeneratedResult r;
    r.length = 4 * PPQ;
    GeneratedPart p; p.name = "Melody";
    for (int i = 0; i < 4; ++i) { Note n; n.start = i * PPQ; n.length = PPQ; n.pitch = 60 + i; p.notes.push_back (n); }
    r.parts = { p };
    // Twelve bars chosen for eight bars of music: eight filled, four left empty (0042).
    const auto longer = fitToSpan (r, 10 * PPQ);
    CHECK_EQ (longer.length, 10 * PPQ);
    CHECK_EQ (longer.parts[0].notes.size(), size_t (4));
    CHECK_EQ (longer.parts[0].notes.back().start, 3 * PPQ);
    // Five bars chosen for eight: the five, the rest left out.
    const auto shorter = fitToSpan (r, 2 * PPQ + PPQ / 2);
    CHECK_EQ (shorter.parts[0].notes.size(), size_t (3));
    CHECK_EQ (shorter.parts[0].notes.back().length, PPQ / 2);
}

TEST ("generators: chords dealt out one note to a part, top note first")
{
    std::vector<Note> chords;
    for (int p : { 60, 64, 67 }) { Note n; n.start = 0; n.length = PPQ; n.pitch = p; chords.push_back (n); }
    for (int p : { 62, 65 }) { Note n; n.start = PPQ; n.length = PPQ; n.pitch = p; chords.push_back (n); }
    const auto lines = spreadChords (chords, 3);
    CHECK_EQ (lines.size(), size_t (3));
    CHECK_EQ (lines[0][0].pitch, 67);
    CHECK_EQ (lines[1][0].pitch, 64);
    CHECK_EQ (lines[2][0].pitch, 60);
    CHECK_EQ (lines[2][1].pitch, 62);   // two notes for three parts: the lowest doubled
    for (const auto& l : lines) CHECK_EQ (l.size(), size_t (2));
}

TEST ("generators: a measure into four selected bars of a string quartet")
{
    Score s;
    for (const char* id : { "vln1", "vln2", "vla", "vc" })
    {
        Part p; p.id = s.newId(); p.instrument = id; p.name = instrumentById (id).name;
        s.parts.push_back (p);
    }
    writeNote (s, s.parts[1].id, 0, PPQ, 60, 0);            // something outside the span, kept
    writeNote (s, s.parts[0].id, 5 * PPQ, PPQ, 76, 0);      // something inside it, replaced
    GeneratedResult r;
    r.length = 8 * PPQ;
    GeneratedPart melody; melody.name = "Melody";
    GeneratedPart chords; chords.name = "Chords";
    GeneratedPart bass; bass.name = "Bass";
    for (int bar = 0; bar < 2; ++bar)
    {
        { Note n; n.start = bar * 4 * PPQ; n.length = 4 * PPQ; n.pitch = 79; melody.notes.push_back (n); }
        for (int p : { 60, 64, 67 }) { Note n; n.start = bar * 4 * PPQ; n.length = 4 * PPQ; n.pitch = p; chords.notes.push_back (n); }
        { Note n; n.start = bar * 4 * PPQ; n.length = 4 * PPQ; n.pitch = 36; bass.notes.push_back (n); }
    }
    r.parts = { melody, chords, bass };
    std::vector<uint32_t> ids;
    for (const auto& p : s.parts) ids.push_back (p.id);
    insertIntoRange (s, r, ids, 4 * PPQ, 20 * PPQ);         // bars 2 to 5
    CHECK_EQ (s.parts.size(), size_t (4));                   // no new parts: everything fitted
    // The tune once - two bars of four, the last two left empty (0042) - in
    // the first violins, the old note gone.
    CHECK_EQ (s.parts[0].notes.size(), size_t (2));
    for (const auto& n : s.parts[0].notes) CHECK (n.start >= 4 * PPQ && n.end() <= 20 * PPQ);
    // Chords dealt to the second violins and violas, one note each.
    CHECK_EQ (s.parts[1].notes.size(), size_t (3));          // its own note at the start, kept, plus two
    CHECK_EQ (s.parts[2].notes.size(), size_t (2));
    for (size_t i = 1; i < 3; ++i) CHECK_EQ (polyphonyOf (s.parts[i].notes), 1);
    // The bass in the cellos.
    CHECK_EQ (s.parts[3].notes.size(), size_t (2));
    const auto& vc = instrumentById ("vc");
    for (const auto& n : s.parts[3].notes) CHECK (n.pitch >= vc.low && n.pitch <= vc.high);
}

TEST ("generators: Good Idea fits its length to the selected bars")
{
    auto& e = engineInstance();
    Score s;
    Part p; p.id = s.newId(); p.instrument = "fl"; p.name = "Flute";
    s.parts = { p };
    auto ctx = contextFor (s, p.id, 0, {});
    ctx.rangeBars = 3;
    e.reset ("good-idea");
    const auto out = e.generate ("good-idea", ctx, 1, 6);
    CHECK (! out.results.empty());
    for (const auto& r : out.results) CHECK (r.length <= 3 * 4 * PPQ);   // a motif or phrase of at most three bars
    const auto settings = e.settings ("good-idea", ctx);
    for (const auto& st : settings)
        if (st.id == "motifBars") CHECK_EQ (st.names[static_cast<size_t> (st.index)], std::string ("Any"));   // left as it was
}

TEST ("generators: Starting Blocks offers every chord family, one block per degree")
{
    auto& e = engineInstance();
    GeneratorContext ctx;
    e.reset ("starting-blocks");
    e.useKey ("starting-blocks", 0, 0);
    auto find = [&] (const std::string& id) -> const GeneratorSetting*
    {
        static std::vector<GeneratorSetting> list;
        list = e.settings ("starting-blocks", ctx);
        for (const auto& s : list) if (s.id == id) return &s;
        return nullptr;
    };
    const auto* family = find ("family");
    CHECK (family != nullptr);
    if (family == nullptr) return;
    CHECK_EQ (family->names.size(), size_t (8));
    CHECK (find ("dia") != nullptr);
    CHECK (find ("chord") == nullptr);

    // "6ths & 7ths": the chord menu lists that family alone, starting on its first.
    e.set ("starting-blocks", "family", 2, ctx);
    CHECK (find ("dia") == nullptr);
    const auto* chord = find ("chord");
    CHECK (chord != nullptr);
    if (chord == nullptr) return;
    CHECK_EQ (chord->names.size(), size_t (15));
    CHECK_EQ (chord->names[static_cast<size_t> (chord->index)], std::string ("6  Sixth"));
    const int maj7 = 5;
    CHECK_EQ (chord->names[maj7], std::string ("maj7  Major Seventh"));
    e.set ("starting-blocks", "chord", maj7, ctx);

    const auto out = e.generate ("starting-blocks", ctx, 1, 0);
    CHECK_EQ (out.results.size(), size_t (7));
    if (out.results.size() != 7) return;
    // The ii is a D major seventh here: the family's chords are built on the
    // degree, not taken from the key.
    std::vector<int> pitches;
    for (const auto& n : out.results[1].parts[0].notes) pitches.push_back (n.pitch);
    CHECK (pitches == (std::vector<int> { 62, 66, 69, 73 }));
    CHECK_EQ (out.results[1].detail.substr (0, 1), std::string ("D"));
    e.reset ("starting-blocks");
}

TEST ("generators: Blocks offers chords, arpeggios, runs and intervals; drums stay with Good Idea")
{
    auto& e = engineInstance();
    GeneratorContext ctx;
    e.reset ("starting-blocks");
    std::vector<std::string> kinds;
    for (const auto& s : e.settings ("starting-blocks", ctx))
        if (s.id == "cat") kinds = s.names;
    CHECK (kinds == (std::vector<std::string> { "Chord", "Arpeggio", "Run", "Melody" }));

    e.reset ("good-idea");
    std::vector<std::string> makes;
    for (const auto& s : e.settings ("good-idea", ctx))
        if (s.label == "Make a") makes = s.names;
    CHECK (std::find (makes.begin(), makes.end(), "Drums") != makes.end());
}

TEST ("generators: the Generate tab's names say what they do")
{
    auto& e = engineInstance();
    std::map<std::string, std::string> names;
    for (const auto& g : e.generators()) names[g.id] = g.name;
    CHECK_EQ (names["good-idea"], std::string ("Generate Notes"));
    CHECK_EQ (names["midi-suggester"], std::string ("Suggest Notes"));
    CHECK_EQ (names["midi-variator"], std::string ("Vary Notes"));

    // Results are titled by what they are, not by the engine's name and seed.
    GeneratorContext ctx;
    e.reset ("good-idea");
    const auto out = e.generate ("good-idea", ctx, 5, 2);
    CHECK (! out.results.empty());
    for (const auto& r : out.results) CHECK (r.title.find ("Good Idea") == std::string::npos);
}

namespace
{
GeneratedResult chordLine (std::initializer_list<std::vector<int>> chords, Tick each, Tick held)
{
    GeneratedResult r;
    GeneratedPart gp;
    gp.name = "Chords";
    Tick at = 0;
    for (const auto& c : chords)
    {
        for (int p : c) { Note n; n.start = at; n.length = held; n.pitch = p; gp.notes.push_back (n); }
        at += each;
    }
    r.parts = { gp };
    r.length = at;
    return r;
}

Score quartet()
{
    Score s;
    for (const char* id : { "vln1", "vln2", "vla", "vc" })
    {
        Part p; p.id = s.newId(); p.instrument = id; p.name = instrumentById (id).name;
        s.parts.push_back (p);
    }
    s.bars = 8;
    return s;
}
} // namespace

TEST ("generators: a one-note instrument gets one note at a time - the top of a chord, or the bottom for a bass")
{
    const auto r = chordLine ({ { 60, 64, 67 }, { 62, 65, 69 }, { 59, 62, 67 } }, PPQ, PPQ);
    auto s = quartet();
    insertResult (s, r, s.parts[0].id, 0);                       // the caret in the first violin
    const auto& vln = s.parts[0].notes;
    CHECK_EQ (polyphonyOf (vln), 1);
    CHECK_EQ (vln.size(), size_t (3));
    std::vector<int> top;
    for (const auto& n : vln) top.push_back (n.pitch % 12);
    CHECK (top == (std::vector<int> { 7, 9, 7 }));               // G, A, G: the top notes, wherever the octave fitting put them

    auto t = quartet();
    const auto report = insertResult (t, r, t.parts[3].id, 0);   // the cello takes the bass
    CHECK_EQ (polyphonyOf (t.parts[3].notes), 1);
    std::vector<int> low;
    for (const auto& n : t.parts[3].notes) low.push_back (n.pitch % 12);
    CHECK (low == (std::vector<int> { 0, 2, 11 }));              // C, D, B
    CHECK (std::find (report.thinnedParts.begin(), report.thinnedParts.end(), t.parts[3].id) != report.thinnedParts.end());
}

TEST ("generators: chords dealt out across a quartet never overlap in one part, however long they are held")
{
    // Each chord held for two beats, a new one every beat: dealt out one note
    // to a part, each part's notes would run into the next.
    const auto r = chordLine ({ { 55, 60, 64, 67 }, { 57, 60, 65, 69 }, { 55, 59, 62, 67 }, { 48, 60, 64, 67 } }, PPQ, 2 * PPQ);
    auto s = quartet();
    std::vector<uint32_t> ids;
    for (const auto& p : s.parts) ids.push_back (p.id);
    insertIntoRange (s, r, ids, 0, s.barStart (1));
    for (const auto& p : s.parts)
    {
        CHECK_EQ (polyphonyOf (p.notes), 1);
        CHECK (! p.notes.empty());
    }
    // Legato, not cut short: each note runs up to the next.
    const auto& top = s.parts[0].notes;
    CHECK_EQ (top.size(), size_t (4));
    if (top.size() == 4) CHECK_EQ (top[0].end(), top[1].start);
}

TEST ("generators: a piano keeps its chords, a glockenspiel two notes, a drum kit every hit")
{
    const auto r = chordLine ({ { 60, 64, 67, 72 }, { 62, 65, 69, 74 } }, PPQ, PPQ);
    Score s;
    for (const char* id : { "pno", "glock" }) { Part p; p.id = s.newId(); p.instrument = id; s.parts.push_back (p); }
    insertResult (s, r, s.parts[0].id, 0);
    CHECK_EQ (s.parts[0].notes.size(), size_t (8));
    insertResult (s, r, s.parts[1].id, 0);
    CHECK_EQ (polyphonyOf (s.parts[1].notes), 2);
    CHECK_EQ (s.parts[1].notes.size(), size_t (4));

    GeneratedResult groove;
    GeneratedPart d;
    d.name = "Drums";
    d.drums = true;
    for (int p : { 36, 38, 42, 46, 49, 51 }) { Note n; n.start = 0; n.length = PPQ; n.pitch = p; d.notes.push_back (n); }
    groove.parts = { d };
    groove.length = PPQ;
    Score k;
    Part kit; kit.id = k.newId(); kit.instrument = "kit"; k.parts = { kit };
    insertResult (k, groove, k.parts[0].id, 0);
    CHECK_EQ (k.parts[0].notes.size(), size_t (6));              // six at once, more than the kit's four: all kept
}

TEST ("generators: Generate Notes' chord phrases never give a string part two notes at once")
{
    auto& e = engineInstance();
    int results = 0;
    for (const char* content : { "Chords", "Both" })
        for (int seed = 1; seed <= 6; ++seed)
        {
            auto base = quartet();
            auto ctx = contextFor (base, base.parts[0].id, 0, {});
            ctx.rangeBars = 4;
            e.reset ("good-idea");
            for (const auto& st : e.settings ("good-idea", ctx))
                for (size_t i = 0; i < st.names.size(); ++i)
                    if ((st.id == "kind" && st.names[i] == "Phrase") || (st.id == "content" && st.names[i] == content))
                        e.set ("good-idea", st.id, static_cast<int> (i), ctx);
            const auto out = e.generate ("good-idea", ctx, seed, 1);
            for (const auto& r : out.results)
            {
                ++results;
                auto caret = base;
                insertResult (caret, r, caret.parts[0].id, 0);
                auto bars = base;
                std::vector<uint32_t> ids;
                for (const auto& p : bars.parts) ids.push_back (p.id);
                insertIntoRange (bars, r, ids, 0, bars.barStart (4));
                for (const auto* sc : { &caret, &bars })
                    for (const auto& p : sc->parts)
                        CHECK (polyphonyOf (p.notes) <= instrumentById (p.instrument).poly);
            }
        }
    e.reset ("good-idea");
    CHECK_EQ (results, 12);
}

TEST ("generators: an audition is every note on a piano, drums on a kit, cut where it ends")
{
    Score s;
    Part vn;
    vn.id = s.newId();
    vn.instrument = "vln1";
    vn.solo = true;
    s.parts.push_back (vn);
    GeneratedResult r;
    r.length = 8 * PPQ;
    GeneratedPart chords;
    chords.name = "Chords";
    for (int bar = 0; bar < 2; ++bar)
        for (int pitch : { 60, 64, 67, 71 })
            { Note n; n.start = bar * 4 * PPQ; n.length = 4 * PPQ; n.pitch = pitch; chords.notes.push_back (n); }
    GeneratedPart melody;
    melody.name = "Melody";
    for (int i = 0; i < 8; ++i) { Note n; n.start = i * PPQ; n.length = PPQ; n.pitch = 96 + i; melody.notes.push_back (n); }
    GeneratedPart kit;
    kit.name = "Drums";
    kit.drums = true;
    { Note n; n.start = 0; n.length = PPQ / 4; n.pitch = 36; kit.notes.push_back (n); }
    r.parts = { chords, melody, kit };

    addAudition (s, r, 4 * PPQ, 6 * PPQ);   // from bar 2, a bar and a half of it
    CHECK_EQ (s.parts.size(), size_t (3));
    if (s.parts.size() != 3) return;
    CHECK (s.parts[0].notes.empty());       // the violin is left alone
    const auto& piano = s.parts[1];
    CHECK_EQ (piano.instrument, std::string ("pno"));
    CHECK (piano.solo);                      // heard, with the violin soloed
    CHECK_EQ (polyphonyOf (piano.notes), 5); // the chord and the tune, not thinned
    CHECK_EQ (piano.notes.size(), size_t (8 + 6));
    for (const auto& n : piano.notes)
    {
        CHECK (n.start >= 4 * PPQ);
        CHECK (n.end() <= 10 * PPQ);
        CHECK (n.pitch >= 60);               // not moved into any register
    }
    CHECK_EQ (s.parts[2].instrument, std::string ("kit"));
    CHECK_EQ (s.parts[2].notes.size(), size_t (1));
}
