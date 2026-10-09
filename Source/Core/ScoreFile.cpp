#include "ScoreFile.h"

#include "Instruments.h"
#include "ScaleModel.h"
#include "Spelling.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <sstream>

namespace nt
{

namespace
{
std::string quote (const std::string& s)
{
    std::string out = "\"";
    for (unsigned char c : s)
    {
        switch (c)
        {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20)
                {
                    char buf[8];
                    std::snprintf (buf, sizeof (buf), "\\u%04x", c);
                    out += buf;
                }
                else out += static_cast<char> (c);
        }
    }
    return out + "\"";
}

std::string number (double v)
{
    if (v == std::floor (v) && std::abs (v) < 1e15) return std::to_string (static_cast<long long> (v));
    char buf[32];
    std::snprintf (buf, sizeof (buf), "%.6g", v);
    return buf;
}

//==============================================================================
// Just enough JSON to read our own files back.

struct Json
{
    enum Type { null, boolean, num, str, arr, obj } type = null;
    bool b = false;
    double n = 0;
    std::string s;
    std::vector<Json> a;
    std::map<std::string, Json> o;

    const Json& operator[] (const std::string& key) const
    {
        static const Json none;
        auto it = o.find (key);
        return it == o.end() ? none : it->second;
    }
    double num_or (double fallback) const { return type == num ? n : fallback; }
    std::string str_or (const std::string& fallback) const { return type == str ? s : fallback; }
    bool bool_or (bool fallback) const { return type == boolean ? b : fallback; }
};

struct Parser
{
    const std::string& t;
    size_t p = 0;
    bool bad = false;

    void ws() { while (p < t.size() && (t[p] == ' ' || t[p] == '\n' || t[p] == '\r' || t[p] == '\t')) ++p; }
    bool eat (char c) { ws(); if (p < t.size() && t[p] == c) { ++p; return true; } return false; }

    Json value (int depth = 0)
    {
        Json j;
        ws();
        if (p >= t.size() || depth > 64) { bad = true; return j; }
        const char c = t[p];
        if (c == '{')
        {
            ++p;
            j.type = Json::obj;
            if (eat ('}')) return j;
            do
            {
                ws();
                Json k = string();
                if (! eat (':')) { bad = true; return j; }
                j.o[k.s] = value (depth + 1);
            } while (! bad && eat (','));
            if (! eat ('}')) bad = true;
        }
        else if (c == '[')
        {
            ++p;
            j.type = Json::arr;
            if (eat (']')) return j;
            do { j.a.push_back (value (depth + 1)); } while (! bad && eat (','));
            if (! eat (']')) bad = true;
        }
        else if (c == '"') j = string();
        else if (t.compare (p, 4, "true") == 0) { p += 4; j.type = Json::boolean; j.b = true; }
        else if (t.compare (p, 5, "false") == 0) { p += 5; j.type = Json::boolean; }
        else if (t.compare (p, 4, "null") == 0) { p += 4; }
        else
        {
            const char* start = t.c_str() + p;
            char* end = nullptr;
            j.n = std::strtod (start, &end);
            if (end == start) { bad = true; return j; }
            j.type = Json::num;
            p += static_cast<size_t> (end - start);
        }
        return j;
    }

    Json string()
    {
        Json j;
        j.type = Json::str;
        if (p >= t.size() || t[p] != '"') { bad = true; return j; }
        ++p;
        while (p < t.size() && t[p] != '"')
        {
            char c = t[p++];
            if (c == '\\' && p < t.size())
            {
                const char e = t[p++];
                switch (e)
                {
                    case 'n': c = '\n'; break;
                    case 'r': c = '\r'; break;
                    case 't': c = '\t'; break;
                    case 'u':
                    {
                        const unsigned code = static_cast<unsigned> (std::strtoul (t.substr (p, 4).c_str(), nullptr, 16));
                        p += 4;
                        if (code < 0x80) c = static_cast<char> (code);
                        else if (code < 0x800)
                        {
                            j.s += static_cast<char> (0xc0 | (code >> 6));
                            c = static_cast<char> (0x80 | (code & 0x3f));
                        }
                        else
                        {
                            j.s += static_cast<char> (0xe0 | (code >> 12));
                            j.s += static_cast<char> (0x80 | ((code >> 6) & 0x3f));
                            c = static_cast<char> (0x80 | (code & 0x3f));
                        }
                        break;
                    }
                    default: c = e;
                }
            }
            j.s += c;
        }
        if (p >= t.size()) bad = true;
        else ++p;
        return j;
    }
};
} // namespace

std::string saveScore (const Score& score)
{
    std::ostringstream out;
    out << "{\n  \"format\": \"noterator\",\n  \"version\": 1,\n";
    out << "  \"title\": " << quote (score.title) << ",\n";
    out << "  \"composer\": " << quote (score.composer) << ",\n";
    out << "  \"bars\": " << score.bars << ",\n";
    out << "  \"meters\": [";
    for (size_t i = 0; i < score.meters.size(); ++i)
    {
        const auto& m = score.meters[i];
        out << (i ? ", " : "") << "{ \"bar\": " << m.bar << ", \"num\": " << m.num << ", \"den\": " << m.den << " }";
    }
    out << "],\n  \"keys\": [";
    for (size_t i = 0; i < score.keys.size(); ++i)
    {
        const auto& k = score.keys[i];
        out << (i ? ", " : "") << "{ \"bar\": " << k.bar
            << ", \"root\": " << quote (scaleview::roots[static_cast<size_t> (k.root)].name)
            << ", \"scale\": " << quote (scaleview::scales[static_cast<size_t> (k.scale)].name) << " }";
    }
    out << "],\n  \"tempos\": [";
    for (size_t i = 0; i < score.tempos.size(); ++i)
        out << (i ? ", " : "") << "{ \"tick\": " << score.tempos[i].at << ", \"bpm\": " << number (score.tempos[i].bpm) << " }";
    out << "],\n  \"chordRoots\": [";
    for (size_t i = 0; i < score.chordRoots.size(); ++i)
    {
        const auto& c = score.chordRoots[i];
        out << (i ? ", " : "") << "{ \"start\": " << c.start << ", \"end\": " << c.end << ", \"root\": " << c.root
            << ", \"pitchClasses\": [";
        for (size_t k = 0; k < c.pitchClasses.size(); ++k) out << (k ? ", " : "") << c.pitchClasses[k];
        out << "] }";
    }
    out << "],\n  \"ppq\": " << PPQ << ",\n  \"parts\": [";
    for (size_t pi = 0; pi < score.parts.size(); ++pi)
    {
        const auto& p = score.parts[pi];
        out << (pi ? "," : "") << "\n    {\n      \"name\": " << quote (p.name)
            << ",\n      \"instrument\": " << quote (p.instrument)
            << ",\n      \"autoCC\": " << (p.autoCC ? "true" : "false")
            << ",\n      \"mute\": " << (p.mute ? "true" : "false")
            << ",\n      \"solo\": " << (p.solo ? "true" : "false")
            << ",\n      \"volume\": " << number (p.volume)
            << ",\n      \"notes\": [";
        // [start, length, pitch, velocity, voice]: a score is mostly notes,
        // and an object per note would triple the file.
        for (size_t ni = 0; ni < p.notes.size(); ++ni)
        {
            const auto& n = p.notes[ni];
            out << (ni ? "," : "") << (ni % 8 == 0 ? "\n        " : " ")
                << "[" << n.start << ", " << n.length << ", " << n.pitch << ", " << n.velocity << ", " << n.voice << "]";
        }
        out << (p.notes.empty() ? "" : "\n      ") << "]\n    }";
    }
    out << (score.parts.empty() ? "" : "\n  ") << "]\n}\n";
    return out.str();
}

LoadResult loadScore (const std::string& text)
{
    LoadResult res;
    Parser parser { text };
    const Json root = parser.value();
    if (parser.bad || root.type != Json::obj)
    {
        res.error = "This file is damaged: it is not a Noterator project.";
        return res;
    }
    if (root["format"].str_or ("") != "noterator")
    {
        res.error = "This is not a Noterator project.";
        return res;
    }
    Score& s = res.score;
    s.title = root["title"].str_or ("Untitled");
    s.composer = root["composer"].str_or ("");
    s.bars = std::max (1, static_cast<int> (root["bars"].num_or (16)));
    const double ppq = root["ppq"].num_or (static_cast<double> (PPQ));
    auto tick = [ppq] (double t) { return static_cast<Tick> (std::llround (t * static_cast<double> (PPQ) / ppq)); };

    s.meters.clear();
    for (const auto& m : root["meters"].a)
        s.meters.push_back ({ static_cast<int> (m["bar"].num_or (0)), static_cast<int> (m["num"].num_or (4)),
                              static_cast<int> (m["den"].num_or (4)) });
    s.keys.clear();
    for (const auto& k : root["keys"].a)
    {
        int r = rootIndexByName (k["root"].str_or ("C"));
        int sc = scaleIndexByName (k["scale"].str_or ("Major"));
        s.keys.push_back ({ static_cast<int> (k["bar"].num_or (0)), r < 0 ? 0 : r, sc < 0 ? 0 : sc });
    }
    s.tempos.clear();
    for (const auto& t : root["tempos"].a)
        s.tempos.push_back ({ tick (t["tick"].num_or (0)), t["bpm"].num_or (120) });
    // Chords made on a known root (decision 0046); older files have none.
    for (const auto& c : root["chordRoots"].a)
    {
        ChordRoot cr;
        cr.start = tick (c["start"].num_or (0));
        cr.end = tick (c["end"].num_or (0));
        cr.root = static_cast<int> (c["root"].num_or (0)) % 12;
        for (const auto& pc : c["pitchClasses"].a) cr.pitchClasses.push_back (static_cast<int> (pc.num_or (0)) % 12);
        if (cr.end > cr.start && ! cr.pitchClasses.empty()) s.chordRoots.push_back (cr);
    }

    for (const auto& pj : root["parts"].a)
    {
        Part p;
        p.name = pj["name"].str_or ("Part");
        p.instrument = pj["instrument"].str_or ("pno");
        if (! hasInstrument (p.instrument)) p.instrument = "pno";
        p.autoCC = pj["autoCC"].bool_or (true);
        p.mute = pj["mute"].bool_or (false);
        p.solo = pj["solo"].bool_or (false);
        p.volume = static_cast<float> (std::clamp (pj["volume"].num_or (0.8), 0.0, 1.0));
        for (const auto& nj : pj["notes"].a)
        {
            if (nj.type != Json::arr || nj.a.size() < 3) continue;
            Note n;
            n.start = tick (nj.a[0].num_or (0));
            n.length = tick (nj.a[1].num_or (PPQ));
            n.pitch = static_cast<int> (nj.a[2].num_or (60));
            n.velocity = nj.a.size() > 3 ? static_cast<int> (nj.a[3].num_or (100)) : 100;
            n.voice = nj.a.size() > 4 ? static_cast<int> (nj.a[4].num_or (0)) : 0;
            p.notes.push_back (n);
        }
        s.parts.push_back (std::move (p));
    }
    s.nextId = 1;
    s.normalise();
    res.ok = true;
    return res;
}

} // namespace nt
