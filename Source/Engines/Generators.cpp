#include "Generators.h"
#include "Orchestrate.h"

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
    // Played once: cut where the span ends, the rest of a longer span left
    // empty (decision 0042).
    for (auto& p : out.parts)
    {
        const auto source = p.notes;
        p.notes.clear();
        for (auto n : source)
        {
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

bool isSingleLine (const GeneratedResult& result)
{
    int lines = 0;
    for (const auto& gp : result.parts)
    {
        if (gp.notes.empty()) continue;
        if (gp.drums) return false;
        std::string n = gp.name;
        std::transform (n.begin(), n.end(), n.begin(), [] (unsigned char ch) { return static_cast<char> (std::tolower (ch)); });
        if (n.find ("chord") != std::string::npos || polyphonyOf (gp.notes) > 2) return false;
        ++lines;
    }
    return lines == 1;
}

InsertReport insertWhole (Score& score, const GeneratedResult& result, uint32_t partId, Tick at, Tick span,
                          const InsertOptions& options)
{
    InsertReport report;
    const auto* p = score.partById (partId);
    if (p == nullptr) return report;
    const auto& inst = instrumentById (p->instrument);
    const auto source = span > 0 ? fitToSpan (result, span) : result;
    std::vector<Note> notes;
    for (const auto& gp : source.parts)
    {
        if (gp.notes.empty()) continue;
        if (gp.drums != inst.drums) { report.unplaced.push_back (gp.name); continue; }
        notes.insert (notes.end(), gp.notes.begin(), gp.notes.end());
    }
    if (notes.empty()) return report;
    // Thinned first, so the register is chosen for the line that is played.
    if (options.fitPolyphony)
    {
        int dropped = 0;
        notes = fitToPolyphony (notes, inst, &dropped);
        if (dropped > 0) report.thinnedParts.push_back (partId);
    }
    if (options.fitToInstrument) notes = fitToInstrument (notes, inst);
    const Tick length = span > 0 ? span : source.length;
    const auto ids = pasteNotes (score, partId, at, notes, length, options.replace);
    report.newNotes.insert (ids.begin(), ids.end());
    score.fitBars();
    return report;
}

InsertReport insertIntoRange (Score& score, const GeneratedResult& result, const std::vector<uint32_t>& parts,
                              Tick from, Tick to, const InsertOptions& options)
{
    const auto fitted = fitToSpan (result, to - from);
    InsertOptions o = options;
    o.replace = true;
    if (parts.empty()) return {};
    if (parts.size() == 1) return insertWhole (score, fitted, parts.front(), from, 0, o);

    // Several parts: shared out by what each instrument is (decision 0040).
    return orchestrate (score, fitted, parts, from, to, o);
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

std::vector<Note> fitToPolyphony (std::vector<Note> notes, const Instrument& inst, int* dropped)
{
    if (dropped != nullptr) *dropped = 0;
    if (inst.drums || notes.empty()) return notes;
    const size_t most = static_cast<size_t> (std::max (1, inst.poly));
    const bool keepLow = inst.role == 'B';
    std::sort (notes.begin(), notes.end(), [] (const Note& a, const Note& b) { return a.start != b.start ? a.start < b.start : a.pitch < b.pitch; });
    std::vector<Note> kept;
    kept.reserve (notes.size());
    for (size_t i = 0; i < notes.size();)
    {
        // The notes that start together, low to high.
        size_t j = i;
        while (j < notes.size() && notes[j].start == notes[i].start) ++j;
        const Tick at = notes[i].start;
        size_t from = i, to = j;
        if (to - from > most)
        {
            if (dropped != nullptr) *dropped += static_cast<int> (to - from - most);
            if (keepLow) to = from + most;
            else from = to - most;
        }
        // Make room: the notes still sounding that started earliest end here.
        for (;;)
        {
            size_t sounding = 0, earliest = kept.size();
            for (size_t k = 0; k < kept.size(); ++k)
                if (kept[k].end() > at)
                {
                    ++sounding;
                    if (earliest == kept.size() || kept[k].start < kept[earliest].start) earliest = k;
                }
            if (sounding + (to - from) <= most || earliest == kept.size()) break;
            kept[earliest].length = at - kept[earliest].start;
        }
        for (size_t k = from; k < to; ++k) kept.push_back (notes[k]);
        i = j;
    }
    return kept;
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
        if (options.fitPolyphony)
        {
            int dropped = 0;
            notes = fitToPolyphony (notes, instrumentById (instrument), &dropped);
            if (dropped > 0) report.thinnedParts.push_back (partId);
        }
        const auto ids = pasteNotes (score, partId, at, notes, options.replace ? span : 0, options.replace);
        report.newNotes.insert (ids.begin(), ids.end());
    }
    score.fitBars();
    return report;
}

} // namespace nt
