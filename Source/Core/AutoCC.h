/*
    AutoCC - controller curves drawn from the notes, the way the AutoCC JSFX
    draws them live in REAPER.

    A note starts an arc on each lane: a rise to the peak, a settle down to the
    sustain level, a hold while the note is held, and a fall back to the floor
    once nothing is. The four instrument presets, every lane's numbers, the
    curve shapes and the rule for when a new note restarts the arc are the
    JSFX's (`AutoCC.jsfx`, `load_preset`, `env_tick`, `handle_event`), ported
    from running live to being worked out once for a whole part: the MIDI is
    known in advance here, so the curves are computed rather than performed.

    Which preset a part gets comes from its instrument (Instruments.h).
*/

#pragma once

#include "Instruments.h"
#include "Score.h"

#include <array>
#include <vector>

namespace nt
{

struct CCEvent
{
    Tick at = 0;
    int controller = 1;
    int value = 0;
};

struct CCLane
{
    int controller = 1;
    int floor = 0, peak = 100, sustain = 90;
    int riseMs = 250, settleMs = 200, fallMs = 400;
    int velocity = 40;            // percent: how much velocity scales the arc
    double riseShape = 1.0, fallShape = 1.0;   // curve exponents
};

struct AutoCCOptions
{
    double depth = 1.0;           // Master Depth
    double timeScale = 1.0;       // Time Scale
    bool retrigger = false;       // false is the JSFX's default, Legato
    double stepSeconds = 0.01;    // how finely the curve is sampled
    std::vector<int> onlyControllers;   // empty: every lane
};

std::array<CCLane, 4> autoCCPreset (CCShape shape);

// The curves for one part, sorted by time. Nothing for an instrument whose
// shape is `none`.
std::vector<CCEvent> autoCC (const Score& score, const Part& part, const AutoCCOptions& options = {});

} // namespace nt
