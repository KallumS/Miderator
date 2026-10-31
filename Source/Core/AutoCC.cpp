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

// AutoCC.jsfx 1.3 load_preset, value for value: fitted to Lorne Balfe's
// mock-ups (21 cues), each lane within 1-2.3 CC of his mean held note.
std::array<CCLane, 4> autoCCPreset (CCShape shape)
{
    switch (shape)
    {
        case CCShape::strings:   // start at 85, swell +31 over 2.2 s, hold it
            return { lane (1, 85, 116, 116, 2200, 0, 1000, 10, 0.66, 1.00),
                     lane (11, 94, 118, 118, 1800, 0, 1000, 0, 0.54, 1.00),
                     lane (7, 94, 94, 94, 0, 0, 0, 0, 1.00, 1.00),
                     lane (21, 92, 92, 92, 0, 0, 0, 0, 1.00, 1.00) };
        case CCShape::brass:     // start high, a quick +7 bloom, ease back by half
            return { lane (1, 102, 109, 106, 1800, 200, 1000, 10, 0.22, 1.00),
                     lane (11, 78, 85, 82, 1800, 400, 1000, 0, 0.23, 1.00),
                     lane (7, 94, 94, 94, 0, 0, 0, 0, 1.00, 1.00),
                     lane (21, 92, 92, 92, 0, 0, 0, 0, 1.00, 1.00) };
        case CCShape::woodwinds: // start low, swell +17 within a second, hold it
            return { lane (1, 46, 63, 63, 800, 0, 1000, 10, 0.66, 1.00),
                     lane (11, 69, 88, 84, 1200, 800, 1000, 0, 0.66, 1.00),
                     lane (7, 94, 94, 94, 0, 0, 0, 0, 1.00, 1.00),
                     lane (21, 92, 92, 92, 0, 0, 0, 0, 1.00, 1.00) };
        case CCShape::neutral:
        case CCShape::none:
        default:                 // Default: every family together - start at 91, swell +24 over 2.6 s
            return { lane (1, 91, 115, 115, 2600, 0, 1000, 10, 0.54, 1.00),
                     lane (11, 85, 104, 104, 2200, 0, 1000, 0, 0.47, 1.00),
                     lane (7, 94, 94, 94, 0, 0, 0, 0, 1.00, 1.00),
                     lane (21, 92, 92, 92, 0, 0, 0, 0, 1.00, 1.00) };
    }
}

std::vector<CCEvent> autoCC (const Score& score, const Part& part, const AutoCCOptions& options)
{
    std::vector<CCEvent> out;
    const auto& inst = instrumentById (part.instrument);
    if (inst.cc == CCShape::none || part.notes.empty()) return out;

    const auto lanes = autoCCPreset (inst.cc);

    // Dynamic shifts the two dynamics lanes, CC1 and CC11 (eff_fl / eff_pk /
    // eff_su in the JSFX); the swell keeps its size, clamped to 0..127.
    const auto shift = [&options] (size_t i, int v)
    {
        return static_cast<double> (std::clamp (v + (i < 2 ? options.dynamic : 0), 0, 127));
    };
    const auto floorOf = [&] (size_t i) { return shift (i, lanes[i].floor); };

    // A note this soon after every key went up carries the line on instead of
    // swelling again (RESTART_GAP in the JSFX). In seconds, not time-scaled.
    constexpr double restartGap = 0.4;

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
    for (size_t i = 0; i < 4; ++i) runs[i].value = floorOf (i);

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
            r.start = r.stage == 0 ? floorOf (i) : r.value;
            r.value = r.start;
            r.stage = 1;
            r.t = 0;
            const double va = lanes[i].velocity * 0.01;
            r.velScale = 1 - va + va * (velocity / 127.0);
            emit (now, i, r.start);
        }
    };

    // carry_env: a lane already falling holds where it is (stage 5, kept
    // relative to the floor) until the next key-up; one still rising or
    // sustaining carries on.
    auto carry = [&]
    {
        for (size_t i = 0; i < 4; ++i)
        {
            auto& r = runs[i];
            if (r.stage == 4) { r.releaseFrom = r.value - floorOf (i); r.stage = 5; r.t = 0; }
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
            const double fl = floorOf (i);
            const double pk = fl + (shift (i, l.peak) - fl) * r.velScale * options.depth;
            const double su = fl + (shift (i, l.sustain) - fl) * r.velScale * options.depth;
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
                else if (st == 5)
                {
                    v = std::clamp (fl + r.releaseFrom, 0.0, 127.0);
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
    for (size_t i = 0; i < 4; ++i) emit (std::max (0.0, first - 0.05), i, floorOf (i));

    double now = first;
    double keyUpAt = -1.0e9;
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
                if (count == 1 || options.retrigger || idle())
                {
                    // Legato only: Retrigger asks for a swell on every note.
                    if (count == 1 && ! options.retrigger && ! idle() && ed.t - keyUpAt < restartGap)
                        carry();
                    else
                        start (ed.t, ed.velocity);
                }
            }
            else
            {
                if (held[p]) { held[p] = false; count = std::max (0, count - 1); }
                if (count == 0) { keyUpAt = ed.t; release(); }
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
