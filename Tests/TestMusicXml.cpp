#include "Check.h"

#include "MusicXml.h"
#include "Spelling.h"
#include "Xml.h"

#include <cmath>
#include <set>
#include <tuple>

using namespace nt;

namespace
{
void add (Score& s, size_t part, double beat, double beats, int pitch)
{
    Note n;
    n.start = static_cast<Tick> (std::llround (beat * PPQ));
    n.length = static_cast<Tick> (std::llround (beats * PPQ));
    n.pitch = pitch;
    n.id = s.newId();
    s.parts[part].notes.push_back (n);
}

Score sample()
{
    Score s;
    s.title = "Round & trip";
    s.composer = "Noterator";
    s.keys = { { 0, rootIndexByName ("D"), 0 } };
    s.meters = { { 0, 4, 4 }, { 2, 3, 4 } };
    s.tempos = { { 0, 96.0 } };
    for (const char* id : { "vln1", "cl", "pno", "kit" })
    {
        Part p; p.id = s.newId(); p.instrument = id; p.name = instrumentById (id).name;
        s.parts.push_back (p);
    }
    // Violin: eighths, a dotted pair, a triplet and a note tied over the bar.
    double t = 0;
    for (int p : { 74, 76, 78, 79 }) { add (s, 0, t, 0.5, p); t += 0.5; }
    add (s, 0, t, 0.75, 81); t += 0.75; add (s, 0, t, 0.25, 79); t += 0.25;
    for (int p : { 78, 76, 74 }) { add (s, 0, t, 1.0 / 3, p); t += 1.0 / 3; }
    add (s, 0, 7, 2, 73);                     // across into bar 2
    add (s, 0, 9, 3, 74);                     // bar 3, in 3/4
    // Clarinet in Bb: stored sounding, written a tone up.
    add (s, 1, 0, 4, 62); add (s, 1, 4, 4, 64);
    // Piano: right-hand chords over a left-hand bass.
    for (int p : { 66, 69, 74 }) add (s, 2, 0, 2, p);
    add (s, 2, 0, 4, 38);
    // Drums.
    for (int i = 0; i < 4; ++i) add (s, 3, i, 1, i % 2 == 0 ? 36 : 38);
    s.normalise();
    return s;
}

std::multiset<std::tuple<Tick, Tick, int>> notesOf (const Part& p)
{
    std::multiset<std::tuple<Tick, Tick, int>> out;
    for (const auto& n : p.notes) out.insert ({ n.start, n.length, n.pitch });
    return out;
}
} // namespace

TEST ("xml: entities, comments, CDATA and empty elements")
{
    const auto r = xml::parse ("<?xml version=\"1.0\"?>\n<!DOCTYPE a [ <!ENTITY x \"y\"> ]>\n<!-- hi -->"
                               "<a k=\"1 &amp; 2\"><b>x &lt; y &#65;&#x42;</b><c/><!-- no --><d><![CDATA[<raw>]]></d></a>");
    CHECK (r.root != nullptr);
    if (! r.root) return;
    CHECK_EQ (r.root->name, std::string ("a"));
    CHECK_EQ (r.root->attr ("k"), std::string ("1 & 2"));
    CHECK_EQ (r.root->childText ("b"), std::string ("x < y AB"));
    CHECK (r.root->has ("c"));
    CHECK_EQ (r.root->childText ("d"), std::string ("<raw>"));
    CHECK (xml::parse ("<a><b></a>").root == nullptr);
    CHECK (xml::parse ("").root == nullptr);
}

TEST ("musicxml: a score survives export and import")
{
    const auto s = sample();
    const auto text = writeMusicXml (s);
    CHECK (text.find ("<score-partwise version=\"4.0\">") != std::string::npos);
    CHECK (text.find ("<time-modification>") != std::string::npos);
    CHECK (text.find ("<tie type=\"start\"/>") != std::string::npos);
    CHECK (text.find ("<chromatic>-2</chromatic>") != std::string::npos);   // the clarinet
    CHECK (text.find ("<unpitched>") != std::string::npos);
    CHECK (text.find ("<staves>2</staves>") != std::string::npos);

    const auto back = readMusicXml (text);
    CHECK (back.ok);
    if (! back.ok) { std::printf ("  %s\n", back.error.c_str()); return; }
    CHECK_EQ (back.score.title, s.title);
    CHECK_EQ (back.score.composer, s.composer);
    CHECK_EQ (back.score.parts.size(), s.parts.size());
    CHECK (back.score.keys.front() == s.keys.front());
    CHECK_EQ (back.score.meters.size(), size_t (2));
    if (back.score.meters.size() == 2) CHECK (back.score.meters[1] == s.meters[1]);
    CHECK (std::abs (back.score.tempos.front().bpm - 96.0) < 0.5);
    for (size_t i = 0; i < s.parts.size() && i < back.score.parts.size(); ++i)
    {
        CHECK_EQ (back.score.parts[i].instrument, s.parts[i].instrument);
        CHECK (notesOf (back.score.parts[i]) == notesOf (s.parts[i]));
    }
}

TEST ("musicxml: a file from another program - backups, chords, ties, a pickup")
{
    const std::string text = R"(<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE score-partwise PUBLIC "-//Recordare//DTD MusicXML 3.1 Partwise//EN" "http://www.musicxml.org/dtds/partwise.dtd">
<score-partwise version="3.1">
  <movement-title>Test</movement-title>
  <part-list>
    <score-part id="P1"><part-name>Violoncello</part-name>
      <midi-instrument id="P1-I1"><midi-channel>1</midi-channel><midi-program>43</midi-program></midi-instrument>
    </score-part>
  </part-list>
  <part id="P1">
    <measure number="0" implicit="yes">
      <attributes><divisions>2</divisions><key><fifths>-1</fifths><mode>minor</mode></key>
        <time><beats>3</beats><beat-type>4</beat-type></time><clef><sign>F</sign><line>4</line></clef></attributes>
      <note><pitch><step>A</step><octave>2</octave></pitch><duration>2</duration><voice>1</voice><type>quarter</type></note>
    </measure>
    <measure number="1">
      <note><pitch><step>D</step><octave>3</octave></pitch><duration>4</duration><tie type="start"/><voice>1</voice><type>half</type></note>
      <note><chord/><pitch><step>F</step><octave>3</octave></pitch><duration>4</duration><voice>1</voice><type>half</type></note>
      <note><pitch><step>D</step><octave>3</octave></pitch><duration>2</duration><tie type="stop"/><voice>1</voice><type>quarter</type></note>
      <backup><duration>6</duration></backup>
      <note><pitch><step>B</step><alter>-1</alter><octave>1</octave></pitch><duration>6</duration><voice>2</voice><type>half</type><dot/></note>
    </measure>
  </part>
</score-partwise>)";
    const auto r = readMusicXml (text);
    CHECK (r.ok);
    if (! r.ok) { std::printf ("  %s\n", r.error.c_str()); return; }
    CHECK_EQ (r.score.title, std::string ("Test"));
    CHECK_EQ (r.score.parts.size(), size_t (1));
    const auto& p = r.score.parts[0];
    CHECK_EQ (p.instrument, std::string ("vc"));
    CHECK_EQ (r.score.keys.front().root, rootIndexByName ("D"));
    CHECK_EQ (r.score.keys.front().scale, 1);
    CHECK_EQ (r.score.meters.front().num, 3);
    // The pickup moves to the last beat of bar one; the tied D is one note
    // of three beats; the low Bb is the second voice.
    const auto ns = notesOf (p);
    CHECK (ns.count ({ 2 * PPQ, PPQ, 45 }) == 1);
    CHECK (ns.count ({ 3 * PPQ, 3 * PPQ, 50 }) == 1);
    CHECK (ns.count ({ 3 * PPQ, 2 * PPQ, 53 }) == 1);
    CHECK (ns.count ({ 3 * PPQ, 3 * PPQ, 34 }) == 1);
    CHECK_EQ (p.notes.size(), size_t (4));
    for (const auto& n : p.notes) if (n.pitch == 34) CHECK_EQ (n.voice, 1);
}

TEST ("musicxml: rubbish is refused with a reason")
{
    CHECK (! readMusicXml ("not xml at all").ok);
    CHECK (! readMusicXml ("<html><body/></html>").ok);
    CHECK (! readMusicXml ("<score-timewise/>").ok);
    CHECK (! readMusicXml ("<score-partwise/>").ok);
}

TEST ("musicxml: instruments by name")
{
    CHECK_EQ (instrumentForPartName ("Violin II", -1, false, 1), std::string ("vln2"));
    CHECK_EQ (instrumentForPartName ("Viola", -1, false, 1), std::string ("vla"));
    CHECK_EQ (instrumentForPartName ("Clarinet in Bb", -1, false, 1), std::string ("cl"));
    CHECK_EQ (instrumentForPartName ("Bass Clarinet", -1, false, 1), std::string ("bcl"));
    CHECK_EQ (instrumentForPartName ("Drumset", -1, false, 1), std::string ("kit"));
    CHECK_EQ (instrumentForPartName ("Anything", -1, true, 1), std::string ("kit"));
    CHECK_EQ (instrumentForPartName ("Mystery", 74, false, 1), std::string ("fl"));
}
