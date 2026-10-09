#include "Engrave.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <set>

namespace nt::engrave
{

//==============================================================================
// Note values

namespace
{
std::vector<Value> buildValues()
{
    std::vector<Value> values;
    const double dotMul[3] = { 1.0, 1.5, 1.75 };
    for (int den : { 1, 2, 4, 8, 16, 32, 64 })
        for (int dots = 0; dots <= 2; ++dots)
            for (int tup : { 0, 3 })
            {
                // A dotted triplet exists and nothing here writes one; leaving it
                // in let the decomposition reach for one in place of a plain value.
                if (tup != 0 && dots > 0) continue;
                const double t = 4.0 * PPQ / den * dotMul[dots] * (tup != 0 ? 2.0 / 3.0 : 1.0);
                if (t != std::floor (t) || t <= 0) continue;
                values.push_back ({ static_cast<Tick> (t), den, dots, tup });
            }
    // Longest first; between equals, the plainer: a dotted quarter is never a
    // triplet half on the page.
    std::stable_sort (values.begin(), values.end(), [] (const Value& a, const Value& b)
    {
        if (a.ticks != b.ticks) return a.ticks > b.ticks;
        if ((a.tuplet != 0) != (b.tuplet != 0)) return a.tuplet == 0;
        return a.dots < b.dots;
    });
    return values;
}

const std::vector<Value>& values()
{
    static const std::vector<Value> v = buildValues();
    return v;
}

// The coarsest value that divides an onset: as much note as that onset can
// carry without the beat under it going missing.
Tick alignment (Tick pos, Tick cap)
{
    if (pos == 0) return cap;
    for (const auto& v : values())
        if (v.ticks <= cap && pos % v.ticks == 0) return v.ticks;
    return 1;
}

int hooks (int den)
{
    int n = 0;
    for (int d = 4; d < den; d *= 2) ++n;
    return n;
}

constexpr double padLeft = 1.4;
constexpr double padRight = 0.6;
} // namespace

const Value* singleValue (Tick ticks)
{
    for (const auto& v : values())
        if (v.ticks == ticks) return &v;
    return nullptr;
}

std::vector<Value> splitDuration (Tick pos, Tick ticks, Tick barTicks, bool rest, Tick beatTicks)
{
    std::vector<Value> out;
    if (ticks <= 0) return out;

    // A note with a symbol of its own is written as that symbol wherever it
    // falls. A rest never takes that shortcut: rests show the beat.
    if (! rest)
        if (const auto* v = singleValue (ticks)) return { *v };

    const bool compound = beatTicks % 3 == 0 && beatTicks > PPQ;
    Tick p = pos, left = ticks;
    for (int guard = 0; left > 0 && guard < 32; ++guard)
    {
        const Tick align = alignment (p % barTicks, barTicks);
        const Value* took = nullptr;
        for (const auto& v : values())
        {
            if (v.ticks > left || v.ticks > align) continue;
            if (rest)
            {
                // No dotted rests in a simple meter; in a compound one only the
                // dotted beat itself.
                if (v.dots > 0 && ! (compound && v.dots == 1 && v.ticks % beatTicks == 0)) continue;
                const Tick inBeat = (p % barTicks) % beatTicks;
                if (v.ticks < beatTicks && inBeat + v.ticks > beatTicks) continue;
                if (v.ticks > beatTicks && (v.ticks % beatTicks != 0 || inBeat != 0)) continue;
                // A triplet rest only where the gap is on a triplet grid.
                if (v.tuplet != 0 && (p % (v.ticks) != 0)) continue;
            }
            took = &v;
            break;
        }
        if (took == nullptr)
            for (const auto& v : values())
                if (v.ticks <= left) { took = &v; break; }
        if (took == nullptr) break;   // shorter than a 64th: let it go
        out.push_back (*took);
        p += took->ticks;
        left -= took->ticks;
    }
    return out;
}

//==============================================================================
// Pitch to page

DrumNote drumNote (int pitch)
{
    switch (pitch)
    {
        case 35: case 36: return { 1, false, 1 };    // kick, first space
        case 37:          return { 5, true, 0 };     // side stick
        case 38: case 40: return { 5, false, 0 };    // snare, third space
        case 39:          return { 5, true, 0 };     // clap
        case 41: case 43: return { 2, false, 0 };    // floor toms
        case 45: case 47: return { 6, false, 0 };    // mid toms
        case 48: case 50: return { 7, false, 0 };    // high toms
        case 42:          return { 9, true, 0 };     // closed hi-hat, above the staff
        case 44:          return { -1, true, 1 };    // pedal hi-hat, below
        case 46:          return { 9, true, 0 };     // open hi-hat
        case 49: case 57: return { 10, true, 0 };    // crash
        case 51: case 59: return { 8, true, 0 };     // ride, top line
        case 53:          return { 8, true, 0 };     // ride bell
        case 52: case 55: return { 10, true, 0 };    // china, splash
        case 54: case 56: return { 9, true, 0 };     // tambourine, cowbell
        default:          return { 5, false, 0 };
    }
}

int staffForPitch (const Instrument& inst, int pitch)
{
    if (inst.staves.size() < 2) return 0;
    return pitch >= inst.splitPitch ? 0 : 1;
}

int positionOf (int writtenPitch, Clef clef, const KeyContext& key, int* accidental)
{
    const auto s = spell (writtenPitch, key);
    if (accidental != nullptr) *accidental = s.accidental;
    return s.step - clefBottomStep (clef);
}

int pitchAtPosition (int pos, Clef clef, const KeyContext& key, const Instrument& inst, bool transposedScore)
{
    static constexpr int letterPc[7] = { 0, 2, 4, 5, 7, 9, 11 };
    const int step = pos + clefBottomStep (clef);
    const int letter = ((step % 7) + 7) % 7;
    const int octave = static_cast<int> (std::floor (step / 7.0));
    const int written = (octave + 1) * 12 + letterPc[letter] + key.signature.map[static_cast<size_t> (letter)];
    return std::clamp (written - inst.octave - (transposedScore ? inst.transposition : 0), 0, 127);
}

//==============================================================================
// Layout queries

double Layout::playheadX (Tick t) const
{
    for (size_t i = 0; i < measures.size(); ++i)
    {
        const auto& m = measures[i];
        const Tick endT = m.start + m.ticks;
        if (t >= endT) continue;
        const Tick lastT = m.columns.empty() ? m.start : m.columns.back().first;
        if (t <= lastT) break;
        // From the bar's last column straight to the next bar's first, or
        // to the final bar line.
        const double lastX = xForTick (lastT);
        const double nextX = i + 1 < measures.size() ? xForTick (endT) : m.x + m.width;
        return lastX + (nextX - lastX) * static_cast<double> (t - lastT) / static_cast<double> (std::max<Tick> (1, endT - lastT));
    }
    return xForTick (t);
}

double Layout::xForTick (Tick t) const
{
    if (measures.empty()) return 0;
    for (const auto& m : measures)
    {
        if (t >= m.start + m.ticks && &m != &measures.back()) continue;
        Tick prevT = m.start;
        double prevX = m.contentX;
        for (const auto& [ct, cx] : m.columns)
        {
            if (t <= ct)
            {
                if (ct == prevT) return cx;
                return prevX + (cx - prevX) * static_cast<double> (t - prevT) / static_cast<double> (ct - prevT);
            }
            prevT = ct;
            prevX = cx;
        }
        const Tick endT = m.start + m.ticks;
        const double endX = m.x + m.width - padRight;
        if (t >= endT) return m.x + m.width;
        return prevX + (endX - prevX) * static_cast<double> (t - prevT) / static_cast<double> (std::max<Tick> (1, endT - prevT));
    }
    return measures.back().x + measures.back().width;
}

Tick Layout::tickForX (double x) const
{
    if (measures.empty()) return 0;
    const int mi = measureAtX (x);
    const auto& m = measures[static_cast<size_t> (mi)];
    Tick prevT = m.start;
    double prevX = m.x;
    std::vector<std::pair<Tick, double>> pts (m.columns);
    pts.push_back ({ m.start + m.ticks, m.x + m.width });
    for (const auto& [ct, cx] : pts)
    {
        if (x <= cx)
        {
            if (cx <= prevX) return ct;
            return prevT + static_cast<Tick> (std::llround ((x - prevX) / (cx - prevX) * static_cast<double> (ct - prevT)));
        }
        prevT = ct;
        prevX = cx;
    }
    return m.start + m.ticks;
}

int Layout::measureAtX (double x) const
{
    for (size_t i = 0; i < measures.size(); ++i)
        if (x < measures[i].x + measures[i].width) return static_cast<int> (i);
    return measures.empty() ? 0 : static_cast<int> (measures.size()) - 1;
}

int Layout::staffAtY (double y) const
{
    int best = 0;
    double gap = 1e9;
    for (size_t i = 0; i < staves.size(); ++i)
    {
        const double centre = staves[i].top + 2.0;
        if (std::abs (y - centre) < gap) { gap = std::abs (y - centre); best = static_cast<int> (i); }
    }
    return best;
}

const Head* Layout::headAt (double x, double y, int* staffOut, int* elementOut) const
{
    const Head* found = nullptr;
    double bestDist = 1e9;
    for (size_t s = 0; s < staves.size(); ++s)
    {
        const auto& st = staves[s];
        for (size_t e = 0; e < st.elements.size(); ++e)
        {
            const auto& el = st.elements[e];
            if (el.rest) continue;
            const double w = el.den == 1 ? wholeHeadWidth : headWidth;
            for (const auto& h : el.heads)
            {
                const double hy = st.top + 4.0 - h.pos * 0.5;
                const double cx = h.x + w * 0.5;
                const double dx = std::abs (x - cx), dy = std::abs (y - hy);
                if (dx <= w * 0.5 + 0.25 && dy <= 0.55)
                {
                    const double d = dx + dy * 2;
                    if (d < bestDist)
                    {
                        bestDist = d;
                        found = &h;
                        if (staffOut) *staffOut = static_cast<int> (s);
                        if (elementOut) *elementOut = static_cast<int> (e);
                    }
                }
            }
        }
    }
    return found;
}

//==============================================================================
// The layout itself

namespace
{
struct VNote
{
    Tick start, end;          // as performed
    int pitch, written;
    int voice;
    uint32_t id;
};

struct Event
{
    Tick at, end;
    std::vector<VNote> notes;
};

// The grid one beat's worth of music sits on: the coarsest that puts every
// onset and end in the beat exactly, else the coarsest that is close.
Tick gridForBeat (const std::vector<Tick>& times, Tick beatStart, Tick beat)
{
    std::vector<Tick> candidates;
    if (beat % 3 == 0 && beat > PPQ)   // a compound beat divides in three
        candidates = { beat, beat / 3, beat / 6, beat / 2, beat / 12, beat / 4 };
    else
        candidates = { beat, beat / 2, beat / 4, beat / 3, beat / 6, beat / 8, beat / 12, beat / 16 };
    candidates.erase (std::remove_if (candidates.begin(), candidates.end(), [] (Tick g) { return g < PPQ / 16; }), candidates.end());

    auto worst = [&times, beatStart] (Tick g)
    {
        Tick w = 0;
        for (Tick t : times)
        {
            const Tick rel = t - beatStart;
            const Tick snapped = ((rel + g / 2) / g) * g;
            w = std::max (w, std::abs (rel - snapped));
        }
        return w;
    };
    for (Tick g : candidates) if (worst (g) <= 2) return g;
    for (Tick g : candidates) if (worst (g) <= g / 5) return g;
    return candidates.empty() ? PPQ / 8 : candidates.back();
}

struct BeatGrid
{
    const Score& score;
    std::vector<std::pair<Tick, Tick>> beats;   // start, grid
    Tick snap (Tick t) const
    {
        // The beat containing t; a time on a beat line is already exact.
        auto it = std::upper_bound (beats.begin(), beats.end(), t, [] (Tick v, const std::pair<Tick, Tick>& b) { return v < b.first; });
        if (it == beats.begin()) return t;
        --it;
        const Tick g = std::max<Tick> (1, it->second);
        const Tick rel = t - it->first;
        return it->first + ((rel + g / 2) / g) * g;
    }
};

std::vector<std::pair<Tick, Tick>> beatsOf (const Score& score)
{
    std::vector<std::pair<Tick, Tick>> b;
    for (int bar = 0; bar < score.bars + 1; ++bar)
    {
        const auto& m = score.meterAtBar (bar);
        const Tick start = score.barStart (bar), beat = m.beatTicks();
        for (Tick t = start; t < start + m.barTicks(); t += beat) b.push_back ({ t, beat });
    }
    return b;
}

// Splits one voice of one staff into chords, quantised. Notes struck together
// are one chord; a gated note is written to whatever starts next (sb 0005).
std::vector<Event> eventsFor (std::vector<VNote> notes, const std::vector<std::pair<Tick, Tick>>& beats, const Score& score,
                              bool drums)
{
    std::vector<Event> out;
    if (notes.empty()) return out;
    std::sort (notes.begin(), notes.end(), [] (const VNote& a, const VNote& b) { return a.start < b.start; });

    // The grid for each beat, from the times that fall in it.
    std::vector<std::pair<Tick, Tick>> grid;
    {
        size_t ni = 0;
        std::vector<Tick> ends;
        for (const auto& n : notes) ends.push_back (n.end);
        std::sort (ends.begin(), ends.end());
        size_t ei = 0;
        for (size_t bi = 0; bi < beats.size(); ++bi)
        {
            const Tick a = beats[bi].first, len = beats[bi].second, b = a + len;
            std::vector<Tick> times;
            while (ni < notes.size() && notes[ni].start < b) { if (notes[ni].start >= a) times.push_back (notes[ni].start); ++ni; }
            while (ei < ends.size() && ends[ei] < b) { if (ends[ei] > a) times.push_back (ends[ei]); ++ei; }
            grid.push_back ({ a, times.empty() ? len : gridForBeat (times, a, len) });
        }
    }
    BeatGrid bg { score, grid };

    for (auto& n : notes) n.start = bg.snap (n.start);
    std::stable_sort (notes.begin(), notes.end(), [] (const VNote& a, const VNote& b) { return a.start < b.start; });

    for (const auto& n : notes)
    {
        if (! out.empty() && out.back().at == n.start) out.back().notes.push_back (n);
        else out.push_back ({ n.start, n.end, { n } });
    }

    for (size_t i = 0; i < out.size(); ++i)
    {
        auto& e = out[i];
        Tick rawEnd = e.at;
        for (const auto& n : e.notes) rawEnd = std::max (rawEnd, n.end);
        const Tick len = std::max<Tick> (1, rawEnd - e.at);
        const Tick next = i + 1 < out.size() ? out[i + 1].at : -1;

        Tick end = rawEnd;
        if (drums)
        {
            // A drum does not sustain: written to the next hit, at most a beat.
            // The last hit is a beat long, up to the bar line.
            const int bar = score.barAt (e.at);
            const Tick beat = score.meterAtBar (bar).beatTicks();
            end = next >= 0 ? std::min (next, e.at + beat) : std::min (e.at + beat, score.barStart (bar + 1));
        }
        else if (next >= 0 && next > rawEnd && next - rawEnd <= std::max<Tick> (len / 4, PPQ / 16))
            end = next;                                   // gated short of the next note
        else if (next < 0 || next > rawEnd)
        {
            // Nothing straight after: round up to the beat if it is close.
            const Tick beat = score.meterAtBar (score.barAt (rawEnd)).beatTicks();
            const Tick barStart = score.barStart (score.barAt (rawEnd));
            const Tick up = barStart + ((rawEnd - barStart + beat - 1) / beat) * beat;
            if (up - rawEnd <= len / 4 && (next < 0 || up <= next)) end = up;
        }
        end = bg.snap (end);
        if (next >= 0) end = std::min (end, next);       // overlaps are cut at the next chord
        if (end <= e.at)
        {
            // Snapped away to nothing: give it one unit of its beat's grid.
            auto it = std::upper_bound (grid.begin(), grid.end(), e.at, [] (Tick v, const std::pair<Tick, Tick>& b) { return v < b.first; });
            const Tick g = it == grid.begin() ? PPQ / 4 : std::prev (it)->second;
            end = e.at + g;
            if (next >= 0) end = std::min (end, next);
        }
        e.end = end;
    }
    return out;
}

// A sustained low note under moving music goes to the lower voice, so the
// line above it can move without cutting it short. Only where the part never
// set voices itself.
void autoVoices (std::vector<VNote>& notes, int poly)
{
    if (poly <= 1) return;
    for (const auto& n : notes) if (n.voice != 0) return;
    std::sort (notes.begin(), notes.end(), [] (const VNote& a, const VNote& b) { return a.start != b.start ? a.start < b.start : a.pitch < b.pitch; });
    Tick lowerBusyUntil = 0;
    for (size_t i = 0; i < notes.size(); ++i)
    {
        auto& n = notes[i];
        const Tick len = n.end - n.start;
        // Something else starts while it sounds, well before it ends, and it
        // is the lowest note of its own chord.
        bool lowest = true, overlapped = false;
        // Sorted by start then pitch, so a lower note of the same chord is
        // the one just before, and the notes that start while this one
        // sounds follow it.
        if (i > 0 && notes[i - 1].start == n.start) lowest = false;
        for (size_t j = i + 1; j < notes.size() && notes[j].start <= n.end; ++j)
        {
            const auto& o = notes[j];
            if (o.start > n.start && o.start < n.end - std::max<Tick> (len / 4, PPQ / 4) && o.pitch > n.pitch) overlapped = true;
        }
        if (lowest && overlapped && n.start >= lowerBusyUntil)
        {
            n.voice = 1;
            lowerBusyUntil = n.end;
        }
    }
    // Anything struck with a moved note moves with it if it ends with it.
    for (auto& n : notes)
        if (n.voice == 0)
            for (const auto& o : notes)
                if (o.voice == 1 && o.start == n.start && o.end == n.end && o.pitch < n.pitch + 12) n.voice = 1;
}

// Below the middle line the stem turns up, above it down; a chord follows its
// head furthest from that line (Gehrkens, Sec. 2).
Stem wantStem (const Element& el)
{
    int far = 0;
    for (const auto& h : el.heads)
        if (std::abs (h.pos - 4) > std::abs (far)) far = h.pos - 4;
    if (far == 0)
    {
        // Equally far both ways: the majority of the heads decide.
        int balance = 0;
        for (const auto& h : el.heads) balance += h.pos - 4;
        return balance > 0 ? Stem::down : Stem::up;
    }
    return far > 0 ? Stem::down : Stem::up;
}

double accidentalWidth (int acc)
{
    switch (acc)
    {
        case -2: return 1.644;
        case -1: return 0.904;
        case 0:  return 0.672;
        case 1:  return 0.996;
        case 2:  return 0.988;
        default: return 0.9;
    }
}

// Which side of the stem each head sits, and where the accidentals go
// (Starting Blocks Notation's placeHeads). Returns how far left of the
// element's x the ink reaches, accidentals included.
double placeHeads (Element& el)
{
    if (el.rest || el.heads.empty()) return 0;
    const double w = el.den == 1 ? wholeHeadWidth : headWidth;
    const int sideDir = el.stem == Stem::down ? -1 : 1;

    bool flip = false;
    for (size_t i = 0; i < el.heads.size(); ++i)
    {
        flip = i > 0 && std::abs (el.heads[i].pos - el.heads[i - 1].pos) <= 1 && ! flip;
        el.heads[i].side = flip ? 1 : 0;
    }
    double left = 0;
    for (auto& h : el.heads)
    {
        h.x = el.x;
        if (h.side == 1) h.x += sideDir * (w - stemThickness);
        left = std::max (left, el.x - h.x);
    }

    // Accidentals in columns, topmost first so it sits nearest the chord.
    constexpr int clear = 6;
    std::vector<std::vector<int>> cols;
    std::vector<int> colOf (el.heads.size(), -1);
    for (int i = static_cast<int> (el.heads.size()) - 1; i >= 0; --i)
    {
        const auto& h = el.heads[static_cast<size_t> (i)];
        if (! h.showAccidental) continue;
        size_t c = 0;
        for (;; ++c)
        {
            if (c >= cols.size()) { cols.emplace_back(); break; }
            bool clash = false;
            for (int p : cols[c]) if (std::abs (p - h.pos) < clear) { clash = true; break; }
            if (! clash) break;
        }
        cols[c].push_back (h.pos);
        colOf[static_cast<size_t> (i)] = static_cast<int> (c);
    }
    double at = left + 0.25;
    std::vector<double> colRight (cols.size());
    for (size_t c = 0; c < cols.size(); ++c)
    {
        double cw = 0;
        for (size_t i = 0; i < el.heads.size(); ++i)
            if (colOf[i] == static_cast<int> (c)) cw = std::max (cw, accidentalWidth (el.heads[i].accidental));
        colRight[c] = at;
        at += cw + 0.15;
    }
    for (size_t i = 0; i < el.heads.size(); ++i)
        if (colOf[i] >= 0)
        {
            auto& h = el.heads[i];
            h.accidentalX = -(colRight[static_cast<size_t> (colOf[i])] + accidentalWidth (h.accidental));
        }
    return cols.empty() ? left : at;
}

double spaceFor (Tick ticks)
{
    // Not proportional - a page of that is mostly air - but growing by a
    // fixed step each time a duration doubles, which is how engravers space.
    const double ratio = static_cast<double> (std::max<Tick> (ticks, 1)) / (PPQ / 4.0);
    return std::max (1.7, 2.1 + 0.95 * std::log2 (ratio));
}

double headY (int pos) { return 4.0 - pos * 0.5; }   // relative to the staff top

double keySigWidth (const KeySignature& k) { return k.count * 1.0 + (k.count > 0 ? 0.6 : 0.0); }
} // namespace

Layout layout (const Score& score, const Options& options)
{
    Layout lay;
    const auto beats = beatsOf (score);

    // The staves.
    for (size_t pi = 0; pi < score.parts.size(); ++pi)
    {
        const auto& inst = instrumentById (score.parts[pi].instrument);
        for (size_t s = 0; s < inst.staves.size(); ++s)
        {
            Staff st;
            st.part = static_cast<int> (pi);
            st.staffInPart = static_cast<int> (s);
            st.staffCount = static_cast<int> (inst.staves.size());
            st.clef = inst.staves[s];
            lay.staves.push_back (st);
        }
    }

    // The measures, and what each one shows at its start.
    for (int bar = 0; bar < score.bars; ++bar)
    {
        Measure m;
        m.bar = bar;
        m.start = score.barStart (bar);
        m.meter = score.meterAtBar (bar);
        m.ticks = m.meter.barTicks();
        const auto& k = score.keyAtBar (bar);
        m.key = keyContext (k.root, k.scale).signature;
        if (bar > 0)
        {
            const auto& pk = score.keyAtBar (bar - 1);
            m.previousKey = keyContext (pk.root, pk.scale).signature;
            m.showKey = m.previousKey.fifths() != m.key.fifths();
            const auto& pm = score.meterAtBar (bar - 1);
            m.showTime = pm.num != m.meter.num || pm.den != m.meter.den;
        }
        else
        {
            m.showKey = m.key.count > 0;
            m.showTime = m.showClef = true;
        }
        lay.measures.push_back (m);
    }

    // The music of each staff, voice by voice.
    for (auto& st : lay.staves)
    {
        const auto& part = score.parts[static_cast<size_t> (st.part)];
        const auto& inst = instrumentById (part.instrument);
        const bool perc = st.clef == Clef::percussion;

        std::vector<VNote> mine;
        for (const auto& n : part.notes)
        {
            if (! perc && staffForPitch (inst, n.pitch) != st.staffInPart) continue;
            VNote v { n.start, n.end(), n.pitch, inst.writtenPitch (n.pitch, options.transposedScore), n.voice, n.id };
            if (perc) v.voice = drumNote (n.pitch).voice;
            mine.push_back (v);
        }
        if (! perc) autoVoices (mine, inst.poly);

        std::vector<Element> all;
        for (int voice = 0; voice < 2; ++voice)
        {
            std::vector<VNote> vn;
            for (const auto& n : mine) if (n.voice == voice) vn.push_back (n);
            const auto events = eventsFor (vn, beats, score, perc);

            for (const auto& e : events)
            {
                Tick at = e.at;
                Tick left = e.end - e.at;
                bool first = true;
                while (left > 0)
                {
                    const int bar = score.barAt (at);
                    if (bar >= score.bars) break;
                    const Tick barStart = score.barStart (bar);
                    const Tick barLen = score.barLength (bar);
                    const Tick chunk = std::min (left, barStart + barLen - at);
                    auto pieces = splitDuration (at - barStart, chunk, barLen, false, score.meterAtBar (bar).beatTicks());
                    Tick off = at;
                    for (size_t i = 0; i < pieces.size(); ++i)
                    {
                        Element el;
                        el.voice = voice;
                        el.at = off;
                        el.ticks = pieces[i].ticks;
                        el.den = pieces[i].den;
                        el.dots = pieces[i].dots;
                        el.tuplet = pieces[i].tuplet;
                        el.tieIn = ! (first && i == 0);
                        el.tieOut = ! (i + 1 == pieces.size() && left - chunk <= 0);
                        el.measure = bar;
                        for (const auto& n : e.notes)
                        {
                            Head h;
                            h.pitch = n.pitch;
                            h.written = n.written;
                            h.noteId = n.id;
                            h.outOfRange = ! perc && (n.pitch < inst.low || n.pitch > inst.high);
                            h.outsideSweet = ! perc && ! h.outOfRange && (n.pitch < inst.sweetLow || n.pitch > inst.sweetHigh);
                            if (perc)
                            {
                                const auto d = drumNote (n.pitch);
                                h.pos = d.pos;
                                h.drumCross = d.cross;
                            }
                            el.heads.push_back (h);
                        }
                        all.push_back (el);
                        off += pieces[i].ticks;
                    }
                    if (pieces.empty()) break;
                    first = false;
                    at += chunk;
                    left -= chunk;
                }
            }
        }

        // Rests in the gaps. A bar with nothing in the upper voice is one
        // whole-bar rest (Sec. 33); the lower voice only rests where it is
        // in use at all.
        std::vector<Element> rests;
        std::vector<std::array<std::vector<std::pair<Tick, Tick>>, 2>> spansByBar (lay.measures.size());
        for (const auto& el : all)
            if (el.measure >= 0 && el.measure < static_cast<int> (spansByBar.size()))
                spansByBar[static_cast<size_t> (el.measure)][static_cast<size_t> (el.voice)].push_back ({ el.at, el.at + el.ticks });
        for (const auto& m : lay.measures)
        {
            for (int voice = 0; voice < 2; ++voice)
            {
                auto spans = spansByBar[static_cast<size_t> (m.bar)][static_cast<size_t> (voice)];
                if (spans.empty())
                {
                    if (voice == 0)
                    {
                        Element r;
                        r.rest = r.measureRest = true;
                        r.at = m.start;
                        r.ticks = m.ticks;
                        r.den = 1;
                        r.measure = m.bar;
                        rests.push_back (r);
                    }
                    continue;
                }
                std::sort (spans.begin(), spans.end());
                Tick cursor = m.start;
                std::vector<std::pair<Tick, Tick>> gaps;
                for (const auto& [a, b] : spans)
                {
                    if (a > cursor) gaps.push_back ({ cursor, a });
                    cursor = std::max (cursor, b);
                }
                if (m.start + m.ticks > cursor) gaps.push_back ({ cursor, m.start + m.ticks });
                for (const auto& [a, b] : gaps)
                {
                    Tick off = a;
                    for (const auto& v : splitDuration (a - m.start, b - a, m.ticks, true, m.meter.beatTicks()))
                    {
                        Element r;
                        r.rest = true;
                        r.voice = voice;
                        r.at = off;
                        r.ticks = v.ticks;
                        r.den = v.den;
                        r.dots = v.dots;
                        r.tuplet = v.tuplet;
                        r.measure = m.bar;
                        rests.push_back (r);
                        off += v.ticks;
                    }
                }
            }
        }
        all.insert (all.end(), rests.begin(), rests.end());
        std::stable_sort (all.begin(), all.end(), [] (const Element& a, const Element& b)
        {
            if (a.at != b.at) return a.at < b.at;
            if (a.voice != b.voice) return a.voice < b.voice;
            return a.rest < b.rest;
        });

        // Spelling and accidentals, which last to the bar line (Sec. 24) and
        // carry across a tie (Sec. 25), remembered per staff degree.
        int currentBar = -1;
        std::map<int, int> sounding;
        KeyContext ctx;
        for (auto& el : all)
        {
            if (el.measure != currentBar)
            {
                currentBar = el.measure;
                sounding.clear();
                const auto& k = score.keyAtBar (currentBar);
                ctx = keyContext (k.root, k.scale);
            }
            if (el.rest || perc) { std::sort (el.heads.begin(), el.heads.end(), [] (const Head& a, const Head& b) { return a.pos < b.pos; }); continue; }
            for (auto& h : el.heads)
            {
                const auto s = spell (h.written, ctx);
                h.pos = s.step - clefBottomStep (st.clef);
                h.accidental = s.accidental;
                auto it = sounding.find (s.step);
                const int have = it != sounding.end() ? it->second : ctx.signature.map[static_cast<size_t> (s.letter)];
                if (! el.tieIn)
                {
                    h.showAccidental = s.accidental != have;
                    sounding[s.step] = s.accidental;
                }
            }
            std::sort (el.heads.begin(), el.heads.end(), [] (const Head& a, const Head& b) { return a.pos < b.pos; });
            // Two notes of one chord on the same degree with different
            // accidentals both need theirs shown.
            for (size_t i = 1; i < el.heads.size(); ++i)
                if (el.heads[i].pos == el.heads[i - 1].pos && el.heads[i].accidental != el.heads[i - 1].accidental)
                    el.heads[i].showAccidental = el.heads[i - 1].showAccidental = true;
        }

        // What the instrument cannot do: more notes than it has, faster than
        // it plays cleanly at this tempo, a wider leap than it takes in passing.
        const Element* prev = nullptr;
        for (auto& el : all)
        {
            if (el.rest || perc) { if (el.rest && el.voice == 0) prev = nullptr; continue; }
            if (el.voice != 0) continue;
            el.tooManyNotes = static_cast<int> (el.heads.size()) > inst.poly;
            if (prev != nullptr && ! el.tieIn)
            {
                const double gap = score.secondsAt (el.at) - score.secondsAt (prev->at);
                el.tooFast = gap < inst.fast * 0.999;
                if (inst.monophonic() && el.heads.size() == 1 && prev->heads.size() == 1)
                    el.tooWide = std::abs (el.heads[0].pitch - prev->heads[0].pitch) > inst.leap;
            }
            if (! el.tieIn) prev = &el;
        }

        st.elements = std::move (all);
    }

    //==========================================================================
    // Columns and spacing, shared by every staff so simultaneous notes line up.

    double x = 0;
    {
        const auto& m0 = lay.measures.empty() ? Measure {} : lay.measures.front();
        lay.headerWidth = 0.8 + 2.9 + 0.9 + keySigWidth (m0.key) + 2.2 + 1.0;
    }
    // Stem directions first: a crossed head's side depends on them, and the
    // room for accidentals on that.
    for (auto& st : lay.staves)
    {
        std::set<std::pair<int, int>> twoVoices;   // bar, which has voice 1 notes
        for (const auto& el : st.elements)
            if (el.voice == 1 && ! el.rest) twoVoices.insert ({ el.measure, 1 });
        for (auto& el : st.elements)
        {
            const bool split = twoVoices.count ({ el.measure, 1 }) != 0;
            if (el.rest)
            {
                el.restY = el.den == 1 ? 1.0 : 2.0;
                if (split) el.restY += el.voice == 0 ? -2.0 : 2.0;
                continue;
            }
            if (el.den <= 1) { el.stem = Stem::none; continue; }
            el.stem = split ? (el.voice == 0 ? Stem::up : Stem::down) : wantStem (el);
            el.beamCount = hooks (el.den);
        }
    }

    // Beam groups decide the final stem direction of their members, so they
    // are found before spacing: a group follows its majority (Sec. 4).
    for (auto& st : lay.staves)
    {
        std::set<int> splitBars;
        for (const auto& el : st.elements) if (el.voice == 1 && ! el.rest) splitBars.insert (el.measure);
        for (int voice = 0; voice < 2; ++voice)
        {
            std::vector<int> run;
            Tick runGroup = -1, runEnd = -1;
            auto close = [&]
            {
                if (run.size() >= 2)
                {
                    Beam b;
                    b.elements = run;
                    int balance = 0;
                    for (int i : run) balance += wantStem (st.elements[static_cast<size_t> (i)]) == Stem::up ? 1 : -1;
                    const auto& firstEl = st.elements[static_cast<size_t> (run.front())];
                    if (splitBars.count (firstEl.measure) != 0) b.stem = voice == 0 ? Stem::up : Stem::down;
                    else b.stem = balance >= 0 ? Stem::up : Stem::down;
                    const int index = static_cast<int> (st.beams.size());
                    for (int i : run)
                    {
                        auto& el = st.elements[static_cast<size_t> (i)];
                        el.beam = index;
                        el.stem = b.stem;
                    }
                    st.beams.push_back (b);
                }
                run.clear();
            };
            for (size_t i = 0; i < st.elements.size(); ++i)
            {
                const auto& el = st.elements[i];
                if (el.voice != voice) continue;
                if (el.rest || el.beamCount == 0) { close(); runGroup = -1; continue; }
                const auto& m = lay.measures[static_cast<size_t> (el.measure)];
                const Tick group = m.meter.beatTicks();
                const Tick g = (el.at - m.start) / group + static_cast<Tick> (el.measure) * 10000;
                if (! run.empty() && (g != runGroup || el.at != runEnd)) close();
                run.push_back (static_cast<int> (i));
                runGroup = g;
                runEnd = el.at + el.ticks;
            }
            close();
        }
    }

    // Every element, by the measure it is in, so spacing a measure touches
    // only its own.
    std::vector<std::vector<Element*>> byMeasure (lay.measures.size());
    for (auto& st : lay.staves)
        for (auto& el : st.elements)
            if (el.measure >= 0 && el.measure < static_cast<int> (byMeasure.size()))
                byMeasure[static_cast<size_t> (el.measure)].push_back (&el);

    for (auto& m : lay.measures)
    {
        auto& mine = byMeasure[static_cast<size_t> (m.bar)];
        m.x = x;
        double head = 0;
        if (m.bar == 0) head = lay.headerWidth;
        else
        {
            if (m.showKey) head += std::max (keySigWidth (m.key), 1.0) + 0.8;
            if (m.showTime) head += 3.0;
        }
        m.contentX = m.x + head + padLeft;

        std::vector<Tick> onsets;
        for (const auto* el : mine)
            if (! el->measureRest) onsets.push_back (el->at);
        std::sort (onsets.begin(), onsets.end());
        onsets.erase (std::unique (onsets.begin(), onsets.end()), onsets.end());

        double cx = m.contentX;
        for (size_t i = 0; i < onsets.size(); ++i)
        {
            const Tick o = onsets[i];
            // Room in front for accidentals and heads crossed to the left;
            // behind for dots and heads crossed to the right.
            double before = 0, after = 0;
            for (auto* elp : mine)
                {
                    auto& el = *elp;
                    if (el.at != o || el.measureRest) continue;
                    el.x = 0;
                    const double reach = placeHeads (el);
                    before = std::max (before, reach);
                    double right = (el.den == 1 ? wholeHeadWidth : headWidth);
                    for (const auto& h : el.heads) if (h.side == 1 && el.stem != Stem::down) right = 2 * headWidth;
                    if (el.dots > 0) right += 0.4 + 0.6 * el.dots;
                    if (el.stem == Stem::up && el.beam < 0 && el.beamCount > 0) right += 0.9;   // the flag
                    after = std::max (after, right);
                }
            if (i > 0) cx += before;
            else cx = std::max (cx, m.contentX + before - 0.6);
            m.columns.push_back ({ o, cx });
            const Tick next = i + 1 < onsets.size() ? onsets[i + 1] : m.start + m.ticks;
            cx += std::max (spaceFor (next - o), after + 0.5);
        }
        m.width = std::max (cx + padRight - m.x, head + padLeft + 6.0);

        for (auto* elp : mine)
            {
                auto& el = *elp;
                if (el.measureRest) { el.x = m.contentX + (m.x + m.width - m.contentX - 1.128) * 0.5 - 0.3; continue; }
                for (const auto& [t, colX] : m.columns)
                    if (t == el.at) { el.x = colX; break; }
                placeHeads (el);
            }
        x += m.width;
    }
    lay.width = x;

    //==========================================================================
    // Stems, beams, tuplets and ties - all y relative to the staff's top line.

    for (auto& st : lay.staves)
    {
        auto stemFor = [] (Element& el)
        {
            if (el.rest || el.stem == Stem::none || el.heads.empty()) return;
            const double low = headY (el.heads.front().pos);    // furthest down
            const double high = headY (el.heads.back().pos);
            const double extra = std::max (0, el.beamCount - 2) * 0.75;
            if (el.stem == Stem::up)
            {
                el.stemX = el.x + headWidth - stemThickness * 0.5;
                el.stemBottom = low - 0.168;
                // A note far below the staff still reaches the middle line.
                el.stemTop = std::min (high - 3.5 - extra, 2.0);
            }
            else
            {
                el.stemX = el.x + stemThickness * 0.5;
                el.stemTop = high + 0.168;
                el.stemBottom = std::max (low + 3.5 + extra, 2.0);
            }
        };
        for (auto& el : st.elements) stemFor (el);

        for (auto& b : st.beams)
        {
            auto& first = st.elements[static_cast<size_t> (b.elements.front())];
            auto& last = st.elements[static_cast<size_t> (b.elements.back())];
            const bool up = b.stem == Stem::up;
            auto ref = [up] (const Element& el) { return up ? headY (el.heads.back().pos) : headY (el.heads.front().pos); };
            b.x1 = first.stemX;
            b.x2 = last.stemX;

            // A quarter of the interval, at most a space and a bit, and flat
            // when the middle of the group goes further than both ends.
            double slope = (ref (last) - ref (first)) * 0.25;
            slope = std::clamp (slope, -1.25, 1.25);
            bool concave = false;
            for (size_t k = 1; k + 1 < b.elements.size(); ++k)
            {
                const double r = ref (st.elements[static_cast<size_t> (b.elements[k])]);
                if (up ? (r < ref (first) && r < ref (last)) : (r > ref (first) && r > ref (last))) concave = true;
            }
            if (concave || b.x2 <= b.x1) slope = 0;

            int maxBeams = 1;
            for (int i : b.elements) maxBeams = std::max (maxBeams, st.elements[static_cast<size_t> (i)].beamCount);
            const double need = 3.25 + (maxBeams - 1) * 0.5;

            double offset = up ? 1e9 : -1e9;
            for (int i : b.elements)
            {
                const auto& el = st.elements[static_cast<size_t> (i)];
                const double along = b.x2 > b.x1 ? (el.stemX - b.x1) / (b.x2 - b.x1) * slope : 0;
                const double want = up ? ref (el) - need - along : ref (el) + need - along;
                offset = up ? std::min (offset, want) : std::max (offset, want);
            }
            // Notes far outside the staff still bring the beam to the middle line.
            if (up && offset + std::max (0.0, slope) > 2.0) offset = 2.0 - std::max (0.0, slope);
            if (! up && offset + std::min (0.0, slope) < 2.0) offset = 2.0 - std::min (0.0, slope);
            b.y1 = offset;
            b.y2 = offset + slope;

            for (int i : b.elements)
            {
                auto& el = st.elements[static_cast<size_t> (i)];
                if (up) el.stemTop = b.yAt (el.stemX);
                else el.stemBottom = b.yAt (el.stemX);
            }

            // Secondary beams, level by level: runs of members carrying that
            // many, and a stub for a member alone at its level.
            for (int level = 1; level < maxBeams; ++level)
            {
                size_t k = 0;
                while (k < b.elements.size())
                {
                    if (st.elements[static_cast<size_t> (b.elements[k])].beamCount <= level) { ++k; continue; }
                    size_t j = k;
                    while (j + 1 < b.elements.size() && st.elements[static_cast<size_t> (b.elements[j + 1])].beamCount > level) ++j;
                    const double xa = st.elements[static_cast<size_t> (b.elements[k])].stemX;
                    const double xb = st.elements[static_cast<size_t> (b.elements[j])].stemX;
                    if (j > k) b.segments.push_back ({ level, xa, xb });
                    else
                    {
                        // A stub points into the group: right from the first
                        // member, left from anything else.
                        const bool right = k == 0;
                        b.segments.push_back ({ level, right ? xa : xa - 1.1, right ? xa + 1.1 : xa });
                    }
                    k = j + 1;
                }
            }
        }

        // Tuplets: consecutive triplet values of one voice, grouped by the
        // span three of the shortest of them fills.
        for (int voice = 0; voice < 2; ++voice)
        {
            std::vector<int> run;
            auto flush = [&]
            {
                size_t k = 0;
                while (k < run.size())
                {
                    Tick shortest = st.elements[static_cast<size_t> (run[k])].ticks;
                    for (size_t j = k; j < run.size() && j < k + 6; ++j) shortest = std::min (shortest, st.elements[static_cast<size_t> (run[j])].ticks);
                    const Tick span = shortest * 3;
                    Tick sum = 0;
                    size_t j = k;
                    std::vector<int> group;
                    while (j < run.size() && sum < span)
                    {
                        sum += st.elements[static_cast<size_t> (run[j])].ticks;
                        group.push_back (run[j]);
                        ++j;
                    }
                    Tuplet t;
                    t.elements = group;
                    const auto& a = st.elements[static_cast<size_t> (group.front())];
                    const auto& z = st.elements[static_cast<size_t> (group.back())];
                    const int beam = a.beam;
                    bool oneBeam = beam >= 0;
                    for (int gi : group) if (st.elements[static_cast<size_t> (gi)].beam != beam) oneBeam = false;
                    if (oneBeam && static_cast<int> (st.beams[static_cast<size_t> (beam)].elements.size()) != static_cast<int> (group.size())) oneBeam = false;
                    t.bracket = ! oneBeam;
                    int ups = 0;
                    for (int gi : group) { const auto& e = st.elements[static_cast<size_t> (gi)]; ups += (e.rest || e.stem != Stem::down) ? 1 : -1; }
                    t.above = voice == 0 ? ups >= 0 : false;
                    t.x1 = a.x - 0.2;
                    t.x2 = z.x + headWidth + 0.2;
                    double y = t.above ? 0.0 : 4.0;
                    for (int gi : group)
                    {
                        const auto& e = st.elements[static_cast<size_t> (gi)];
                        if (e.rest) continue;
                        if (t.above) y = std::min ({ y, e.stem == Stem::up ? e.stemTop : headY (e.heads.back().pos) - 0.5 });
                        else y = std::max ({ y, e.stem == Stem::down ? e.stemBottom : headY (e.heads.front().pos) + 0.5 });
                    }
                    t.y = t.above ? y - 1.0 : y + 1.0;
                    st.tuplets.push_back (t);
                    k = j;
                }
                run.clear();
            };
            for (size_t i = 0; i < st.elements.size(); ++i)
            {
                const auto& el = st.elements[i];
                if (el.voice != voice) continue;
                if (el.tuplet == 3 && ! el.measureRest) run.push_back (static_cast<int> (i));
                else flush();
            }
            flush();
        }

        // Ties: each tied head to the same pitch in the voice's next element.
        for (int voice = 0; voice < 2; ++voice)
        {
            const Element* last = nullptr;
            for (const auto& el : st.elements)
            {
                if (el.voice != voice) continue;
                if (last != nullptr && last->tieOut && ! el.rest)
                {
                    for (size_t hi = 0; hi < last->heads.size(); ++hi)
                    {
                        const auto& h = last->heads[hi];
                        for (const auto& h2 : el.heads)
                        {
                            if (h2.pitch != h.pitch) continue;
                            Tie t;
                            const double w = last->den == 1 ? wholeHeadWidth : headWidth;
                            t.x1 = h.x + w + 0.15;
                            t.x2 = h2.x - 0.15;
                            t.y = headY (h.pos);
                            // Away from the stem; a chord splits its ties
                            // about its middle.
                            if (last->heads.size() > 1) t.above = hi >= last->heads.size() / 2;
                            else t.above = last->stem == Stem::down || (last->stem == Stem::none && h.pos > 4);
                            st.ties.push_back (t);
                        }
                    }
                }
                last = &el;
            }
        }

        // How far above and below the staff this one's ink reaches.
        int lo = 0, hi = 8;
        for (const auto& el : st.elements)
        {
            for (const auto& h : el.heads) { lo = std::min (lo, h.pos - 1); hi = std::max (hi, h.pos + 1); }
            if (! el.rest && el.stem != Stem::none)
            {
                hi = std::max (hi, static_cast<int> (std::ceil ((4.0 - el.stemTop) * 2)));
                lo = std::min (lo, static_cast<int> (std::floor ((4.0 - el.stemBottom) * 2)));
            }
        }
        for (const auto& t : st.tuplets)
        {
            hi = std::max (hi, static_cast<int> (std::ceil ((4.0 - t.y + 1.0) * 2)));
            lo = std::min (lo, static_cast<int> (std::floor ((4.0 - t.y - 1.0) * 2)));
        }
        st.lowestPos = lo;
        st.highestPos = hi;
    }

    // Stack the staves, far enough apart that one's ledger lines and stems
    // never reach the next.
    double y = 0;
    for (size_t i = 0; i < lay.staves.size(); ++i)
    {
        auto& st = lay.staves[i];
        const double above = std::max (0.0, (st.highestPos - 8) * 0.5);
        if (i == 0) y = above + 1.0;
        else
        {
            const auto& prev = lay.staves[i - 1];
            const double below = std::max (0.0, -prev.lowestPos * 0.5);
            const double base = (st.part == prev.part) ? options.grandGap : options.partGap;
            y = prev.top + 4.0 + std::max ({ base, below + above + 1.5, options.minStaffGap });
        }
        st.top = y;
    }
    if (! lay.staves.empty())
        lay.height = lay.staves.back().top + 4.0 + std::max (2.0, -lay.staves.back().lowestPos * 0.5 + 1.0);
    return lay;
}

} // namespace nt::engrave
