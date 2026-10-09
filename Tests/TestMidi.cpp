#include "Check.h"

#include "MidiFile.h"
#include "Perform.h"
#include "Spelling.h"

using namespace nt;

namespace
{
Score sample()
{
    Score s;
    s.title = "Sample";
    s.meters = { { 0, 3, 4 } };
    s.keys = { { 0, rootIndexByName ("E"), 1 } };
    s.tempos = { { 0, 90.0 } };
    Part a; a.id = s.newId(); a.name = "Flute"; a.instrument = "fl";
    Part d; d.id = s.newId(); d.name = "Drums"; d.instrument = "kit";
    for (int i = 0; i < 6; ++i)
    {
        Note n; n.start = i * PPQ; n.length = PPQ / 2; n.pitch = 64 + i; n.velocity = 80 + i; n.id = s.newId();
        a.notes.push_back (n);
        Note k = n; k.pitch = 36; k.id = s.newId();
        d.notes.push_back (k);
    }
    s.parts = { a, d };
    s.normalise();
    return s;
}
} // namespace

TEST ("midi: notes, tempo, meter and key survive a round trip")
{
    const auto s = sample();
    PerformOptions o;
    o.autoCC = false;
    const auto bytes = writeMidiFile (s, o);
    const auto back = readMidiFile (bytes);
    CHECK (back.ok);
    CHECK_EQ (back.score.parts.size(), size_t (2));
    CHECK_EQ (back.score.meters.front().num, 3);
    CHECK (std::abs (back.score.tempos.front().bpm - 90.0) < 0.01);
    CHECK_EQ (back.score.keys.front().root, rootIndexByName ("E"));
    CHECK_EQ (back.score.keys.front().scale, 1);
    const Part* flute = nullptr;
    const Part* drums = nullptr;
    for (const auto& p : back.score.parts)
    {
        if (p.instrument == "kit") drums = &p;
        else flute = &p;
    }
    CHECK (flute != nullptr && drums != nullptr);
    if (flute != nullptr)
    {
        CHECK_EQ (flute->instrument, std::string ("fl"));
        CHECK_EQ (flute->name, std::string ("Flute"));
        CHECK_EQ (flute->notes.size(), size_t (6));
        CHECK_EQ (flute->notes[3].start, 3 * PPQ);
        CHECK_EQ (flute->notes[3].length, PPQ / 2);
        CHECK_EQ (flute->notes[3].pitch, 67);
        CHECK_EQ (flute->notes[3].velocity, 83);
    }
}

TEST ("midi: drums go on channel 10, pitched parts skip it")
{
    Score s;
    for (int i = 0; i < 12; ++i)
    {
        Part p; p.id = s.newId(); p.instrument = "fl";
        s.parts.push_back (p);
    }
    Part d; d.id = s.newId(); d.instrument = "kit";
    s.parts.push_back (d);
    const auto ch = channelsForParts (s);
    for (size_t i = 0; i < 12; ++i) CHECK (ch[i] != 9);
    CHECK_EQ (ch[9], 10);
    CHECK_EQ (ch[12], 9);
}

TEST ("midi: exporting a range of bars starts it at zero")
{
    const auto s = sample();
    PerformOptions o;
    o.from = s.barStart (1);
    o.to = s.barStart (2);
    o.autoCC = false;
    const auto back = readMidiFile (writeMidiFile (s, o));
    CHECK (back.ok);
    for (const auto& p : back.score.parts)
    {
        CHECK_EQ (p.notes.size(), size_t (3));
        if (! p.notes.empty()) CHECK_EQ (p.notes.front().start, Tick (0));
    }
}

TEST ("perform: mute and solo")
{
    auto s = sample();
    s.parts[1].mute = true;
    for (const auto& e : perform (s)) CHECK (e.part != 1);
    s.parts[1].mute = false;
    s.parts[1].solo = true;
    for (const auto& e : perform (s)) CHECK (e.part == 1);
}

TEST ("perform: AutoCC rides a string part and not a piano")
{
    Score s;
    Part v; v.id = s.newId(); v.instrument = "vla";
    Part p; p.id = s.newId(); p.instrument = "pno";
    Note n; n.start = 0; n.length = 4 * PPQ; n.pitch = 60; n.id = s.newId();
    v.notes = { n };
    n.id = s.newId();
    p.notes = { n };
    s.parts = { v, p };
    s.normalise();
    PerformOptions o;
    o.includeSetup = false;
    int violaCC = 0, pianoCC = 0;
    for (const auto& e : perform (s, o))
        if (e.type == PlayEvent::controller) (e.part == 0 ? violaCC : pianoCC)++;
    CHECK (violaCC > 10);
    CHECK_EQ (pianoCC, 0);
}

TEST ("midi: rubbish is refused, not crashed on")
{
    CHECK (! readMidiFile ({}).ok);
    CHECK (! readMidiFile ({ 'M', 'T', 'h', 'd', 0, 0 }).ok);
    std::vector<uint8_t> truncated = writeMidiFile (sample());
    truncated.resize (truncated.size() / 2);
    readMidiFile (truncated);   // whatever it says, it must not crash
}
