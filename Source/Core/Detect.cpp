#include "Detect.h"

#include "Instruments.h"
#include "ScaleModel.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>

namespace nt
{

std::vector<TimedPitch> collectPitches (const Score& score, Tick from, Tick to,
                                        const std::vector<uint32_t>& onlyParts)
{
    std::vector<TimedPitch> out;
    for (const auto& part : score.parts)
    {
        if (! onlyParts.empty() && std::find (onlyParts.begin(), onlyParts.end(), part.id) == onlyParts.end())
            continue;
        if (instrumentById (part.instrument).drums) continue;
        for (const auto& n : part.notes)
            if (n.end() > from && n.start < to)
                out.push_back ({ n.pitch, n.start, n.length });
    }
    std::sort (out.begin(), out.end(), [] (const TimedPitch& a, const TimedPitch& b)
    {
        if (a.start != b.start) return a.start < b.start;
        return a.pitch < b.pitch;
    });
    return out;
}

//==============================================================================
// Keys - Midi Suggester's ms_theory.detectKey, ported with its weights.

namespace
{
constexpr std::array<double, 12> majorProfile { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 };
constexpr std::array<double, 12> minorProfile { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };

// How a key on each pitch class is conventionally written.
const std::array<const char*, 12> majorRoot { "C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
const std::array<const char*, 12> minorRoot { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "G#", "A", "Bb", "B" };

constexpr double keyOutside = 1.0;
constexpr double keyTriad = 0.6;
constexpr double keyEndBonus = 0.2;
constexpr double keyStartBonus = 0.05;
// Noterator's own: the written key settles a near tie (detectKeys).
constexpr double signatureBonus = 0.12;

int rootIndexOf (const char* name)
{
    for (size_t i = 0; i < scaleview::roots.size(); ++i)
        if (std::string (scaleview::roots[i].name) == name) return static_cast<int> (i);
    return 0;
}

double correlate (const std::array<double, 12>& xs, const std::array<double, 12>& ys)
{
    double mx = 0, my = 0;
    for (size_t i = 0; i < 12; ++i) { mx += xs[i]; my += ys[i]; }
    mx /= 12; my /= 12;
    double sxy = 0, sxx = 0, syy = 0;
    for (size_t i = 0; i < 12; ++i)
    {
        const double dx = xs[i] - mx, dy = ys[i] - my;
        sxy += dx * dy; sxx += dx * dx; syy += dy * dy;
    }
    if (sxx == 0 || syy == 0) return 0;
    return sxy / std::sqrt (sxx * syy);
}

size_t pcIndex (int pc) { return static_cast<size_t> (((pc % 12) + 12) % 12); }
} // namespace

std::vector<KeyCandidate> rankKeys (const std::vector<TimedPitch>& notes, int firstPc, int lastPc)
{
    std::array<double, 12> hist {};
    double total = 0;
    for (const auto& n : notes)
    {
        // A very short note still counts for something: a run of sixteenths
        // is as much a statement of the key as one long note.
        const double w = std::max (static_cast<double> (n.length) / PPQ, 0.25);
        hist[pcIndex (n.pitch)] += w;
        total += w;
    }
    if (total <= 0) total = 1;
    auto at = [&hist] (int pc) { return hist[pcIndex (pc)]; };

    std::vector<KeyCandidate> ranked;
    for (int tonic = 0; tonic < 12; ++tonic)
    {
        for (int mode = 0; mode < 2; ++mode)
        {
            const auto& profile = mode == 0 ? majorProfile : minorProfile;
            std::array<double, 12> rotated {};
            for (int i = 0; i < 12; ++i) rotated[static_cast<size_t> (i)] = profile[pcIndex (i - tonic)];
            double score = correlate (hist, rotated);

            std::array<bool, 12> inScale {};
            for (int iv : scaleview::scales[static_cast<size_t> (mode)].intervals) inScale[pcIndex (tonic + iv)] = true;
            if (mode == 1) inScale[pcIndex (tonic + 11)] = true;   // the harmonic minor's leading tone
            double outside = 0;
            for (int pc = 0; pc < 12; ++pc) if (! inScale[static_cast<size_t> (pc)]) outside += at (pc);
            const int third = mode == 0 ? 4 : 3;
            const double triad = at (tonic) + at (tonic + third) + at (tonic + 7);
            score = score - keyOutside * outside / total + keyTriad * triad / total;
            if (lastPc == tonic) score += keyEndBonus;
            if (firstPc == tonic) score += keyStartBonus;

            KeyCandidate c;
            c.root = rootIndexOf ((mode == 0 ? majorRoot : minorRoot)[static_cast<size_t> (tonic)]);
            c.scale = mode;
            c.score = score;
            ranked.push_back (c);
        }
    }
    std::stable_sort (ranked.begin(), ranked.end(), [] (const KeyCandidate& a, const KeyCandidate& b)
    {
        if (a.score != b.score) return a.score > b.score;
        if (a.root != b.root) return a.root < b.root;
        return a.scale < b.scale;
    });
    return ranked;
}

namespace
{
// The lowest note of the first and the last moments: what a piece starts and
// ends on in the bass, which is how a musician settles relative keys.
std::pair<int, int> firstAndLastBass (const std::vector<TimedPitch>& notes)
{
    if (notes.empty()) return { -1, -1 };
    const Tick first = notes.front().start;
    Tick last = 0;
    for (const auto& n : notes) last = std::max (last, n.start);
    int lo = 200, loLast = 200;
    for (const auto& n : notes)
    {
        if (n.start == first) lo = std::min (lo, n.pitch);
        if (n.start == last) loLast = std::min (loLast, n.pitch);
    }
    return { lo % 12, loLast % 12 };
}

std::string labelFor (int root, int scale)
{
    return std::string (scaleview::roots[static_cast<size_t> (root)].name) + " "
         + (scale == 0 ? "major" : "minor");
}
} // namespace

std::vector<KeySpan> detectKeys (const Score& score, bool leanOnSignature)
{
    std::vector<KeySpan> spans;
    const auto all = collectPitches (score, 0, score.endTick());
    if (all.empty()) return spans;

    const auto [firstPc, lastPc] = firstAndLastBass (all);
    auto ranking = rankKeys (all, firstPc, lastPc);
    if (leanOnSignature)
    {
        // The signature's own key, read as major or minor the way ranking
        // names them.
        const auto& k = score.keys.front();
        const int sigPc = scaleview::roots[static_cast<size_t> (k.root)].pitchClass();
        const std::string scaleName = scaleview::scales[static_cast<size_t> (k.scale)].name;
        const int mode = (scaleName.find ("Minor") != std::string::npos || scaleName == "Aeolian") ? 1 : 0;
        for (auto& c : ranking)
            if (c.scale == mode && scaleview::roots[static_cast<size_t> (c.root)].pitchClass() == sigPc) c.score += signatureBonus;
        std::stable_sort (ranking.begin(), ranking.end(), [] (const KeyCandidate& a, const KeyCandidate& b) { return a.score > b.score; });
    }
    const auto global = ranking.front();

    // Bar by bar, a window of eight bars around it. A bar goes to another key
    // only when that key fits its window clearly better than the piece's own
    // key does there.
    const int bars = score.bars;
    std::vector<KeyCandidate> perBar (static_cast<size_t> (bars), global);
    if (bars > 8)
    {
        for (int b = 0; b < bars; ++b)
        {
            const int from = std::max (0, b - 3), to = std::min (bars, b + 5);
            const auto window = collectPitches (score, score.barStart (from), score.barStart (to));
            if (window.size() < 6) continue;
            const auto ranked = rankKeys (window, -1, -1);
            double globalHere = 0;
            for (const auto& c : ranked)
                if (c.root == global.root && c.scale == global.scale) globalHere = c.score;
            if (ranked.front().score - globalHere > 0.15)
                perBar[static_cast<size_t> (b)] = ranked.front();
        }
        // A change has to last four bars or it was colour, not a new key.
        size_t i = 0;
        while (i < perBar.size())
        {
            size_t j = i;
            while (j < perBar.size() && perBar[j].root == perBar[i].root && perBar[j].scale == perBar[i].scale) ++j;
            if (j - i < 4 && ! (perBar[i].root == global.root && perBar[i].scale == global.scale))
                for (size_t k = i; k < j; ++k) perBar[k] = global;
            i = j;
        }
    }

    for (int b = 0; b < bars; ++b)
    {
        const auto& c = perBar[static_cast<size_t> (b)];
        if (! spans.empty() && spans.back().root == c.root && spans.back().scale == c.scale)
        {
            spans.back().end = score.barStart (b + 1);
            continue;
        }
        KeySpan s;
        s.start = score.barStart (b);
        s.end = score.barStart (b + 1);
        s.root = c.root;
        s.scale = c.scale;
        s.label = labelFor (c.root, c.scale);
        spans.push_back (s);
    }
    return spans;
}

//==============================================================================
// Chords

namespace
{
struct Template { int root; int quality; };

// Interval sets the harmony is matched against. Order matters for ties: the
// plainer chord wins.
const std::vector<std::vector<int>> qualities {
    { 0, 4, 7 }, { 0, 3, 7 }, { 0, 4, 7, 10 }, { 0, 3, 7, 10 }, { 0, 4, 7, 11 },
    { 0, 3, 6 }, { 0, 3, 6, 10 }, { 0, 3, 6, 9 }, { 0, 4, 8 }, { 0, 5, 7 }, { 0, 2, 7 },
};

struct Weights
{
    std::array<double, 12> w {};
    int bass = -1;          // lowest pitch sounding
    double total = 0;
};

Weights weigh (const std::vector<TimedPitch>& notes, Tick from, Tick to)
{
    Weights out;
    int lowest = 200;
    for (const auto& n : notes)
    {
        const Tick a = std::max (from, n.start), b = std::min (to, n.start + n.length);
        if (b <= a) continue;
        const double w = static_cast<double> (b - a) / static_cast<double> (to - from);
        out.w[pcIndex (n.pitch)] += w;
        out.total += w;
        if (n.pitch < lowest) lowest = n.pitch;
    }
    out.bass = lowest < 200 ? lowest : -1;
    return out;
}

// How well a chord explains the weights: what it covers, less what it leaves
// out, less a little for each of its own notes that is missing.
double fit (const Weights& w, Template t, double* covered = nullptr)
{
    double in = 0, out = 0;
    std::array<bool, 12> member {};
    for (int iv : qualities[static_cast<size_t> (t.quality)]) member[pcIndex (t.root + iv)] = true;
    int missing = 0;
    for (size_t pc = 0; pc < 12; ++pc)
    {
        if (member[pc]) { in += w.w[pc]; if (w.w[pc] <= 0) ++missing; }
        else out += w.w[pc];
    }
    if (covered) *covered = in;
    double score = in - out - 0.35 * missing;
    if (w.bass >= 0 && pcIndex (w.bass) == pcIndex (t.root)) score += 0.15;
    return score;
}

Template best (const Weights& w)
{
    Template winner { 0, 0 };
    double top = -1e9;
    for (int q = 0; q < static_cast<int> (qualities.size()); ++q)
        for (int r = 0; r < 12; ++r)
        {
            const double s = fit (w, { r, q });
            if (s > top + 1e-9) { top = s; winner = { r, q }; }
        }
    return winner;
}
} // namespace

std::string nameSounding (const std::vector<int>& pitches, int keyRoot, int keyScale)
{
    if (pitches.empty()) return {};
    const auto key = scaleview::buildKey (keyRoot, keyScale);
    std::vector<int> held (pitches);
    std::sort (held.begin(), held.end());
    held.erase (std::unique (held.begin(), held.end()), held.end());
    return scaleview::chordName (held, key);
}

std::string nameFromRoot (const std::vector<int>& pitches, int root, int keyRoot, int keyScale)
{
    if (pitches.empty()) return {};
    std::array<bool, 12> classes {};
    std::array<int, 12> voices {};
    for (int p : pitches) { classes[pcIndex (p)] = true; ++voices[pcIndex (p)]; }
    const size_t r = pcIndex (root);
    if (! classes[r]) return nameSounding (pitches, keyRoot, keyScale);
    const auto key = scaleview::buildKey (keyRoot, keyScale);
    const int bass = static_cast<int> (pcIndex (*std::min_element (pitches.begin(), pitches.end())));
    std::array<bool, 12> has {};
    int count = 0;
    for (size_t pc = 0; pc < 12; ++pc)
        if (classes[pc]) { has[(pc + 12 - r) % 12] = true; ++count; }
    std::string quality;
    if (count == 1) return scaleview::chordNoteName (static_cast<int> (r), key);
    // Two notes, as ScaleView names them: a fifth, or a third with its fifth
    // said to be missing; any other interval as ScaleView reads it from there.
    if (count == 2 && has[7]) quality = "5";
    else if (count == 2 && has[4]) quality = "maj(no5)";
    else if (count == 2 && has[3]) quality = "min(no5)";
    else quality = scaleview::analyse (has, static_cast<int> (r), bass).name;
    std::string name = scaleview::chordNoteName (static_cast<int> (r), key) + quality;
    if (! scaleview::readAsRootPosition (static_cast<int> (r), bass, voices, key))
        name += "/" + scaleview::chordNoteName (bass, key);
    return name;
}

std::vector<ChordSpan> detectChords (const Score& score, Tick from, Tick to,
                                     const std::vector<uint32_t>& onlyParts)
{
    std::vector<ChordSpan> out;
    from = std::max<Tick> (0, from);
    to = std::min (to, score.endTick());
    if (to <= from) return out;
    const auto notes = collectPitches (score, from, to, onlyParts);
    if (notes.empty()) return out;

    // The beats, as [start, end) spans.
    std::vector<std::pair<Tick, Tick>> beats;
    for (int bar = score.barAt (from); bar < score.bars && score.barStart (bar) < to; ++bar)
    {
        const auto& m = score.meterAtBar (bar);
        const Tick start = score.barStart (bar), len = m.barTicks(), beat = m.beatTicks();
        for (Tick t = start; t < start + len; t += beat)
            if (t + beat > from && t < to) beats.emplace_back (std::max (t, from), std::min (t + beat, to));
    }

    // Each beat starts a segment; a beat joins the segment before it while
    // the best chord for the two together is still the one the segment had.
    struct Seg { Tick a, b; Template t; bool empty; };
    std::vector<Seg> segs;
    for (const auto& [a, b] : beats)
    {
        const auto w = weigh (notes, a, b);
        if (w.total <= 0)
        {
            if (! segs.empty() && ! segs.back().empty) segs.back().b = b;   // a rest holds the chord
            else segs.push_back ({ a, b, { 0, 0 }, true });
            continue;
        }
        const auto t = best (w);
        if (! segs.empty() && ! segs.back().empty)
        {
            auto& s = segs.back();
            const auto joined = best (weigh (notes, s.a, b));
            if (joined.root == s.t.root && joined.quality == s.t.quality
                && (t.root == s.t.root || fit (w, s.t) >= fit (w, t) - 0.25))
            {
                s.b = b;
                continue;
            }
        }
        segs.push_back ({ a, b, t, false });
    }

    for (const auto& s : segs)
    {
        if (s.empty) continue;
        const auto w = weigh (notes, s.a, s.b);
        // The notes named: every pitch class that sounds through at least a
        // fifth of the segment, plus anything belonging to the chord itself.
        std::array<bool, 12> member {};
        for (int iv : qualities[static_cast<size_t> (s.t.quality)]) member[pcIndex (s.t.root + iv)] = true;
        std::map<int, int> lowestOf;   // pitch class -> lowest pitch
        for (const auto& n : notes)
        {
            if (n.start + n.length <= s.a || n.start >= s.b) continue;
            const size_t pc = pcIndex (n.pitch);
            if (! member[pc] && w.w[pc] < 0.2) continue;
            auto it = lowestOf.find (static_cast<int> (pc));
            if (it == lowestOf.end() || n.pitch < it->second) lowestOf[static_cast<int> (pc)] = n.pitch;
        }
        // A single line is not harmony: two notes one after the other in a
        // tune are a step, not a chord. A segment is named where notes sound
        // together, or where it outlines three pitch classes (an arpeggio).
        int together = 0, most = 0;
        {
            std::vector<std::pair<Tick, int>> edges;
            for (const auto& n : notes)
            {
                const Tick a = std::max (s.a, n.start), b = std::min (s.b, n.start + n.length);
                if (b > a) { edges.push_back ({ a, 1 }); edges.push_back ({ b, -1 }); }
            }
            std::sort (edges.begin(), edges.end(), [] (const auto& x, const auto& y) { return x.first != y.first ? x.first < y.first : x.second < y.second; });
            for (const auto& [t, d] : edges) { together += d; most = std::max (most, together); }
        }
        ChordSpan span;
        span.start = s.a;
        span.end = s.b;
        for (const auto& [pc, p] : lowestOf) span.pitches.push_back (p);
        // The bass is the lowest note sounding at all, kept even when it
        // passes, so an inversion reads as one.
        if (w.bass >= 0 && std::find (span.pitches.begin(), span.pitches.end(), w.bass) == span.pitches.end())
        {
            const int bassPc = w.bass % 12;
            span.pitches.erase (std::remove_if (span.pitches.begin(), span.pitches.end(),
                                                [bassPc] (int p) { return p % 12 == bassPc; }),
                                span.pitches.end());
            span.pitches.push_back (w.bass);
        }
        std::sort (span.pitches.begin(), span.pitches.end());
        const auto& k = score.keyAtBar (score.barAt (s.a));
        bool outlined = span.pitches.size() >= 3;
        for (int p : span.pitches) outlined = outlined && member[pcIndex (p)];
        const bool harmony = most >= 2 || outlined;
        span.name = (harmony && span.pitches.size() >= 2) ? nameSounding (span.pitches, k.root, k.scale) : std::string {};
        if (! out.empty() && out.back().name == span.name && out.back().end == span.start)
        {
            out.back().end = span.end;
            continue;
        }
        out.push_back (span);
    }

    // A chord made on a known root (decision 0046) is named from it, for as
    // long as the notes sounding there are still exactly its notes.
    for (const auto& cr : score.chordRoots)
    {
        const Tick a = std::max (cr.start, from), b = std::min (cr.end, to);
        if (b <= a) continue;
        std::map<int, int> lowestOf;
        for (const auto& n : notes)
        {
            if (n.start + n.length <= a || n.start >= b) continue;
            const int pc = static_cast<int> (pcIndex (n.pitch));
            auto it = lowestOf.find (pc);
            if (it == lowestOf.end() || n.pitch < it->second) lowestOf[pc] = n.pitch;
        }
        std::vector<int> pcs;
        for (const auto& [pc, p] : lowestOf) pcs.push_back (pc);
        if (pcs != cr.pitchClasses) continue;
        ChordSpan span;
        span.start = a;
        span.end = b;
        for (const auto& [pc, p] : lowestOf) span.pitches.push_back (p);
        std::sort (span.pitches.begin(), span.pitches.end());
        const auto& k = score.keyAtBar (score.barAt (a));
        span.name = nameFromRoot (span.pitches, cr.root, k.root, k.scale);
        // What the reader found there gives way; around it, it stays - but
        // rests after the chord go on holding it, as they hold any chord.
        auto soundsIn = [&notes] (Tick x, Tick y)
        {
            for (const auto& n : notes) if (n.start < y && n.start + n.length > x) return true;
            return false;
        };
        std::vector<ChordSpan> kept;
        for (const auto& o : out)
        {
            if (o.end <= a || o.start >= b) { kept.push_back (o); continue; }
            if (o.start < a) { auto left = o; left.end = a; kept.push_back (left); }
            if (o.end > b)
            {
                if (soundsIn (b, o.end)) { auto right = o; right.start = b; kept.push_back (right); }
                else span.end = std::max (span.end, o.end);
            }
        }
        kept.push_back (span);
        std::sort (kept.begin(), kept.end(), [] (const ChordSpan& x, const ChordSpan& y) { return x.start < y.start; });
        out.clear();
        for (const auto& k2 : kept)
        {
            if (! out.empty() && out.back().name == k2.name && out.back().end == k2.start) { out.back().end = k2.end; continue; }
            out.push_back (k2);
        }
    }
    return out;
}

} // namespace nt
