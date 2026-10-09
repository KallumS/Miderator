#include "AutoCC.h"

#include <algorithm>
#include <cmath>

namespace nt
{

namespace
{
CCLane lane (int cc, int fl, int pk, int su, int atk, int dcy, int rel, int vel, double ae, double re)
{
    CCLane l;
    l.controller = cc;
    l.floor = fl; l.peak = pk; l.sustain = su;
    l.riseMs = atk; l.settleMs = dcy; l.fallMs = rel;
    l.velocity = vel;
    l.riseShape = ae; l.fallShape = re;
    return l;
}

double shaped (double p, double exponent)
{
    p = std::clamp (p, 0.0, 1.0);
    return std::pow (p, std::clamp (exponent, 0.08, 8.0));
}
} // namespace

// AutoCC.jsfx load_preset, value for value.
std::array<CCLane, 4> autoCCPreset (CCShape shape)
{
    switch (shape)
    {
        case CCShape::strings:   // slow swelling rise, settled sustain, long gentle fall
            return { lane (1, 0, 102, 89, 900, 400, 1100, 35, 2.00, 1.25),
                     lane (11, 65, 105, 101, 700, 300, 900, 25, 1.70, 1.20),
                     lane (7, 111, 119, 114, 650, 350, 800, 0, 1.60, 1.15),
                     lane (21, 0, 25, 25, 1800, 0, 180, 20, 2.80, 0.45) };
        case CCShape::brass:     // fast attack with a bloom, firm sustain, moderate fall
            return { lane (1, 0, 112, 92, 130, 220, 420, 55, 0.55, 0.85),
                     lane (11, 70, 110, 104, 100, 180, 380, 40, 0.50, 0.85),
                     lane (7, 110, 121, 113, 120, 200, 400, 0, 0.55, 0.85),
                     lane (21, 0, 25, 25, 900, 0, 150, 25, 2.60, 0.45) };
        case CCShape::woodwinds: // quick speech-like attack, steady sustain, quick fall
            return { lane (1, 0, 106, 91, 150, 160, 220, 45, 0.60, 0.70),
                     lane (11, 68, 105, 101, 120, 140, 200, 35, 0.55, 0.70),
                     lane (7, 112, 119, 114, 140, 150, 210, 0, 0.60, 0.70),
                     lane (21, 0, 25, 25, 800, 0, 130, 20, 2.60, 0.45) };
        case CCShape::neutral:
        case CCShape::none:
        default:                 // Default: neutral, usable on anything
            return { lane (1, 0, 100, 87, 250, 200, 400, 40, 1.00, 1.00),
                     lane (11, 60, 100, 97, 200, 150, 350, 30, 1.00, 1.00),
                     lane (7, 112, 118, 114, 220, 180, 360, 0, 1.00, 1.00),
                     lane (21, 0, 25, 25, 1100, 0, 160, 20, 2.60, 0.45) };
    }
}

std::vector<CCEvent> autoCC (const Score& score, const Part& part, const AutoCCOptions& options)
{
    std::vector<CCEvent> out;
    const auto& inst = instrumentById (part.instrument);
    if (inst.cc == CCShape::none || part.notes.empty()) return out;

    const auto lanes = autoCCPreset (inst.cc);

    // The part as note-ons and note-offs in seconds. At one instant the
    // note-offs go first, as REAPER delivers them, so a line of notes written
    // end to end lets go and starts again on each note.
    struct Edge { double t; bool on; int pitch; int velocity; };
    std::vector<Edge> edges;
    for (const auto& n : part.notes)
    {
        edges.push_back ({ score.secondsAt (n.start), true, n.pitch, n.velocity });
        edges.push_back ({ score.secondsAt (n.end()), false, n.pitch, n.velocity });
    }
    std::stable_sort (edges.begin(), edges.end(), [] (const Edge& a, const Edge& b)
    {
        if (a.t != b.t) return a.t < b.t;
        return ! a.on && b.on;
    });

    struct Run { int stage = 0; double t = 0, value = 0, start = 0, releaseFrom = 0, velScale = 1; int last = -1; };
    std::array<Run, 4> runs;
    for (size_t i = 0; i < 4; ++i) runs[i].value = lanes[i].floor;

    std::array<bool, 128> held {};
    int count = 0;
    const double ts = options.timeScale;

    auto live = [&options, &lanes] (size_t i)
    {
        if (options.onlyControllers.empty()) return true;
        return std::find (options.onlyControllers.begin(), options.onlyControllers.end(),
                          lanes[i].controller) != options.onlyControllers.end();
    };

    auto emit = [&] (double seconds, size_t i, double v)
    {
        if (! live (i)) return;
        const int iv = std::clamp (static_cast<int> (std::floor (v + 0.5)), 0, 127);
        if (iv == runs[i].last) return;
        runs[i].last = iv;
        out.push_back ({ score.tickAtSeconds (seconds), lanes[i].controller, iv });
    };

    auto idle = [&runs]
    {
        for (const auto& r : runs) if (r.stage != 0) return false;
        return true;
    };

    auto start = [&] (double now, int velocity)
    {
        for (size_t i = 0; i < 4; ++i)
        {
            auto& r = runs[i];
            r.start = r.stage == 0 ? lanes[i].floor : r.value;
            r.value = r.start;
            r.stage = 1;
            r.t = 0;
            const double va = lanes[i].velocity * 0.01;
            r.velScale = 1 - va + va * (velocity / 127.0);
            emit (now, i, r.start);
        }
    };

    auto release = [&]
    {
        for (auto& r : runs)
            if (r.stage != 0) { r.releaseFrom = r.value; r.stage = 4; r.t = 0; }
    };

    // env_tick: one step of every lane's envelope.
    auto tick = [&] (double now, double dt)
    {
        for (size_t i = 0; i < 4; ++i)
        {
            auto& r = runs[i];
            const auto& l = lanes[i];
            const double fl = l.floor;
            const double pk = fl + (l.peak - fl) * r.velScale * options.depth;
            const double su = fl + (l.sustain - fl) * r.velScale * options.depth;
            double t = r.t + dt, v = r.value;
            int st = r.stage;
            for (bool done = false; ! done;)
            {
                if (st == 1)
                {
                    const double dur = l.riseMs * 0.001 * ts;
                    if (t >= dur) { t -= dur; st = 2; }
                    else { v = r.start + (pk - r.start) * shaped (dur > 0 ? t / dur : 1, l.riseShape); done = true; }
                }
                else if (st == 2)
                {
                    const double dur = l.settleMs * 0.001 * ts;
                    if (t >= dur) { t -= dur; st = 3; }
                    else { v = pk + (su - pk) * (dur > 0 ? t / dur : 1); done = true; }
                }
                else if (st == 3)
                {
                    v = su;
                    done = true;
                }
                else if (st == 4)
                {
                    const double dur = l.fallMs * 0.001 * ts;
                    if (t >= dur) { st = 0; t = 0; }
                    else { v = r.releaseFrom + (fl - r.releaseFrom) * shaped (dur > 0 ? t / dur : 1, l.fallShape); done = true; }
                }
                else
                {
                    v = fl;
                    t = 0;
                    done = true;
                }
            }
            r.stage = st;
            r.t = t;
            r.value = v;
            emit (now, i, v);
        }
    };

    // The floor first, so the library starts somewhere sane.
    const double first = edges.front().t;
    for (size_t i = 0; i < 4; ++i) emit (std::max (0.0, first - 0.05), i, lanes[i].floor);

    double now = first;
    size_t e = 0;
    const double step = std::max (0.001, options.stepSeconds);
    while (e < edges.size() || ! idle())
    {
        // Every edge due by now, in order.
        while (e < edges.size() && edges[e].t <= now + 1e-9)
        {
            const auto& ed = edges[e];
            const auto p = static_cast<size_t> (std::clamp (ed.pitch, 0, 127));
            if (ed.on)
            {
                if (! held[p]) { held[p] = true; ++count; }
                if (count == 1 || options.retrigger || idle()) start (ed.t, ed.velocity);
            }
            else
            {
                if (held[p]) { held[p] = false; count = std::max (0, count - 1); }
                if (count == 0) release();
            }
            ++e;
        }
        const double next = now + step;
        tick (next, step);
        now = next;
        if (e >= edges.size() && idle()) break;
        if (now - first > 3600.0) break;   // an hour of one part is a runaway, not music
    }
    std::stable_sort (out.begin(), out.end(), [] (const CCEvent& a, const CCEvent& b) { return a.at < b.at; });
    return out;
}

} // namespace nt
