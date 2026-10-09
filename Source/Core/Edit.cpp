#include "Edit.h"

#include <algorithm>
#include <cstdlib>

namespace nt
{

void clearRange (Part& part, Tick start, Tick length, int voice)
{
    const Tick end = start + length;
    std::vector<Note> kept;
    kept.reserve (part.notes.size());
    for (auto n : part.notes)
    {
        if (n.voice != voice) { kept.push_back (n); continue; }
        if (n.start >= start && n.start < end) continue;
        if (n.start < start && n.end() > start) n.length = start - n.start;
        kept.push_back (n);
    }
    part.notes = std::move (kept);
}

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

uint32_t writeNote (Score& score, uint32_t partId, Tick start, Tick length, int pitch, int voice, int velocity)
{
    auto* part = score.partById (partId);
    if (part == nullptr || length <= 0) return 0;
    clearRange (*part, start, length, voice);
    Note n;
    n.start = std::max<Tick> (0, start);
    n.length = length;
    n.pitch = std::clamp (pitch, 0, 127);
    n.voice = voice;
    n.velocity = std::clamp (velocity, 1, 127);
    n.id = score.newId();
    part->notes.push_back (n);
    sortPart (*part);
    score.fitBars();
    return n.id;
}

uint32_t addToChord (Score& score, uint32_t partId, Tick start, Tick length, int pitch, int voice, int velocity)
{
    auto* part = score.partById (partId);
    if (part == nullptr) return 0;
    const Note* chord = nullptr;
    for (const auto& n : part->notes)
    {
        if (n.start == start && n.voice == voice)
        {
            if (n.pitch == pitch) return n.id;   // already there
            chord = &n;
        }
    }
    if (chord == nullptr) return writeNote (score, partId, start, length, pitch, voice, velocity);
    Note n;
    n.start = start;
    n.length = chord->length;
    n.pitch = std::clamp (pitch, 0, 127);
    n.voice = voice;
    n.velocity = chord->velocity;
    n.id = score.newId();
    part->notes.push_back (n);
    sortPart (*part);
    return n.id;
}

void writeRest (Score& score, uint32_t partId, Tick start, Tick length, int voice)
{
    if (auto* part = score.partById (partId)) clearRange (*part, start, length, voice);
}

void deleteNotes (Score& score, const Selection& ids)
{
    for (auto& part : score.parts)
        part.notes.erase (std::remove_if (part.notes.begin(), part.notes.end(),
                                          [&ids] (const Note& n) { return ids.count (n.id) != 0; }),
                          part.notes.end());
}

bool transposeNotes (Score& score, const Selection& ids, int semitones)
{
    bool fits = true;
    forSelected (score, ids, [&fits, semitones] (Part&, Note& n)
    {
        if (n.pitch + semitones < 0 || n.pitch + semitones > 127) fits = false;
    });
    if (! fits) return false;
    forSelected (score, ids, [semitones] (Part&, Note& n) { n.pitch += semitones; });
    score.sortNotes();
    return true;
}

bool moveNotes (Score& score, const Selection& ids, Tick by)
{
    bool fits = true;
    forSelected (score, ids, [&fits, by] (Part&, Note& n) { if (n.start + by < 0) fits = false; });
    if (! fits) return false;
    forSelected (score, ids, [by] (Part&, Note& n) { n.start += by; });
    score.sortNotes();
    score.fitBars();
    return true;
}

void setLengths (Score& score, const Selection& ids, Tick length)
{
    forSelected (score, ids, [length] (Part&, Note& n) { n.length = std::max<Tick> (1, length); });
    score.fitBars();
}

void setVelocity (Score& score, const Selection& ids, int velocity)
{
    forSelected (score, ids, [velocity] (Part&, Note& n) { n.velocity = std::clamp (velocity, 1, 127); });
}

void setVoice (Score& score, const Selection& ids, int voice)
{
    forSelected (score, ids, [voice] (Part&, Note& n) { n.voice = std::clamp (voice, 0, 1); });
}

std::vector<uint32_t> pasteNotes (Score& score, uint32_t partId, Tick at, const std::vector<Note>& notes,
                                  Tick span, bool replace)
{
    std::vector<uint32_t> ids;
    auto* part = score.partById (partId);
    if (part == nullptr) return ids;
    if (replace && span > 0)
    {
        clearRange (*part, at, span, 0);
        clearRange (*part, at, span, 1);
    }
    for (auto n : notes)
    {
        n.start += at;
        if (n.start < 0) continue;
        n.id = score.newId();
        n.pitch = std::clamp (n.pitch, 0, 127);
        n.velocity = std::clamp (n.velocity, 1, 127);
        n.voice = std::clamp (n.voice, 0, 1);
        n.length = std::max<Tick> (1, n.length);
        part->notes.push_back (n);
        ids.push_back (n.id);
    }
    sortPart (*part);
    score.fitBars();
    return ids;
}

void insertBars (Score& score, int bar, int count)
{
    if (count <= 0) return;
    const Tick at = score.barStart (bar);
    const Tick len = static_cast<Tick> (count) * score.meterAtBar (bar).barTicks();
    for (auto& part : score.parts)
        for (auto& n : part.notes)
            if (n.start >= at) n.start += len;
    for (auto& m : score.meters) if (m.bar > bar) m.bar += count;
    for (auto& k : score.keys) if (k.bar > bar) k.bar += count;
    for (auto& tp : score.tempos) if (tp.at > at) tp.at += len;
    score.bars += count;
}

void deleteBars (Score& score, int bar, int count)
{
    count = std::min (count, score.bars - bar);
    if (count <= 0 || score.bars - count < 1) return;
    const Tick from = score.barStart (bar), to = score.barStart (bar + count);
    const Tick len = to - from;
    for (auto& part : score.parts)
    {
        std::vector<Note> kept;
        for (auto n : part.notes)
        {
            if (n.start >= from && n.start < to) continue;
            if (n.start < from && n.end() > from) n.length = std::max<Tick> (1, from - n.start);
            if (n.start >= to) n.start -= len;
            kept.push_back (n);
        }
        part.notes = std::move (kept);
    }
    auto shiftBars = [bar, count] (auto& list)
    {
        for (auto it = list.begin(); it != list.end();)
        {
            if (it->bar > bar && it->bar < bar + count && it != list.begin()) { it = list.erase (it); continue; }
            if (it->bar >= bar + count) it->bar -= count;
            ++it;
        }
    };
    shiftBars (score.meters);
    shiftBars (score.keys);
    for (auto it = score.tempos.begin(); it != score.tempos.end();)
    {
        if (it != score.tempos.begin() && it->at >= from && it->at < to) { it = score.tempos.erase (it); continue; }
        if (it->at >= to) it->at -= len;
        ++it;
    }
    score.bars -= count;
    score.normalise();
}

std::vector<Note> copyNotes (const Score& score, const Selection& ids, Tick* earliest)
{
    std::vector<Note> out;
    Tick first = -1;
    for (const auto& part : score.parts)
        for (const auto& n : part.notes)
            if (ids.count (n.id) != 0)
            {
                out.push_back (n);
                if (first < 0 || n.start < first) first = n.start;
            }
    for (auto& n : out) n.start -= std::max<Tick> (0, first);
    if (earliest != nullptr) *earliest = std::max<Tick> (0, first);
    return out;
}

Selection notesInRange (const Score& score, Tick from, Tick to, const std::vector<uint32_t>& parts)
{
    Selection ids;
    for (const auto& part : score.parts)
    {
        if (! parts.empty() && std::find (parts.begin(), parts.end(), part.id) == parts.end()) continue;
        for (const auto& n : part.notes)
            if (n.start >= from && n.start < to) ids.insert (n.id);
    }
    return ids;
}

int nearestPitchWithClass (int pc, int near)
{
    pc = ((pc % 12) + 12) % 12;
    int best = near, gap = 1000;
    for (int p = near - 6; p <= near + 6; ++p)
    {
        if (((p % 12) + 12) % 12 != pc) continue;
        const int g = std::abs (p - near);
        if (g < gap) { gap = g; best = p; }
    }
    return std::clamp (best, 0, 127);
}

} // namespace nt
