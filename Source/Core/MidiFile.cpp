#include "MidiFile.h"

#include "Instruments.h"
#include "ScaleModel.h"
#include "Spelling.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace nt
{

namespace
{
void put16 (std::vector<uint8_t>& out, int v)
{
    out.push_back (static_cast<uint8_t> ((v >> 8) & 0xff));
    out.push_back (static_cast<uint8_t> (v & 0xff));
}

void put32 (std::vector<uint8_t>& out, uint32_t v)
{
    for (int s = 24; s >= 0; s -= 8) out.push_back (static_cast<uint8_t> ((v >> s) & 0xff));
}

void putVar (std::vector<uint8_t>& out, uint32_t v)
{
    uint8_t buf[5];
    int n = 0;
    buf[n++] = static_cast<uint8_t> (v & 0x7f);
    while ((v >>= 7) > 0) buf[n++] = static_cast<uint8_t> ((v & 0x7f) | 0x80);
    while (n > 0) out.push_back (buf[--n]);
}

struct TrackWriter
{
    std::vector<uint8_t> data;
    Tick last = 0;

    void delta (Tick at)
    {
        putVar (data, static_cast<uint32_t> (std::max<Tick> (0, at - last)));
        last = std::max (last, at);
    }
    void meta (Tick at, uint8_t type, const std::vector<uint8_t>& payload)
    {
        delta (at);
        data.push_back (0xff);
        data.push_back (type);
        putVar (data, static_cast<uint32_t> (payload.size()));
        data.insert (data.end(), payload.begin(), payload.end());
    }
    void text (Tick at, uint8_t type, const std::string& s)
    {
        meta (at, type, std::vector<uint8_t> (s.begin(), s.end()));
    }
    void channel (Tick at, uint8_t status, int d1, int d2, bool twoBytes = true)
    {
        delta (at);
        data.push_back (status);
        data.push_back (static_cast<uint8_t> (std::clamp (d1, 0, 127)));
        if (twoBytes) data.push_back (static_cast<uint8_t> (std::clamp (d2, 0, 127)));
    }
    void finish (std::vector<uint8_t>& out)
    {
        meta (last, 0x2f, {});
        out.insert (out.end(), { 'M', 'T', 'r', 'k' });
        put32 (out, static_cast<uint32_t> (data.size()));
        out.insert (out.end(), data.begin(), data.end());
    }
};

int log2Den (int den)
{
    int n = 0;
    while ((1 << n) < den) ++n;
    return n;
}
} // namespace

std::vector<uint8_t> writeMidiFile (const Score& score, const PerformOptions& options)
{
    const auto events = perform (score, options);
    const Tick from = std::max<Tick> (0, options.from);
    const Tick to = options.to < 0 ? score.endTick() : options.to;

    std::vector<uint8_t> out { 'M', 'T', 'h', 'd' };
    put32 (out, 6);
    put16 (out, 1);
    put16 (out, static_cast<int> (score.parts.size()) + 1);
    put16 (out, static_cast<int> (PPQ));

    // The conductor.
    {
        TrackWriter t;
        t.text (0, 0x03, score.title);
        struct Item { Tick at; int order; std::vector<uint8_t> payload; uint8_t type; };
        std::vector<Item> items;
        for (const auto& m : score.meters)
        {
            items.push_back ({ score.barStart (m.bar), 0, { static_cast<uint8_t> (m.num), static_cast<uint8_t> (log2Den (m.den)), 24, 8 }, 0x58 });
        }
        for (const auto& k : score.keys)
        {
            const auto ctx = keyContext (k.root, k.scale);
            // A minor-family scale is written as minor, everything else as major.
            const bool minor = std::string (scaleview::scales[static_cast<size_t> (k.scale)].name).find ("Minor") != std::string::npos
                               || std::string (scaleview::scales[static_cast<size_t> (k.scale)].name) == "Aeolian";
            items.push_back ({ score.barStart (k.bar), 1,
                               { static_cast<uint8_t> (static_cast<int8_t> (ctx.signature.fifths())), static_cast<uint8_t> (minor ? 1 : 0) }, 0x59 });
        }
        for (const auto& tp : score.tempos)
        {
            const auto us = static_cast<uint32_t> (std::llround (60000000.0 / tp.bpm));
            items.push_back ({ tp.at, 2, { static_cast<uint8_t> ((us >> 16) & 0xff), static_cast<uint8_t> ((us >> 8) & 0xff),
                                           static_cast<uint8_t> (us & 0xff) }, 0x51 });
        }
        // Each kind as it stands at `from`, then whatever changes inside the range.
        std::stable_sort (items.begin(), items.end(), [] (const Item& a, const Item& b)
        {
            if (a.at != b.at) return a.at < b.at;
            return a.order < b.order;
        });
        for (int order = 0; order < 3; ++order)
        {
            const Item* inForce = nullptr;
            for (const auto& it : items)
                if (it.order == order && it.at <= from) inForce = &it;
            if (inForce) t.meta (0, inForce->type, inForce->payload);
        }
        for (const auto& it : items)
            if (it.at > from && it.at < to) t.meta (it.at - from, it.type, it.payload);
        t.finish (out);
    }

    for (size_t pi = 0; pi < score.parts.size(); ++pi)
    {
        TrackWriter t;
        t.text (0, 0x03, score.parts[pi].name);
        for (const auto& e : events)
        {
            if (e.part != static_cast<int> (pi)) continue;
            const auto ch = static_cast<uint8_t> (e.channel & 0x0f);
            switch (e.type)
            {
                case PlayEvent::noteOn:     t.channel (e.at, static_cast<uint8_t> (0x90 | ch), e.data1, std::max (1, e.data2)); break;
                case PlayEvent::noteOff:    t.channel (e.at, static_cast<uint8_t> (0x80 | ch), e.data1, 64); break;
                case PlayEvent::controller: t.channel (e.at, static_cast<uint8_t> (0xb0 | ch), e.data1, e.data2); break;
                case PlayEvent::program:    t.channel (e.at, static_cast<uint8_t> (0xc0 | ch), e.data1, 0, false); break;
            }
        }
        t.finish (out);
    }
    return out;
}

//==============================================================================

std::string instrumentForProgram (int program, const std::string& fallback)
{
    // Exact matches first, in table order, so program 40 is Violin I.
    for (const auto& i : instruments())
        if (! i.drums && i.program == program) return i.id;
    // Then the family the program number sits in.
    if (program >= 0 && program < 8) return "pno";
    if (program >= 24 && program < 32) return "gtr";
    if (program >= 32 && program < 40) return "ebass";
    if (program >= 40 && program < 56) return "vln1";
    if (program >= 56 && program < 64) return "tpt";
    if (program >= 64 && program < 72) return "asax";
    if (program >= 72 && program < 80) return "fl";
    return fallback;
}

namespace
{
struct Reader
{
    const std::vector<uint8_t>& b;
    size_t p = 0;
    bool bad = false;

    bool has (size_t n) const { return p + n <= b.size(); }
    uint8_t u8() { if (! has (1)) { bad = true; return 0; } return b[p++]; }
    uint32_t u16() { const uint32_t a = u8(); return (a << 8) | u8(); }
    uint32_t u32() { const uint32_t a = u16(); return (a << 16) | u16(); }
    uint32_t var()
    {
        uint32_t v = 0;
        for (int i = 0; i < 4; ++i)
        {
            const uint8_t c = u8();
            v = (v << 7) | (c & 0x7f);
            if ((c & 0x80) == 0) break;
        }
        return v;
    }
};

// The root a key signature names, as a scaleview index.
int rootForSignature (int fifths, bool minor)
{
    static const char* majors[15] = { "Cb", "Gb", "Db", "Ab", "Eb", "Bb", "F", "C", "G", "D", "A", "E", "B", "F#", "C#" };
    static const char* minors[15] = { "Ab", "Eb", "Bb", "F", "C", "G", "D", "A", "E", "B", "F#", "C#", "G#", "D#", "A#" };
    const int i = std::clamp (fifths, -7, 7) + 7;
    const int idx = rootIndexByName ((minor ? minors : majors)[i]);
    return idx < 0 ? 0 : idx;
}
} // namespace

MidiReadResult readMidiFile (const std::vector<uint8_t>& bytes, const std::string& fallbackInstrument)
{
    MidiReadResult res;
    Reader r { bytes };
    if (! r.has (14) || bytes[0] != 'M' || bytes[1] != 'T' || bytes[2] != 'h' || bytes[3] != 'd')
    {
        res.error = "This is not a MIDI file.";
        return res;
    }
    r.p = 4;
    const uint32_t headerLen = r.u32();
    r.u16();   // format: 0 and 1 read the same way here
    const uint32_t trackCount = r.u16();
    const uint32_t division = r.u16();
    r.p = 8 + headerLen;
    if (division & 0x8000)
    {
        res.error = "This MIDI file counts time in SMPTE frames, which is not supported yet.";
        return res;
    }
    const double scale = static_cast<double> (PPQ) / std::max<uint32_t> (1, division);
    auto toTick = [scale] (uint64_t t) { return static_cast<Tick> (std::llround (static_cast<double> (t) * scale)); };

    Score& score = res.score;
    score.parts.clear();
    score.meters.clear();
    score.keys.clear();
    score.tempos.clear();

    struct Pending { Tick start; int velocity; };
    struct Bucket { std::string name; int program = -1; std::vector<Note> notes; int track; int channel; };
    std::map<std::pair<int, int>, Bucket> buckets;   // (track, channel)
    std::vector<std::pair<Tick, std::pair<int, int>>> meterAt;   // tick -> num, den
    std::vector<std::pair<Tick, std::pair<int, bool>>> keyAt;
    std::string firstTrackName;

    for (uint32_t track = 0; track < trackCount && r.has (8); ++track)
    {
        if (! (bytes[r.p] == 'M' && bytes[r.p + 1] == 'T' && bytes[r.p + 2] == 'r' && bytes[r.p + 3] == 'k'))
        {
            // Skip an unknown chunk.
            r.p += 4;
            const uint32_t len = r.u32();
            r.p += len;
            --track;
            continue;
        }
        r.p += 4;
        const uint32_t len = r.u32();
        const size_t end = std::min (bytes.size(), r.p + len);
        uint64_t now = 0;
        uint8_t status = 0;
        std::string trackName;
        std::map<std::pair<int, int>, std::vector<Pending>> open;   // (channel, pitch)
        const int t = static_cast<int> (track);

        while (r.p < end && ! r.bad)
        {
            now += r.var();
            uint8_t c = r.u8();
            if (c == 0xff)
            {
                const uint8_t type = r.u8();
                const uint32_t n = r.var();
                if (! r.has (n)) { r.bad = true; break; }
                const uint8_t* d = bytes.data() + r.p;
                if (type == 0x03) trackName.assign (reinterpret_cast<const char*> (d), n);
                else if (type == 0x51 && n >= 3)
                {
                    const uint32_t us = (static_cast<uint32_t> (d[0]) << 16) | (static_cast<uint32_t> (d[1]) << 8) | d[2];
                    if (us > 0) score.tempos.push_back ({ toTick (now), 60000000.0 / us });
                }
                else if (type == 0x58 && n >= 2) meterAt.push_back ({ toTick (now), { d[0], 1 << std::min<int> (d[1], 6) } });
                else if (type == 0x59 && n >= 2) keyAt.push_back ({ toTick (now), { static_cast<int8_t> (d[0]), d[1] != 0 } });
                r.p += n;
                continue;
            }
            if (c == 0xf0 || c == 0xf7)
            {
                r.p += r.var();
                continue;
            }
            uint8_t d1;
            if (c & 0x80) { status = c; d1 = r.u8(); }
            else { d1 = c; }
            const int kind = status & 0xf0, ch = status & 0x0f;
            const bool two = kind != 0xc0 && kind != 0xd0;
            const uint8_t d2 = two ? r.u8() : 0;
            auto& bucket = buckets[{ t, ch }];
            bucket.track = t;
            bucket.channel = ch;
            if (kind == 0x90 && d2 > 0)
                open[{ ch, d1 }].push_back ({ toTick (now), d2 });
            else if (kind == 0x80 || (kind == 0x90 && d2 == 0))
            {
                auto& list = open[{ ch, d1 }];
                if (! list.empty())
                {
                    const auto pend = list.front();
                    list.erase (list.begin());
                    Note n;
                    n.start = pend.start;
                    n.length = std::max<Tick> (1, toTick (now) - pend.start);
                    n.pitch = d1;
                    n.velocity = pend.velocity;
                    bucket.notes.push_back (n);
                }
            }
            else if (kind == 0xc0 && bucket.program < 0) bucket.program = d1;
        }
        // Anything never let go of ends with the track.
        for (auto& [key, list] : open)
            for (const auto& pend : list)
            {
                Note n;
                n.start = pend.start;
                n.length = std::max<Tick> (PPQ / 4, toTick (now) - pend.start);
                n.pitch = key.second;
                n.velocity = pend.velocity;
                auto& bucket = buckets[{ t, key.first }];
                bucket.track = t;
                bucket.channel = key.first;
                bucket.notes.push_back (n);
            }
        for (auto& [key, bucket] : buckets)
            if (key.first == t) bucket.name = trackName;
        if (track == 0) firstTrackName = trackName;
        r.p = end;
    }

    // Meters become bar-based. A change off a bar line is moved to the next one.
    if (meterAt.empty()) meterAt.push_back ({ 0, { 4, 4 } });
    std::stable_sort (meterAt.begin(), meterAt.end(), [] (const auto& a, const auto& b) { return a.first < b.first; });
    score.meters.push_back ({ 0, meterAt.front().second.first, meterAt.front().second.second });
    for (size_t i = 1; i < meterAt.size(); ++i)
    {
        score.normalise();
        const int bar = score.barAt (meterAt[i].first);
        const int at = score.barStart (bar) == meterAt[i].first ? bar : bar + 1;
        Meter m { at, meterAt[i].second.first, meterAt[i].second.second };
        if (! (m.num == score.meterAtBar (at).num && m.den == score.meterAtBar (at).den))
            score.meters.push_back (m);
    }
    for (const auto& [tick, k] : keyAt)
        score.keys.push_back ({ score.barAt (tick), rootForSignature (k.first, k.second), k.second ? 1 : 0 });

    int count = 0;
    for (auto& [key, bucket] : buckets)
    {
        if (bucket.notes.empty()) continue;
        Part part;
        if (bucket.channel == 9) part.instrument = "kit";
        else if (bucket.program >= 0) part.instrument = instrumentForProgram (bucket.program, fallbackInstrument);
        else
        {
            // No program: guess from the name, then from the register.
            std::string lower = bucket.name;
            std::transform (lower.begin(), lower.end(), lower.begin(), [] (unsigned char ch) { return static_cast<char> (std::tolower (ch)); });
            part.instrument = fallbackInstrument;
            if (lower.find ("drum") != std::string::npos) part.instrument = "kit";
            else if (lower.find ("bass") != std::string::npos) part.instrument = "ebass";
            for (const auto& inst : instruments())
            {
                std::string n = inst.name;
                std::transform (n.begin(), n.end(), n.begin(), [] (unsigned char ch) { return static_cast<char> (std::tolower (ch)); });
                if (! lower.empty() && lower.find (n) != std::string::npos) { part.instrument = inst.id; break; }
            }
        }
        part.name = ! bucket.name.empty() ? bucket.name : instrumentById (part.instrument).name;
        part.notes = std::move (bucket.notes);
        score.parts.push_back (std::move (part));
        ++count;
    }
    if (! firstTrackName.empty() && score.parts.size() > 1) score.title = firstTrackName;
    else score.title = "Imported";
    score.bars = 1;
    score.normalise();
    res.ok = ! r.bad || count > 0;
    if (! res.ok) res.error = "The MIDI file is damaged.";
    return res;
}

} // namespace nt
