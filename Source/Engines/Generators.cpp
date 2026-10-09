#include "Generators.h"

#include <algorithm>
#include <cmath>

namespace nt
{

std::vector<Note> fitToInstrument (std::vector<Note> notes, const Instrument& inst)
{
    if (notes.empty() || inst.drums) return notes;
    int bestShift = 0;
    double bestCost = 1e18;
    for (int shift = -48; shift <= 48; shift += 12)
    {
        double cost = std::abs (shift) * 0.01;
        for (const auto& n : notes)
        {
            const int p = n.pitch + shift;
            if (p < inst.low || p > inst.high) cost += 10;
            else if (p < inst.sweetLow || p > inst.sweetHigh) cost += 1;
        }
        if (cost < bestCost) { bestCost = cost; bestShift = shift; }
    }
    for (auto& n : notes)
    {
        n.pitch += bestShift;
        while (n.pitch < inst.low && n.pitch + 12 <= 127) n.pitch += 12;
        while (n.pitch > inst.high && n.pitch - 12 >= 0) n.pitch -= 12;
    }
    return notes;
}

GeneratorContext contextFor (const Score& score, uint32_t partId, Tick at, const Selection& selection)
{
    GeneratorContext ctx;
    const int bar = score.barAt (at);
    const auto& k = score.keyAtBar (bar);
    const auto& m = score.meterAtBar (bar);
    ctx.root = k.root;
    ctx.scale = k.scale;
    ctx.num = m.num;
    ctx.den = m.den;
    ctx.bpm = score.bpmAt (at);
    if (const auto* part = score.partById (partId)) ctx.instrument = part->instrument;

    if (! selection.empty())
    {
        Tick first = -1, last = 0;
        bool drums = true;
        for (const auto& part : score.parts)
            for (const auto& n : part.notes)
                if (selection.count (n.id) != 0)
                {
                    if (first < 0 || n.start < first) first = n.start;
                    last = std::max (last, n.end());
                    drums = drums && instrumentById (part.instrument).drums;
                }
        const int firstBar = score.barAt (first);
        const Tick origin = score.barStart (firstBar);
        int lastBar = score.barAt (std::max<Tick> (origin, last - 1));
        ctx.selectionLength = score.barStart (lastBar + 1) - origin;
        ctx.selectionIsDrums = drums;
        for (const auto& part : score.parts)
            for (const auto& n : part.notes)
                if (selection.count (n.id) != 0)
                {
                    Note c = n;
                    c.start -= origin;
                    ctx.selection.push_back (c);
                }
        std::sort (ctx.selection.begin(), ctx.selection.end(), [] (const Note& a, const Note& b)
        {
            return a.start != b.start ? a.start < b.start : a.pitch < b.pitch;
        });
        const auto& sk = score.keyAtBar (firstBar);
        ctx.root = sk.root;
        ctx.scale = sk.scale;
        ctx.bars = std::max (1, lastBar - firstBar + 1);
    }
    return ctx;
}

GeneratedResult fitToSpan (const GeneratedResult& result, Tick span)
{
    GeneratedResult out = result;
    if (span <= 0) return out;
    Tick len = result.length;
    if (len <= 0)
        for (const auto& p : result.parts)
            for (const auto& n : p.notes) len = std::max (len, n.end());
    if (len <= 0) return out;
    for (auto& p : out.parts)
    {
        const auto source = p.notes;
        p.notes.clear();
        for (Tick offset = 0; offset < span; offset += len)
            for (auto n : source)
            {
                n.start += offset;
                if (n.start >= span) continue;
                n.length = std::min (n.length, span - n.start);
                p.notes.push_back (n);
            }
    }
    out.length = span;
    return out;
}

std::vector<std::vector<Note>> spreadChords (const std::vector<Note>& notes, int lines)
{
    std::vector<std::vector<Note>> out (static_cast<size_t> (std::max (0, lines)));
    if (lines <= 0) return out;
    std::vector<Note> sorted (notes);
    std::stable_sort (sorted.begin(), sorted.end(), [] (const Note& a, const Note& b)
    {
        return a.start != b.start ? a.start < b.start : a.pitch > b.pitch;
    });
    for (size_t i = 0; i < sorted.size();)
    {
        size_t j = i;
        while (j < sorted.size() && sorted[j].start == sorted[i].start) ++j;
        // sorted[i..j) is one chord, highest first.
        const size_t count = j - i;
        for (size_t line = 0; line < out.size(); ++line)
        {
            const size_t pick = std::min (line, count - 1);
            out[line].push_back (sorted[i + pick]);
        }
        i = j;
    }
    return out;
}

InsertReport insertIntoRange (Score& score, const GeneratedResult& result, const std::vector<uint32_t>& parts,
                              Tick from, Tick to, const InsertOptions& options)
{
    const auto fitted = fitToSpan (result, to - from);
    InsertOptions o = options;
    o.replace = true;
    if (parts.size() <= 1)
    {
        if (const auto* p = parts.empty() ? nullptr : score.partById (parts.front())) o.contextInstrument = p->instrument;
        return insertResult (score, fitted, parts.empty() ? 0 : parts.front(), from, o);
    }

    // Who is who among the selected parts, top to bottom.
    std::vector<uint32_t> pitched, kits;
    for (auto id : parts)
        if (const auto* p = score.partById (id)) (instrumentById (p->instrument).drums ? kits : pitched).push_back (id);
    if (const auto* p = score.partById (parts.front())) o.contextInstrument = p->instrument;

    auto lower = [] (std::string n) { std::transform (n.begin(), n.end(), n.begin(), [] (unsigned char c) { return static_cast<char> (std::tolower (c)); }); return n; };
    const GeneratedPart* melody = nullptr;
    const GeneratedPart* bass = nullptr;
    const GeneratedPart* chords = nullptr;
    const GeneratedPart* drums = nullptr;
    std::vector<const GeneratedPart*> others;
    for (const auto& gp : fitted.parts)
    {
        const auto n = lower (gp.name);
        if (gp.drums && drums == nullptr) drums = &gp;
        else if (n.find ("bass") != std::string::npos && bass == nullptr) bass = &gp;
        else if ((n.find ("chord") != std::string::npos || polyphonyOf (gp.notes) > 1) && chords == nullptr) chords = &gp;
        else if (melody == nullptr && ! gp.drums) melody = &gp;
        else others.push_back (&gp);
    }

    InsertReport report;
    auto put = [&] (uint32_t partId, std::vector<Note> notes)
    {
        const auto* p = score.partById (partId);
        if (p == nullptr) return;
        if (o.fitToInstrument) notes = fitToInstrument (notes, instrumentById (p->instrument));
        const auto ids = pasteNotes (score, partId, from, notes, to - from, true);
        report.newNotes.insert (ids.begin(), ids.end());
    };

    GeneratedResult leftover;
    leftover.length = fitted.length;
    size_t top = 0, bottom = pitched.size();   // the pitched parts still free: [top, bottom)
    if (melody != nullptr && top < bottom) put (pitched[top++], melody->notes);
    else if (melody != nullptr) leftover.parts.push_back (*melody);
    if (bass != nullptr && bottom > top) put (pitched[--bottom], bass->notes);
    else if (bass != nullptr) leftover.parts.push_back (*bass);
    if (chords != nullptr)
    {
        std::vector<uint32_t> middle (pitched.begin() + static_cast<long> (top), pitched.begin() + static_cast<long> (bottom));
        const int poly = polyphonyOf (chords->notes);
        uint32_t whole = 0;
        for (auto id : middle)
            if (instrumentById (score.partById (id)->instrument).poly >= poly) { whole = id; break; }
        if (whole != 0) put (whole, chords->notes);
        else if (! middle.empty())
        {
            const auto lines = spreadChords (chords->notes, static_cast<int> (middle.size()));
            for (size_t i = 0; i < middle.size(); ++i) put (middle[i], lines[i]);
        }
        else leftover.parts.push_back (*chords);
    }
    if (drums != nullptr)
    {
        if (! kits.empty()) put (kits.front(), drums->notes);
        else leftover.parts.push_back (*drums);
    }
    for (const auto* gp : others) leftover.parts.push_back (*gp);
    // Whatever found no selected part goes where it would have gone anyway.
    if (! leftover.parts.empty())
    {
        const auto more = insertResult (score, leftover, 0, from, o);
        report.newNotes.insert (more.newNotes.begin(), more.newNotes.end());
        report.newParts = more.newParts;
    }
    score.fitBars();
    return report;
}

std::string instrumentForGeneratedPart (const GeneratedPart& part, const Instrument& target)
{
    if (part.drums) return "kit";
    if (! part.instrument.empty() && hasInstrument (part.instrument)) return part.instrument;
    std::string name = part.name;
    std::transform (name.begin(), name.end(), name.begin(), [] (unsigned char c) { return static_cast<char> (std::tolower (c)); });
    const bool orchestral = target.family == "Strings" || target.family == "Woodwind" || target.family == "Brass";
    if (name.find ("drum") != std::string::npos) return "kit";
    if (name.find ("bass") != std::string::npos)
    {
        if (target.family == "Strings") return "vc";
        if (target.family == "Woodwind") return "bsn";
        if (target.family == "Brass") return "tbn";
        if (target.family == "Voices") return "bass";
        return "ebass";
    }
    if (name.find ("chord") != std::string::npos || name.find ("harmony") != std::string::npos)
        return orchestral ? (target.family == "Strings" ? "vla" : "hn") : "pno";
    if (name.find ("second") != std::string::npos || name.find ("voice") != std::string::npos)
        return target.family == "Strings" ? "vln2" : target.id;
    return target.id;
}

int polyphonyOf (const std::vector<Note>& notes)
{
    std::vector<std::pair<Tick, int>> edges;
    for (const auto& n : notes) { edges.push_back ({ n.start, 1 }); edges.push_back ({ n.end(), -1 }); }
    std::sort (edges.begin(), edges.end(), [] (const auto& a, const auto& b) { return a.first != b.first ? a.first < b.first : a.second < b.second; });
    int now = 0, most = 0;
    for (const auto& [t, d] : edges) { now += d; most = std::max (most, now); }
    return most;
}

namespace
{
bool silentIn (const Part& p, Tick from, Tick to)
{
    for (const auto& n : p.notes)
        if (n.start < to && n.end() > from) return false;
    return true;
}
} // namespace

InsertReport insertResult (Score& score, const GeneratedResult& result, uint32_t targetPartId, Tick at,
                           const InsertOptions& options)
{
    InsertReport report;
    if (result.parts.empty()) return report;
    // Ids and names only: the parts list grows below, which would leave a
    // pointer into it dangling.
    uint32_t targetId = 0;
    std::string targetInstrument = options.contextInstrument;
    if (const auto* t = score.partById (targetPartId)) { targetId = t->id; targetInstrument = t->instrument; }
    const auto& targetInst = instrumentById (targetInstrument);
    const Tick span = result.length > 0 ? result.length : 0;
    std::vector<uint32_t> used;

    for (size_t i = 0; i < result.parts.size(); ++i)
    {
        const auto& gp = result.parts[i];
        uint32_t partId = 0;
        std::string instrument;
        // The first line goes where it was asked for, unless it is drums
        // and the part is not, or the other way round.
        if (i == 0 && targetId != 0 && targetInst.drums == gp.drums)
        {
            partId = targetId;
            instrument = targetInstrument;
        }
        else
        {
            instrument = instrumentForGeneratedPart (gp, targetInst);
            if (instrumentById (instrument).poly < polyphonyOf (gp.notes)) instrument = "pno";
            const Tick to = at + std::max<Tick> (span, 1);
            for (const auto& p : score.parts)
                if (p.instrument == instrument && silentIn (p, at, to)
                    && std::find (used.begin(), used.end(), p.id) == used.end())
                    { partId = p.id; break; }
            if (partId == 0)
            {
                Part p;
                p.id = score.newId();
                p.instrument = instrument;
                const auto& inst = instrumentById (instrument);
                p.name = (gp.name.empty() || gp.name == "Variation") ? inst.name : gp.name;
                if (p.name == "Melody" || p.name == "Chords" || p.name == "Bass" || p.name == "Drums")
                    p.name = inst.name + " (" + p.name + ")";
                partId = p.id;
                score.parts.push_back (p);
                report.newParts.push_back (partId);
            }
        }
        used.push_back (partId);
        auto notes = gp.notes;
        if (options.fitToInstrument) notes = fitToInstrument (notes, instrumentById (instrument));
        const auto ids = pasteNotes (score, partId, at, notes, options.replace ? span : 0, options.replace);
        report.newNotes.insert (ids.begin(), ids.end());
    }
    score.fitBars();
    return report;
}

} // namespace nt
