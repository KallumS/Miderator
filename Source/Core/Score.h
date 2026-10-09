/*
    Score - what a piece of music is, as Noterator stores it.

    No JUCE and no drawing in here, ever. The score is MIDI-shaped on purpose:
    a part is a list of notes, each with a start and a length in ticks and a
    sounding pitch. Notation is not stored; it is worked out from these notes
    every time the page is drawn (see Engrave.h). That is what lets a
    generator's MIDI go straight in and be edited as notation without anything
    being converted, and it is why an edit to the notation is always an edit
    to the MIDI underneath it (decision 0002).
*/

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace nt
{

using Tick = int64_t;

// Ticks per quarter note. 960 divides by every note value down to a 64th and
// by triplets and quintuplets of them, so a written rhythm is always whole
// ticks.
inline constexpr Tick PPQ = 960;

struct Note
{
    Tick start = 0;
    Tick length = PPQ;
    int pitch = 60;       // sounding MIDI pitch, whatever the instrument transposes to
    int velocity = 100;
    int voice = 0;        // 0 is the upper voice of a staff, 1 the lower
    uint32_t id = 0;

    Tick end() const { return start + length; }
};

// A time signature, from a bar onwards. Bars count from 0.
struct Meter
{
    int bar = 0;
    int num = 4;
    int den = 4;

    Tick barTicks() const { return static_cast<Tick> (num) * PPQ * 4 / den; }
    // The beat the beams and the chord lane follow: a dotted quarter in a
    // compound signature, otherwise one unit of the denominator.
    Tick beatTicks() const
    {
        const Tick unit = PPQ * 4 / den;
        return (den >= 8 && num % 3 == 0 && num > 3) ? unit * 3 : unit;
    }
    bool operator== (const Meter& o) const { return bar == o.bar && num == o.num && den == o.den; }
};

// A key, from a bar onwards, as indices into ScaleView's roots and scales so
// every app in the family agrees what a key is and how its notes are spelled.
struct KeySig
{
    int bar = 0;
    int root = 0;     // scaleview::roots index - 0 is C
    int scale = 0;    // scaleview::scales index - 0 is Major
    bool operator== (const KeySig& o) const { return bar == o.bar && root == o.root && scale == o.scale; }
};

struct Tempo
{
    Tick at = 0;
    double bpm = 120.0;
};

struct Part
{
    uint32_t id = 0;
    std::string name;
    std::string instrument;   // an Instrument id, see Instruments.h
    std::vector<Note> notes;  // kept sorted by start, then pitch
    bool autoCC = true;       // play and export AutoCC curves for this part
    bool mute = false;
    bool solo = false;
    float volume = 0.8f;      // 0..1, sent as the channel's CC7 baseline
};

struct Score
{
    std::string title { "Untitled" };
    std::string composer;
    std::vector<Part> parts;
    std::vector<Meter> meters { Meter {} };
    std::vector<KeySig> keys { KeySig {} };
    std::vector<Tempo> tempos { Tempo {} };
    int bars = 16;
    uint32_t nextId = 1;

    //==========================================================================
    // Time
    const Meter& meterAtBar (int bar) const;
    const KeySig& keyAtBar (int bar) const;
    Tick barStart (int bar) const;
    Tick barLength (int bar) const { return meterAtBar (bar).barTicks(); }
    int barAt (Tick t) const;              // may run past `bars`, measuring in the last meter
    Tick endTick() const { return barStart (bars); }
    double bpmAt (Tick t) const;
    double secondsAt (Tick t) const;
    Tick tickAtSeconds (double seconds) const;

    //==========================================================================
    // Bookkeeping
    uint32_t newId() { return nextId++; }
    Part* partById (uint32_t id);
    const Part* partById (uint32_t id) const;
    int partIndex (uint32_t id) const;
    Note* findNote (uint32_t noteId, Part** owner = nullptr);
    Tick lastNoteEnd() const;
    void sortNotes();
    // Grows the score so every note fits, in whole bars. Never shrinks it.
    void fitBars();
    // Every note gets an id, the meters, keys and tempos are sorted and the
    // first of each starts at the top. Call after loading anything.
    void normalise();
};

} // namespace nt
