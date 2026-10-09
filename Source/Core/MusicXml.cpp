#include "MusicXml.h"

#include "Engrave.h"
#include "Instruments.h"
#include "MidiFile.h"
#include "Perform.h"
#include "ScaleModel.h"
#include "Spelling.h"
#include "Xml.h"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <map>
#include <set>

namespace nt
{

namespace
{
constexpr int letterPc[7] = { 0, 2, 4, 5, 7, 9, 11 };
const char* const letterNames[7] = { "C", "D", "E", "F", "G", "A", "B" };

std::string typeName (int den)
{
    switch (den)
    {
        case 1: return "whole";
        case 2: return "half";
        case 4: return "quarter";
        case 8: return "eighth";
        case 16: return "16th";
        case 32: return "32nd";
        case 64: return "64th";
        default: return "128th";
    }
}

std::string accidentalName (int acc)
{
    switch (acc)
    {
        case -2: return "flat-flat";
        case -1: return "flat";
        case 1: return "sharp";
        case 2: return "double-sharp";
        default: return "natural";
    }
}

// Semitones up to the number of letter steps: what <diatonic> wants.
int diatonicFor (int semitones)
{
    static constexpr int steps[12] = { 0, 1, 1, 2, 2, 3, 3, 4, 5, 5, 6, 6 };
    const int s = ((semitones % 12) + 12) % 12;
    return steps[s] + 7 * static_cast<int> (std::floor (semitones / 12.0));
}

void clefOf (Clef c, std::string& sign, int& line, int& octave)
{
    octave = 0;
    switch (c)
    {
        case Clef::treble:     sign = "G"; line = 2; break;
        case Clef::treble8vb:  sign = "G"; line = 2; octave = -1; break;
        case Clef::bass:       sign = "F"; line = 4; break;
        case Clef::alto:       sign = "C"; line = 3; break;
        case Clef::tenor:      sign = "C"; line = 4; break;
        case Clef::percussion: sign = "percussion"; line = 0; break;
    }
}

std::string drumName (int pitch)
{
    switch (pitch)
    {
        case 35: case 36: return "Bass Drum";
        case 37: return "Side Stick";
        case 38: case 40: return "Snare";
        case 39: return "Hand Clap";
        case 41: case 43: return "Floor Tom";
        case 45: case 47: return "Tom";
        case 48: case 50: return "High Tom";
        case 42: return "Closed Hi-Hat";
        case 44: return "Pedal Hi-Hat";
        case 46: return "Open Hi-Hat";
        case 49: case 57: return "Crash Cymbal";
        case 51: case 59: return "Ride Cymbal";
        case 53: return "Ride Bell";
        default: return "Percussion " + std::to_string (pitch);
    }
}

std::string modeName (int scale)
{
    const std::string n = scaleview::scales[static_cast<size_t> (scale)].name;
    if (n.find ("Minor") != std::string::npos || n == "Aeolian") return "minor";
    if (n == "Dorian") return "dorian";
    if (n == "Phrygian") return "phrygian";
    if (n == "Lydian") return "lydian";
    if (n == "Mixolydian") return "mixolydian";
    return "major";
}
} // namespace

//==============================================================================
// Writing

std::string writeMusicXml (const Score& score)
{
    engrave::Options lo;
    lo.transposedScore = true;   // MusicXML holds written pitch
    const auto lay = engrave::layout (score, lo);
    const auto channels = channelsForParts (score);

    xml::Writer w ("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n"
                   "<!DOCTYPE score-partwise PUBLIC \"-//Recordare//DTD MusicXML 4.0 Partwise//EN\" "
                   "\"http://www.musicxml.org/dtds/partwise.dtd\">\n");
    w.open ("score-partwise", { { "version", "4.0" } });
    w.open ("work");
    w.leaf ("work-title", score.title);
    w.close();
    w.open ("identification");
    if (! score.composer.empty()) w.leaf ("creator", score.composer, { { "type", "composer" } });
    w.open ("encoding");
    w.leaf ("software", "Noterator");
    {
        char date[16];
        const std::time_t now = std::time (nullptr);
        std::strftime (date, sizeof (date), "%Y-%m-%d", std::gmtime (&now));
        w.leaf ("encoding-date", date);
    }
    w.close();
    w.close();

    // Which drum pitches each kit part uses, for its instrument list.
    std::vector<std::set<int>> drumsUsed (score.parts.size());
    for (size_t pi = 0; pi < score.parts.size(); ++pi)
        if (instrumentById (score.parts[pi].instrument).drums)
            for (const auto& n : score.parts[pi].notes) drumsUsed[pi].insert (n.pitch);

    w.open ("part-list");
    for (size_t pi = 0; pi < score.parts.size(); ++pi)
    {
        const auto& part = score.parts[pi];
        const auto& inst = instrumentById (part.instrument);
        const std::string id = "P" + std::to_string (pi + 1);
        w.open ("score-part", { { "id", id } });
        w.leaf ("part-name", part.name);
        w.leaf ("part-abbreviation", inst.shortName);
        if (inst.drums)
        {
            for (int p : drumsUsed[pi])
            {
                w.open ("score-instrument", { { "id", id + "-I" + std::to_string (p) } });
                w.leaf ("instrument-name", drumName (p));
                w.close();
            }
            for (int p : drumsUsed[pi])
            {
                w.open ("midi-instrument", { { "id", id + "-I" + std::to_string (p) } });
                w.leaf ("midi-channel", 10);
                w.leaf ("midi-unpitched", p + 1);
                w.close();
            }
        }
        else
        {
            w.open ("score-instrument", { { "id", id + "-I1" } });
            w.leaf ("instrument-name", inst.name);
            w.close();
            w.open ("midi-instrument", { { "id", id + "-I1" } });
            w.leaf ("midi-channel", channels[pi] % channelsPerBank + 1);   // a file has sixteen
            w.leaf ("midi-program", inst.program + 1);
            w.leaf ("volume", static_cast<long long> (std::lround (part.volume * 100.0f)));
            w.close();
        }
        w.close();
    }
    w.close();

    for (size_t pi = 0; pi < score.parts.size(); ++pi)
    {
        const auto& part = score.parts[pi];
        const auto& inst = instrumentById (part.instrument);
        const std::string id = "P" + std::to_string (pi + 1);
        std::vector<const engrave::Staff*> staves;
        for (const auto& st : lay.staves)
            if (st.part == static_cast<int> (pi)) staves.push_back (&st);

        // Each staff's elements by measure, and each element's beam and tuplet role.
        struct Role { std::vector<std::string> beams; std::string tuplet; bool tupletBracket = false; };
        std::vector<std::map<int, Role>> roles (staves.size());
        for (size_t si = 0; si < staves.size(); ++si)
        {
            const auto& st = *staves[si];
            for (const auto& b : st.beams)
                for (size_t k = 0; k < b.elements.size(); ++k)
                {
                    const int ei = b.elements[k];
                    auto& r = roles[si][ei];
                    const int count = st.elements[static_cast<size_t> (ei)].beamCount;
                    for (int level = 1; level <= count; ++level)
                    {
                        auto has = [&] (size_t idx) { return idx < b.elements.size() && st.elements[static_cast<size_t> (b.elements[idx])].beamCount >= level; };
                        const bool prev = k > 0 && has (k - 1), next = has (k + 1);
                        std::string what;
                        if (level == 1) what = k == 0 ? "begin" : (k + 1 == b.elements.size() ? "end" : "continue");
                        else if (prev && next) what = "continue";
                        else if (prev) what = "end";
                        else if (next) what = "begin";
                        else what = k == 0 ? "forward hook" : "backward hook";
                        r.beams.push_back (what);
                    }
                }
            for (const auto& t : st.tuplets)
            {
                if (t.elements.empty()) continue;
                roles[si][t.elements.front()].tuplet = "start";
                roles[si][t.elements.front()].tupletBracket = t.bracket;
                if (t.elements.size() > 1) roles[si][t.elements.back()].tuplet = "stop";
                else roles[si][t.elements.front()].tuplet = "start-stop";
            }
        }

        w.open ("part", { { "id", id } });
        for (const auto& m : lay.measures)
        {
            w.open ("measure", { { "number", std::to_string (m.bar + 1) } });
            const auto& key = score.keyAtBar (m.bar);
            if (m.bar == 0 || m.showKey || m.showTime)
            {
                w.open ("attributes");
                if (m.bar == 0) w.leaf ("divisions", static_cast<long long> (PPQ));
                if (m.bar == 0 || m.showKey)
                {
                    w.open ("key");
                    w.leaf ("fifths", m.key.fifths());
                    w.leaf ("mode", modeName (key.scale));
                    w.close();
                }
                if (m.bar == 0 || m.showTime)
                {
                    w.open ("time");
                    w.leaf ("beats", m.meter.num);
                    w.leaf ("beat-type", m.meter.den);
                    w.close();
                }
                if (m.bar == 0)
                {
                    if (staves.size() > 1) w.leaf ("staves", static_cast<long long> (staves.size()));
                    for (size_t si = 0; si < staves.size(); ++si)
                    {
                        std::string sign;
                        int line = 0, oct = 0;
                        clefOf (staves[si]->clef, sign, line, oct);
                        w.open ("clef", staves.size() > 1 ? xml::Writer::Attributes { { "number", std::to_string (si + 1) } } : xml::Writer::Attributes {});
                        w.leaf ("sign", sign);
                        if (line > 0) w.leaf ("line", line);
                        if (oct != 0) w.leaf ("clef-octave-change", oct);
                        w.close();
                    }
                    // Written above sounding by `octave + transposition`.
                    if (inst.octave != 0 || inst.transposition != 0)
                    {
                        w.open ("transpose");
                        w.leaf ("diatonic", -diatonicFor (inst.transposition));
                        w.leaf ("chromatic", -inst.transposition);
                        if (inst.octave != 0) w.leaf ("octave-change", -inst.octave / 12);
                        w.close();
                    }
                }
                w.close();
            }
            // Tempo, from the first part only, where each change falls.
            if (pi == 0)
                for (const auto& tp : score.tempos)
                    if (tp.at >= m.start && tp.at < m.start + m.ticks)
                    {
                        w.open ("direction", { { "placement", "above" } });
                        w.open ("direction-type");
                        w.open ("metronome");
                        w.leaf ("beat-unit", "quarter");
                        w.leaf ("per-minute", static_cast<long long> (std::lround (tp.bpm)));
                        w.close();
                        w.close();
                        if (tp.at > m.start) w.leaf ("offset", static_cast<long long> (tp.at - m.start));
                        w.empty ("sound", { { "tempo", std::to_string (static_cast<long long> (std::lround (tp.bpm))) } });
                        w.close();
                    }

            bool first = true;
            for (size_t si = 0; si < staves.size(); ++si)
            {
                const auto& st = *staves[si];
                for (int voice = 0; voice < 2; ++voice)
                {
                    std::vector<int> mine;
                    for (size_t ei = 0; ei < st.elements.size(); ++ei)
                        if (st.elements[ei].measure == m.bar && st.elements[ei].voice == voice) mine.push_back (static_cast<int> (ei));
                    if (mine.empty()) continue;
                    if (! first)
                    {
                        w.open ("backup");
                        w.leaf ("duration", static_cast<long long> (m.ticks));
                        w.close();
                    }
                    first = false;
                    // Voices are numbered per staff the way most programs do: 1 and 2, then 5 and 6.
                    const std::string voiceNumber = std::to_string (static_cast<int> (si) * 4 + voice + 1);
                    for (int ei : mine)
                    {
                        const auto& el = st.elements[static_cast<size_t> (ei)];
                        const auto roleIt = roles[si].find (ei);
                        const Role* role = roleIt != roles[si].end() ? &roleIt->second : nullptr;
                        const size_t count = el.rest ? 1 : std::max<size_t> (1, el.heads.size());
                        for (size_t hi = 0; hi < count; ++hi)
                        {
                            w.open ("note");
                            if (hi > 0) w.empty ("chord");
                            if (el.rest)
                            {
                                if (el.measureRest) w.empty ("rest", { { "measure", "yes" } });
                                else w.empty ("rest");
                            }
                            else
                            {
                                const auto& h = el.heads[hi];
                                if (inst.drums)
                                {
                                    const int step = h.pos + clefBottomStep (Clef::treble);
                                    w.open ("unpitched");
                                    w.leaf ("display-step", letterNames[((step % 7) + 7) % 7]);
                                    w.leaf ("display-octave", static_cast<long long> (std::floor (step / 7.0)));
                                    w.close();
                                }
                                else
                                {
                                    const int step = h.pos + clefBottomStep (st.clef);
                                    w.open ("pitch");
                                    w.leaf ("step", letterNames[((step % 7) + 7) % 7]);
                                    if (h.accidental != 0) w.leaf ("alter", h.accidental);
                                    w.leaf ("octave", static_cast<long long> (std::floor (step / 7.0)));
                                    w.close();
                                }
                            }
                            w.leaf ("duration", static_cast<long long> (el.ticks));
                            if (! el.rest)
                            {
                                if (el.tieIn) w.empty ("tie", { { "type", "stop" } });
                                if (el.tieOut) w.empty ("tie", { { "type", "start" } });
                                if (inst.drums) w.empty ("instrument", { { "id", id + "-I" + std::to_string (el.heads[hi].pitch) } });
                            }
                            w.leaf ("voice", voiceNumber);
                            if (! el.measureRest)
                            {
                                w.leaf ("type", typeName (el.den));
                                for (int d = 0; d < el.dots; ++d) w.empty ("dot");
                            }
                            if (! el.rest && el.heads[hi].showAccidental && ! inst.drums)
                                w.leaf ("accidental", accidentalName (el.heads[hi].accidental));
                            if (el.tuplet != 0)
                            {
                                w.open ("time-modification");
                                w.leaf ("actual-notes", 3);
                                w.leaf ("normal-notes", 2);
                                w.close();
                            }
                            if (! el.rest && el.stem != engrave::Stem::none)
                                w.leaf ("stem", el.stem == engrave::Stem::up ? "up" : "down");
                            if (! el.rest && el.heads[hi].drumCross) w.leaf ("notehead", "x");
                            if (staves.size() > 1) w.leaf ("staff", static_cast<long long> (si + 1));
                            if (hi == 0 && role != nullptr)
                                for (size_t b = 0; b < role->beams.size(); ++b)
                                    w.leaf ("beam", role->beams[b], { { "number", std::to_string (b + 1) } });
                            const bool tied = ! el.rest && (el.tieIn || el.tieOut);
                            const bool tuplet = hi == 0 && role != nullptr && ! role->tuplet.empty();
                            if (tied || tuplet)
                            {
                                w.open ("notations");
                                if (! el.rest && el.tieIn) w.empty ("tied", { { "type", "stop" } });
                                if (! el.rest && el.tieOut) w.empty ("tied", { { "type", "start" } });
                                if (tuplet)
                                {
                                    const std::string bracket = role->tupletBracket ? "yes" : "no";
                                    if (role->tuplet == "start" || role->tuplet == "start-stop")
                                        w.empty ("tuplet", { { "type", "start" }, { "bracket", bracket } });
                                    if (role->tuplet == "stop" || role->tuplet == "start-stop")
                                        w.empty ("tuplet", { { "type", "stop" } });
                                }
                                w.close();
                            }
                            w.close();
                        }
                    }
                }
            }
            if (m.bar + 1 == static_cast<int> (lay.measures.size()))
            {
                w.open ("barline", { { "location", "right" } });
                w.leaf ("bar-style", "light-heavy");
                w.close();
            }
            w.close();
        }
        w.close();
    }
    w.close();
    return w.str();
}

//==============================================================================
// Reading

std::string instrumentForPartName (const std::string& name, int program, bool percussion, int staves)
{
    if (percussion) return "kit";
    std::string n = name;
    std::transform (n.begin(), n.end(), n.begin(), [] (unsigned char c) { return static_cast<char> (std::tolower (c)); });
    // Longest and most particular first: "bass clarinet" before "clarinet",
    // "violin ii" before "violin", "violoncello" before "viola".
    static const std::vector<std::pair<const char*, const char*>> names {
        { "violoncello", "vc" }, { "cello", "vc" }, { "contrabassoon", "cbsn" }, { "contrabass", "cb" },
        { "double bass", "cb" }, { "string bass", "cb" }, { "violin ii", "vln2" }, { "violin 2", "vln2" },
        { "violins ii", "vln2" }, { "violin", "vln1" }, { "viola", "vla" }, { "piccolo", "picc" },
        { "flute", "fl" }, { "english horn", "eh" }, { "cor anglais", "eh" }, { "oboe", "ob" },
        { "bass clarinet", "bcl" }, { "clarinet", "cl" }, { "bassoon", "bsn" }, { "alto sax", "asax" },
        { "tenor sax", "tsax" }, { "baritone sax", "bsax" }, { "bari sax", "bsax" }, { "vibraphone", "vib" },
        { "upright bass", "ubass" }, { "acoustic bass", "ubass" }, { "french horn", "hn" }, { "horn", "hn" }, { "trumpet", "tpt" },
        { "bass trombone", "btbn" }, { "trombone", "tbn" }, { "tuba", "tuba" }, { "timpani", "timp" },
        { "glockenspiel", "glock" }, { "xylophone", "xyl" }, { "marimba", "mar" }, { "harp", "harp" },
        { "celesta", "cel" }, { "piano", "pno" }, { "keyboard", "pno" }, { "drum", "kit" },
        { "percussion", "kit" }, { "soprano", "sop" }, { "alto", "alto" }, { "tenor", "ten" },
        { "bass guitar", "ebass" }, { "electric bass", "ebass" }, { "guitar", "gtr" }, { "baritone", "bass" },
        { "bass", "bass" },
    };
    for (const auto& [key, id] : names)
        if (n.find (key) != std::string::npos) return id;
    if (program > 0) return instrumentForProgram (program - 1, staves > 1 ? "pno" : "pno");
    return "pno";
}

namespace
{
int rootForFifths (int fifths, bool minor)
{
    static const char* majors[15] = { "Cb", "Gb", "Db", "Ab", "Eb", "Bb", "F", "C", "G", "D", "A", "E", "B", "F#", "C#" };
    static const char* minors[15] = { "Ab", "Eb", "Bb", "F", "C", "G", "D", "A", "E", "B", "F#", "C#", "G#", "D#", "A#" };
    const int i = std::clamp (fifths, -7, 7) + 7;
    const int idx = rootIndexByName ((minor ? minors : majors)[i]);
    return idx < 0 ? 0 : idx;
}

int scaleForMode (const std::string& mode)
{
    if (mode == "minor") return 1;
    if (mode == "dorian") return scaleIndexByName ("Dorian");
    if (mode == "phrygian") return scaleIndexByName ("Phrygian");
    if (mode == "lydian") return scaleIndexByName ("Lydian");
    if (mode == "mixolydian") return scaleIndexByName ("Mixolydian");
    if (mode == "aeolian") return scaleIndexByName ("Aeolian");
    return 0;
}

// A mode's tonic from a signature: dorian on D for no sharps or flats, and so on.
int rootForMode (int fifths, const std::string& mode)
{
    static const std::map<std::string, int> fromMajor { { "dorian", 2 }, { "phrygian", 4 }, { "lydian", 5 }, { "mixolydian", 7 }, { "aeolian", 9 } };
    auto it = fromMajor.find (mode);
    if (it == fromMajor.end()) return rootForFifths (fifths, mode == "minor");
    const int majorRoot = scaleview::roots[static_cast<size_t> (rootForFifths (fifths, false))].pitchClass();
    const int pc = (majorRoot + it->second) % 12;
    // The spelling the signature leans to.
    static const char* sharpNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    static const char* flatNames[12] = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
    const int idx = rootIndexByName ((fifths < 0 ? flatNames : sharpNames)[pc]);
    return idx < 0 ? 0 : idx;
}

struct PartInfo
{
    std::string name;
    int program = -1;
    std::map<std::string, int> unpitched;   // score-instrument id -> MIDI pitch
};
} // namespace

MusicXmlResult readMusicXml (const std::string& text)
{
    MusicXmlResult res;
    auto parsed = xml::parse (text);
    if (! parsed.root) { res.error = parsed.error; return res; }
    const auto& root = *parsed.root;
    if (root.name == "score-timewise")
    {
        res.error = "This is a timewise MusicXML file, which is not supported. Most programs can export it partwise.";
        return res;
    }
    if (root.name != "score-partwise")
    {
        res.error = "This is not a MusicXML score.";
        return res;
    }

    Score& s = res.score;
    s.bars = 1;
    s.parts.clear();
    s.meters.clear();
    s.keys.clear();
    s.tempos.clear();
    if (const auto* work = root.child ("work")) s.title = work->childText ("work-title", s.title);
    if (root.has ("movement-title") && (s.title.empty() || s.title == "Untitled")) s.title = root.childText ("movement-title");
    if (const auto* ident = root.child ("identification"))
        for (const auto* c : ident->all ("creator"))
            if (c->attr ("type") == "composer") s.composer = c->text;

    std::map<std::string, PartInfo> infos;
    if (const auto* list = root.child ("part-list"))
        for (const auto* sp : list->all ("score-part"))
        {
            PartInfo info;
            info.name = sp->childText ("part-name");
            std::map<std::string, std::string> instrumentNames;
            for (const auto* si : sp->all ("score-instrument")) instrumentNames[si->attr ("id")] = si->childText ("instrument-name");
            for (const auto* mi : sp->all ("midi-instrument"))
            {
                if (mi->has ("midi-program") && info.program < 0) info.program = static_cast<int> (mi->childNumber ("midi-program", -1));
                if (mi->has ("midi-unpitched")) info.unpitched[mi->attr ("id")] = static_cast<int> (mi->childNumber ("midi-unpitched", 39)) - 1;
            }
            if (info.name.empty() && ! instrumentNames.empty()) info.name = instrumentNames.begin()->second;
            infos[sp->attr ("id")] = info;
        }

    bool firstPart = true;
    for (const auto* partNode : root.all ("part"))
    {
        const auto info = infos[partNode->attr ("id")];
        Part part;
        part.id = s.newId();
        part.name = info.name.empty() ? "Part " + std::to_string (s.parts.size() + 1) : info.name;
        bool percussion = ! info.unpitched.empty();
        int staves = 1;

        Tick divisions = 1;
        int num = 4, den = 4;
        int chromatic = 0, octaveChange = 0;
        Tick measureStart = 0;
        std::map<std::string, int> voiceSlots;   // "staff/voice" -> 0 or 1
        int bar = 0;
        // Notes whose tie into the next one is still open, by sounding pitch.
        std::map<int, size_t> openTies;

        for (const auto* measure : partNode->all ("measure"))
        {
            Tick pos = 0, maxPos = 0, lastStart = 0;
            const size_t firstNote = part.notes.size();
            for (const auto& c : measure->children)
            {
                if (c.name == "attributes")
                {
                    if (c.has ("divisions")) divisions = std::max<Tick> (1, static_cast<Tick> (c.childNumber ("divisions", 1)));
                    if (const auto* k = c.child ("key"))
                        if (firstPart && k->has ("fifths"))
                        {
                            const int fifths = static_cast<int> (k->childNumber ("fifths", 0));
                            const auto mode = k->childText ("mode", "major");
                            KeySig ks { bar, rootForMode (fifths, mode), std::max (0, scaleForMode (mode)) };
                            if (s.keys.empty() || ! (s.keys.back().root == ks.root && s.keys.back().scale == ks.scale)) s.keys.push_back (ks);
                        }
                    if (const auto* t = c.child ("time"))
                    {
                        // A compound signature like 3+2/8 adds its beats up.
                        int beats = 0;
                        const auto bt = t->childText ("beats", "4");
                        for (size_t i = 0; i < bt.size();)
                        {
                            size_t j = i;
                            while (j < bt.size() && std::isdigit (static_cast<unsigned char> (bt[j]))) ++j;
                            if (j > i) beats += std::stoi (bt.substr (i, j - i));
                            i = j + 1;
                        }
                        num = std::max (1, beats);
                        den = std::max (1, static_cast<int> (t->childNumber ("beat-type", 4)));
                        if (firstPart)
                        {
                            Meter mt { bar, num, den };
                            if (s.meters.empty() || ! (s.meters.back().num == num && s.meters.back().den == den)) s.meters.push_back (mt);
                        }
                    }
                    if (c.has ("staves")) staves = static_cast<int> (c.childNumber ("staves", 1));
                    for (const auto* cl : c.all ("clef"))
                        if (cl->childText ("sign") == "percussion") percussion = true;
                    if (const auto* tr = c.child ("transpose"))
                    {
                        chromatic = static_cast<int> (tr->childNumber ("chromatic", 0));
                        octaveChange = static_cast<int> (tr->childNumber ("octave-change", 0));
                    }
                }
                else if (c.name == "direction" || c.name == "sound")
                {
                    const auto* sound = c.name == "sound" ? &c : c.child ("sound");
                    if (firstPart && sound != nullptr && ! sound->attr ("tempo").empty())
                    {
                        const Tick offset = c.has ("offset") ? static_cast<Tick> (c.childNumber ("offset", 0)) * PPQ / divisions : 0;
                        s.tempos.push_back ({ measureStart + pos + offset, std::atof (sound->attr ("tempo").c_str()) });
                    }
                }
                else if (c.name == "backup") pos -= static_cast<Tick> (c.childNumber ("duration", 0)) * PPQ / divisions;
                else if (c.name == "forward") { pos += static_cast<Tick> (c.childNumber ("duration", 0)) * PPQ / divisions; maxPos = std::max (maxPos, pos); }
                else if (c.name == "note")
                {
                    if (c.has ("grace") || c.has ("cue")) continue;
                    const Tick dur = static_cast<Tick> (std::llround (c.childNumber ("duration", 0) * static_cast<double> (PPQ) / static_cast<double> (divisions)));
                    const bool chord = c.has ("chord");
                    const Tick start = measureStart + (chord ? lastStart : pos);
                    if (! chord) { lastStart = pos; pos += dur; maxPos = std::max (maxPos, pos); }
                    if (c.has ("rest") || dur <= 0) continue;

                    int pitch = -1;
                    if (const auto* p = c.child ("pitch"))
                    {
                        const auto step = p->childText ("step", "C");
                        int letter = 0;
                        for (int i = 0; i < 7; ++i) if (step == letterNames[i]) letter = i;
                        const int alter = static_cast<int> (std::lround (p->childNumber ("alter", 0)));
                        const int octave = static_cast<int> (p->childNumber ("octave", 4));
                        pitch = (octave + 1) * 12 + letterPc[letter] + alter + chromatic + 12 * octaveChange;
                    }
                    else if (const auto* u = c.child ("unpitched"))
                    {
                        percussion = true;
                        pitch = 38;
                        if (const auto* ins = c.child ("instrument"))
                        {
                            auto it = info.unpitched.find (ins->attr ("id"));
                            if (it != info.unpitched.end()) pitch = it->second;
                        }
                        else
                        {
                            // No instrument: the line or space says which drum.
                            const auto step = u->childText ("display-step", "C");
                            int letter = 0;
                            for (int i = 0; i < 7; ++i) if (step == letterNames[i]) letter = i;
                            const int pos7 = static_cast<int> (u->childNumber ("display-octave", 4)) * 7 + letter - clefBottomStep (Clef::treble);
                            pitch = pos7 <= 1 ? 36 : pos7 >= 9 ? 42 : 38;
                        }
                    }
                    if (pitch < 0 || pitch > 127) continue;

                    bool tieStop = false, tieStart = false;
                    for (const auto* t : c.all ("tie"))
                    {
                        if (t->attr ("type") == "stop") tieStop = true;
                        if (t->attr ("type") == "start") tieStart = true;
                    }
                    // A tie carries the note on: lengthen it, do not start another.
                    if (tieStop)
                    {
                        auto it = openTies.find (pitch);
                        if (it != openTies.end() && it->second < part.notes.size())
                        {
                            auto& prev = part.notes[it->second];
                            if (std::abs (prev.end() - start) <= PPQ / 32)
                            {
                                prev.length = start + dur - prev.start;
                                if (! tieStart) openTies.erase (it);
                                continue;
                            }
                        }
                    }
                    Note n;
                    n.start = start;
                    n.length = std::max<Tick> (1, dur);
                    n.pitch = pitch;
                    const auto dyn = c.attr ("dynamics");
                    n.velocity = dyn.empty() ? 100 : std::clamp (static_cast<int> (std::lround (std::atof (dyn.c_str()) * 0.9)), 1, 127);
                    const auto slot = c.childText ("staff", "1") + "/" + c.childText ("voice", "1");
                    if (voiceSlots.find (slot) == voiceSlots.end())
                    {
                        int onStaff = 0;
                        for (const auto& [k, v] : voiceSlots) if (k.substr (0, k.find ('/')) == c.childText ("staff", "1")) ++onStaff;
                        voiceSlots[slot] = std::min (onStaff, 1);
                    }
                    n.voice = voiceSlots[slot];
                    n.id = s.newId();
                    part.notes.push_back (n);
                    if (tieStart) openTies[pitch] = part.notes.size() - 1;
                }
            }

            const Tick barLength = static_cast<Tick> (num) * PPQ * 4 / den;
            // A pickup bar is the end of a bar: its notes move to the end of
            // the first bar, so the downbeat lands on bar two.
            if (bar == 0 && measure->attr ("implicit") == "yes" && maxPos > 0 && maxPos < barLength)
            {
                const Tick shift = barLength - maxPos;
                for (size_t i = firstNote; i < part.notes.size(); ++i) part.notes[i].start += shift;
            }
            measureStart += barLength;
            ++bar;
        }

        part.instrument = instrumentForPartName (part.name, info.program, percussion, staves);
        s.parts.push_back (std::move (part));
        s.bars = std::max (s.bars, bar);
        firstPart = false;
    }

    if (s.parts.empty()) { res.error = "This MusicXML file has no parts in it."; return res; }
    const int bars = s.bars;
    s.normalise();
    s.bars = std::max (s.bars, bars);
    res.ok = true;
    return res;
}

} // namespace nt
