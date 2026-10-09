#include "Roll.h"

#include "Spelling.h"

#include <algorithm>
#include <cstdlib>

namespace nt::roll
{

//==============================================================================

std::string Grid::name() const
{
    const Tick whole = 4 * PPQ;
    std::string n = base >= whole ? "1 bar" : "1/" + std::to_string (whole / std::max<Tick> (1, base));
    return triplet ? n + "T" : n;
}

const std::vector<Tick>& gridValues()
{
    static const std::vector<Tick> v { 4 * PPQ, 2 * PPQ, PPQ, PPQ / 2, PPQ / 4, PPQ / 8 };
    return v;
}

namespace
{
// Grid lines are counted from each bar line, so a quarter grid in 7/8 still
// meets every bar line.
Tick snapTo (const Score& score, Tick t, Tick step, bool down)
{
    t = std::max<Tick> (0, t);
    step = std::max<Tick> (1, step);
    const int bar = score.barAt (t);
    const Tick start = score.barStart (bar), next = score.barStart (bar + 1);
    const Tick lo = start + ((t - start) / step) * step;
    if (down) return lo;
    // The bar line is always a line of the grid, whatever the step.
    const Tick hi = std::min (lo + step, next);
    return t - lo < hi - t ? lo : hi;
}
} // namespace

Tick snap (const Score& score, Tick t, const Grid& grid)
{
    return grid.snap ? snapTo (score, t, grid.step(), false) : std::max<Tick> (0, t);
}

Tick snapDown (const Score& score, Tick t, const Grid& grid)
{
    return grid.snap ? snapTo (score, t, grid.step(), true) : std::max<Tick> (0, t);
}

//==============================================================================

Warnings check (const Score& score)
{
    Warnings out;
    for (const auto& part : score.parts)
    {
        const auto& inst = instrumentById (part.instrument);
        if (inst.drums) continue;
        for (const auto& n : part.notes)
        {
            Warning w;
            w.outOfRange = n.pitch < inst.low || n.pitch > inst.high;
            w.outsideSweet = ! w.outOfRange && (n.pitch < inst.sweetLow || n.pitch > inst.sweetHigh);
            if (w.any()) out[n.id] = w;
        }

        // The notes in onsets: everything that starts together. Notes are
        // kept sorted by start, so one pass groups them.
        struct Onset { Tick at; size_t first, count; Tick end; };
        std::vector<Onset> onsets;
        for (size_t i = 0; i < part.notes.size(); ++i)
        {
            const auto& n = part.notes[i];
            if (! onsets.empty() && onsets.back().at == n.start) { ++onsets.back().count; onsets.back().end = std::max (onsets.back().end, n.end()); }
            else onsets.push_back ({ n.start, i, 1, n.end() });
        }

        Tick heldUntil = -1;   // the latest any note so far sounds to
        const Onset* prev = nullptr;
        for (const auto& o : onsets)
        {
            auto flag = [&] (auto setter) { for (size_t i = o.first; i < o.first + o.count; ++i) setter (out[part.notes[i].id]); };
            if (static_cast<int> (o.count) > inst.poly) flag ([] (Warning& w) { w.tooManyNotes = true; });
            // A rest of an eighth or more starts the line again, as a rest
            // does on the page.
            if (prev != nullptr && o.at - heldUntil < PPQ / 2)
            {
                const double gap = score.secondsAt (o.at) - score.secondsAt (prev->at);
                if (gap < inst.fast * 0.999) flag ([] (Warning& w) { w.tooFast = true; });
                if (inst.monophonic() && o.count == 1 && prev->count == 1
                    && std::abs (part.notes[o.first].pitch - part.notes[prev->first].pitch) > inst.leap)
                    flag ([] (Warning& w) { w.tooWide = true; });
            }
            heldUntil = std::max (heldUntil, o.end);
            prev = &o;
        }
    }
    return out;
}

//==============================================================================

namespace
{
void sortPart (Part& part)
{
    std::stable_sort (part.notes.begin(), part.notes.end(), [] (const Note& a, const Note& b)
    {
        if (a.start != b.start) return a.start < b.start;
        return a.pitch < b.pitch;
    });
}

template <typename Fn>
void forSelected (Score& score, const Selection& ids, Fn&& fn)
{
    for (auto& part : score.parts)
        for (auto& n : part.notes)
            if (ids.count (n.id) != 0) fn (part, n);
}
} // namespace

uint32_t drawNote (Score& score, uint32_t partId, Tick start, Tick length, int pitch, int velocity)
{
    auto* part = score.partById (partId);
    if (part == nullptr || length <= 0 || pitch < 0 || pitch > 127) return 0;
    start = std::max<Tick> (0, start);
    const Tick end = start + length;
    std::vector<Note> kept;
    kept.reserve (part->notes.size() + 1);
    for (auto n : part->notes)
    {
        if (n.pitch == pitch)
        {
            if (n.start >= start && n.start < end) continue;
            if (n.start < start && n.end() > start) n.length = start - n.start;
        }
        kept.push_back (n);
    }
    Note n;
    n.start = start;
    n.length = length;
    n.pitch = pitch;
    n.velocity = std::clamp (velocity, 1, 127);
    n.id = score.newId();
    kept.push_back (n);
    part->notes = std::move (kept);
    sortPart (*part);
    score.fitBars();
    return n.id;
}

bool moveNotes (Score& score, const Selection& ids, Tick by, int semitones)
{
    bool fits = true;
    forSelected (score, ids, [&] (Part&, Note& n)
    {
        if (n.start + by < 0 || n.pitch + semitones < 0 || n.pitch + semitones > 127) fits = false;
    });
    if (! fits) return false;
    forSelected (score, ids, [&] (Part&, Note& n) { n.start += by; n.pitch += semitones; });
    score.sortNotes();
    score.fitBars();
    return true;
}

std::vector<uint32_t> copyNotesBy (Score& score, const Selection& ids, Tick by, int semitones)
{
    bool fits = true;
    forSelected (score, ids, [&] (Part&, Note& n)
    {
        if (n.start + by < 0 || n.pitch + semitones < 0 || n.pitch + semitones > 127) fits = false;
    });
    std::vector<uint32_t> made;
    if (! fits) return made;
    for (auto& part : score.parts)
    {
        std::vector<Note> copies;
        for (const auto& n : part.notes)
            if (ids.count (n.id) != 0)
            {
                Note c = n;
                c.start += by;
                c.pitch += semitones;
                copies.push_back (c);
            }
        for (auto& c : copies)
        {
            c.id = score.newId();
            made.push_back (c.id);
            part.notes.push_back (c);
        }
    }
    score.sortNotes();
    score.fitBars();
    return made;
}

void resizeNotes (Score& score, const Selection& ids, Tick by, Tick minLength)
{
    minLength = std::max<Tick> (1, minLength);
    forSelected (score, ids, [&] (Part&, Note& n) { n.length = std::max (minLength, n.length + by); });
    score.fitBars();
}

void resizeStarts (Score& score, const Selection& ids, Tick by, Tick minLength)
{
    minLength = std::max<Tick> (1, minLength);
    forSelected (score, ids, [&] (Part&, Note& n)
    {
        const Tick end = n.end();
        n.start = std::clamp<Tick> (n.start + by, 0, std::max<Tick> (0, end - minLength));
        n.length = end - n.start;
    });
    score.sortNotes();
}

void setVelocities (Score& score, const std::map<uint32_t, int>& velocities)
{
    for (auto& part : score.parts)
        for (auto& n : part.notes)
        {
            const auto it = velocities.find (n.id);
            if (it != velocities.end()) n.velocity = std::clamp (it->second, 1, 127);
        }
}

void quantise (Score& score, const Selection& ids, Tick step, bool lengths)
{
    step = std::max<Tick> (1, step);
    forSelected (score, ids, [&] (Part&, Note& n)
    {
        const Tick start = snapTo (score, n.start, step, false);
        if (lengths)
        {
            Tick end = snapTo (score, n.end(), step, false);
            if (end <= start) end = start + step;
            n.length = end - start;
        }
        n.start = start;
    });
    // Two notes of one pitch now starting together are one note.
    for (auto& part : score.parts)
    {
        sortPart (part);
        std::vector<Note> kept;
        kept.reserve (part.notes.size());
        for (const auto& n : part.notes)
        {
            if (! kept.empty() && kept.back().start == n.start && kept.back().pitch == n.pitch
                && (ids.count (n.id) != 0 || ids.count (kept.back().id) != 0))
            {
                kept.back().length = std::max (kept.back().length, n.length);
                continue;
            }
            kept.push_back (n);
        }
        part.notes = std::move (kept);
    }
    score.fitBars();
}

//==============================================================================

std::string keyName (int pitch)
{
    return pitchName (pitch, keyContext (0, 0));
}

std::string drumName (int pitch)
{
    // General MIDI's percussion map, keys 35 to 81.
    static const char* names[] = {
        "Kick 2", "Kick", "Side stick", "Snare", "Clap", "Snare 2", "Low floor tom", "Closed hi-hat", "High floor tom",
        "Pedal hi-hat", "Low tom", "Open hi-hat", "Low-mid tom", "High-mid tom", "Crash", "High tom", "Ride", "China",
        "Ride bell", "Tambourine", "Splash", "Cowbell", "Crash 2", "Vibraslap", "Ride 2", "High bongo", "Low bongo",
        "Mute high conga", "Open high conga", "Low conga", "High timbale", "Low timbale", "High agogo", "Low agogo",
        "Cabasa", "Maracas", "Short whistle", "Long whistle", "Short guiro", "Long guiro", "Claves", "High wood block",
        "Low wood block", "Mute cuica", "Open cuica", "Mute triangle", "Open triangle"
    };
    if (pitch < 35 || pitch > 81) return {};
    return names[pitch - 35];
}

} // namespace nt::roll
