#include "Score.h"

#include <algorithm>
#include <cmath>

namespace nt
{

const Meter& Score::meterAtBar (int bar) const
{
    const Meter* found = &meters.front();
    for (const auto& m : meters)
        if (m.bar <= bar) found = &m;
    return *found;
}

const KeySig& Score::keyAtBar (int bar) const
{
    const KeySig* found = &keys.front();
    for (const auto& k : keys)
        if (k.bar <= bar) found = &k;
    return *found;
}

// Counted bar by bar through the meter changes rather than cached, because a
// score has a few hundred bars at most and a cache is one more thing to keep
// in step with every edit.
Tick Score::barStart (int bar) const
{
    Tick t = 0;
    for (size_t i = 0; i < meters.size(); ++i)
    {
        const int from = meters[i].bar;
        const int to = (i + 1 < meters.size()) ? meters[i + 1].bar : bar;
        if (bar <= from) break;
        const int count = std::min (bar, to) - from;
        t += static_cast<Tick> (count) * meters[i].barTicks();
        if (bar <= to) break;
    }
    return t;
}

int Score::barAt (Tick t) const
{
    if (t <= 0) return 0;
    Tick start = 0;
    for (size_t i = 0; i < meters.size(); ++i)
    {
        const Tick len = meters[i].barTicks();
        const bool last = i + 1 >= meters.size();
        const int span = last ? 0 : meters[i + 1].bar - meters[i].bar;
        if (last || t < start + span * len)
            return meters[i].bar + static_cast<int> ((t - start) / len);
        start += span * len;
    }
    return 0;
}

double Score::bpmAt (Tick t) const
{
    double bpm = tempos.front().bpm;
    for (const auto& tp : tempos)
        if (tp.at <= t) bpm = tp.bpm;
    return bpm;
}

double Score::secondsAt (Tick t) const
{
    double seconds = 0.0;
    Tick at = 0;
    double bpm = tempos.front().bpm;
    for (const auto& tp : tempos)
    {
        if (tp.at >= t) break;
        seconds += static_cast<double> (tp.at - at) / PPQ * 60.0 / bpm;
        at = tp.at;
        bpm = tp.bpm;
    }
    return seconds + static_cast<double> (t - at) / PPQ * 60.0 / bpm;
}

Tick Score::tickAtSeconds (double seconds) const
{
    double elapsed = 0.0;
    Tick at = 0;
    double bpm = tempos.front().bpm;
    for (const auto& tp : tempos)
    {
        const double span = static_cast<double> (tp.at - at) / PPQ * 60.0 / bpm;
        if (elapsed + span > seconds) break;
        elapsed += span;
        at = tp.at;
        bpm = tp.bpm;
    }
    return at + static_cast<Tick> (std::llround ((seconds - elapsed) * bpm / 60.0 * PPQ));
}

Part* Score::partById (uint32_t id)
{
    for (auto& p : parts) if (p.id == id) return &p;
    return nullptr;
}

const Part* Score::partById (uint32_t id) const
{
    for (const auto& p : parts) if (p.id == id) return &p;
    return nullptr;
}

int Score::partIndex (uint32_t id) const
{
    for (size_t i = 0; i < parts.size(); ++i)
        if (parts[i].id == id) return static_cast<int> (i);
    return -1;
}

Note* Score::findNote (uint32_t noteId, Part** owner)
{
    for (auto& p : parts)
        for (auto& n : p.notes)
            if (n.id == noteId)
            {
                if (owner != nullptr) *owner = &p;
                return &n;
            }
    return nullptr;
}

Tick Score::lastNoteEnd() const
{
    Tick end = 0;
    for (const auto& p : parts)
        for (const auto& n : p.notes)
            end = std::max (end, n.end());
    return end;
}

void Score::sortNotes()
{
    for (auto& p : parts)
        std::stable_sort (p.notes.begin(), p.notes.end(), [] (const Note& a, const Note& b)
        {
            if (a.start != b.start) return a.start < b.start;
            return a.pitch < b.pitch;
        });
}

void Score::fitBars()
{
    const Tick end = lastNoteEnd();
    while (barStart (bars) < end) ++bars;
    bars = std::max (bars, 1);
}

void Score::normalise()
{
    if (meters.empty()) meters.push_back ({});
    if (keys.empty()) keys.push_back ({});
    if (tempos.empty()) tempos.push_back ({});

    std::sort (meters.begin(), meters.end(), [] (const Meter& a, const Meter& b) { return a.bar < b.bar; });
    std::sort (keys.begin(), keys.end(), [] (const KeySig& a, const KeySig& b) { return a.bar < b.bar; });
    std::sort (tempos.begin(), tempos.end(), [] (const Tempo& a, const Tempo& b) { return a.at < b.at; });
    meters.front().bar = 0;
    keys.front().bar = 0;
    tempos.front().at = 0;

    // Two changes on one bar: the later one in the list wins, as it would
    // have if they had been applied in order.
    auto dedupe = [] (auto& list, auto sameSpot)
    {
        for (size_t i = 0; i + 1 < list.size();)
        {
            if (sameSpot (list[i], list[i + 1])) list.erase (list.begin() + static_cast<long> (i));
            else ++i;
        }
    };
    dedupe (meters, [] (const Meter& a, const Meter& b) { return a.bar == b.bar; });
    dedupe (keys, [] (const KeySig& a, const KeySig& b) { return a.bar == b.bar; });
    dedupe (tempos, [] (const Tempo& a, const Tempo& b) { return a.at == b.at; });

    for (auto& m : meters)
    {
        m.num = std::clamp (m.num, 1, 32);
        if (m.den != 1 && m.den != 2 && m.den != 4 && m.den != 8 && m.den != 16 && m.den != 32) m.den = 4;
    }
    for (auto& tp : tempos) tp.bpm = std::clamp (tp.bpm, 10.0, 400.0);

    uint32_t highest = 0;
    for (const auto& p : parts)
    {
        highest = std::max (highest, p.id);
        for (const auto& n : p.notes) highest = std::max (highest, n.id);
    }
    nextId = std::max (nextId, highest + 1);
    for (auto& p : parts)
    {
        if (p.id == 0) p.id = newId();
        for (auto& n : p.notes)
        {
            if (n.id == 0) n.id = newId();
            n.pitch = std::clamp (n.pitch, 0, 127);
            n.velocity = std::clamp (n.velocity, 1, 127);
            n.length = std::max<Tick> (n.length, 1);
            n.start = std::max<Tick> (n.start, 0);
            n.voice = std::clamp (n.voice, 0, 1);
        }
    }
    sortNotes();
    fitBars();
}

} // namespace nt
