/*
    Engrave - the score in, a laid-out page out.

    Pure: no JUCE and no drawing. Everything here is decided in staff spaces -
    which notes make a chord, how a duration is written, where a beam goes,
    which way a stem turns, which accidental is shown - and returned as values,
    so a test can assert a ledger line or a beam exists. `ScoreRenderer` turns
    the result into glyphs. That split is Starting Blocks Notation's (its
    decision 0003, "split the engraving at a pen") and the rules are mostly
    its, ported and grown from one block on one or two staves to a score of
    many parts:

      - a note is written as its share of the bar where the MIDI was gated
        short of the next note (sb 0005);
      - a duration with a single symbol is written as that symbol wherever it
        falls; only one with no symbol of its own is split;
      - the key signature is scored, not looked up (Spelling.h);
      - stem length is measured from the far head of a chord;
      - a beam leans a quarter of the interval it covers, at most a space and
        a bit;
      - accidentals last to the bar line, carry across a tie, and pack into
        columns;
      - which side of the stem a crossed head goes is decided once, here.

    What is new is that every staff of every part shares one set of columns
    per bar, so notes that sound together line up vertically on the page
    (decision 0004).

    Coordinates: x runs right from the start of the first bar's content, y runs
    down, and each staff's top line is at its own `top`. A staff position is
    counted in half-spaces up from the bottom line: 0 is the bottom line, 8 the
    top line, -2 the first ledger line below.
*/

#pragma once

#include "Instruments.h"
#include "Score.h"
#include "Spelling.h"

#include <cmath>
#include <map>
#include <string>
#include <vector>

namespace nt::engrave
{

struct Options
{
    bool transposedScore = false;
    double partGap = 9.0;        // spaces between the bottom of one part and the top of the next
    double grandGap = 7.0;       // between the two staves of a piano
    double minStaffGap = 4.0;    // never closer than this, however low a staff goes
};

struct Head
{
    int pos = 0;                 // half-spaces above the bottom line
    int pitch = 60;              // sounding
    int written = 60;
    uint32_t noteId = 0;
    int accidental = 0;
    bool showAccidental = false;
    double accidentalX = 0;      // relative to the element's x, negative is left
    int side = 0;                // 1: crossed to the far side of the stem
    double x = 0;                // absolute x of the head's left edge
    bool outOfRange = false;     // the instrument cannot play it
    bool outsideSweet = false;   // it can, but it is not where it sounds like itself
    bool drumCross = false;      // an x notehead (cymbals, hi-hat)
};

enum class Stem { none, up, down };

struct Element
{
    bool rest = false;
    bool measureRest = false;
    int voice = 0;
    Tick at = 0;                 // absolute
    Tick ticks = 0;              // written duration
    int den = 4;                 // 1 whole, 2 half, 4 quarter, 8 eighth ...
    int dots = 0;
    int tuplet = 0;              // 3 for a triplet, else 0
    std::vector<Head> heads;     // low to high
    Stem stem = Stem::none;
    double x = 0;                // the column's x: left edge of an uncrossed head
    double stemX = 0;
    double stemTop = 0, stemBottom = 0;   // y, absolute
    int beam = -1;               // index into Staff::beams
    int beamCount = 0;           // beams or flags this value carries
    bool tieIn = false, tieOut = false;
    double restY = 2.0;          // a rest's centre line, relative to the staff top
    bool tooManyNotes = false;   // a chord on an instrument that plays fewer at once
    bool tooFast = false;        // closer to the last note than the instrument manages cleanly
    bool tooWide = false;        // a leap wider than the instrument takes happily
    int measure = 0;
};

struct Beam
{
    std::vector<int> elements;   // indices into Staff::elements
    Stem stem = Stem::up;
    double x1 = 0, y1 = 0, x2 = 0, y2 = 0;   // the primary beam's outer edge, at the stem lines
    // Secondary beams: from x to x at level n (1 = the second beam). A stub
    // is a short one pointing into the group.
    struct Segment { int level; double xa, xb; };
    std::vector<Segment> segments;
    int tuplet = 0;
    double yAt (double x) const { return std::abs (x2 - x1) < 1e-9 ? y1 : y1 + (y2 - y1) * (x - x1) / (x2 - x1); }
};

struct Tuplet
{
    int number = 3;
    std::vector<int> elements;   // indices into Staff::elements, in order
    double x1 = 0, x2 = 0, y = 0;
    bool bracket = false;
    bool above = true;
};

struct Tie
{
    double x1 = 0, x2 = 0;
    double y = 0;                // the heads' line
    bool above = false;
};

struct Staff
{
    int part = 0;                // index into score.parts
    int staffInPart = 0;         // 0 top, 1 the lower staff of a piano
    int staffCount = 1;
    Clef clef = Clef::treble;
    double top = 0;              // y of the top line
    int lowestPos = 0, highestPos = 8;   // ink extents, half-spaces
    std::vector<Element> elements;
    std::vector<Beam> beams;
    std::vector<Tuplet> tuplets;
    std::vector<Tie> ties;
};

struct Measure
{
    int bar = 0;
    Tick start = 0, ticks = 0;
    double x = 0, width = 0;     // x of the bar's left line
    double contentX = 0;         // where the first note's column can sit
    bool showKey = false, showTime = false, showClef = false;
    KeySignature key;
    KeySignature previousKey;
    Meter meter;
    std::vector<std::pair<Tick, double>> columns;   // absolute tick -> x, sorted
};

struct Layout
{
    std::vector<Staff> staves;
    std::vector<Measure> measures;
    double width = 0, height = 0;
    double headerWidth = 0;      // the clef, key and time signature at the very start

    double xForTick (Tick t) const;
    // For a moving playhead, and a page that scrolls with it (decision 0034):
    // xForTick, but continuous across bar lines - after a bar's last note it
    // runs on to the next bar's first, so it never jumps the bar line.
    double playheadX (Tick t) const;
    Tick tickForX (double x) const;
    int measureAtX (double x) const;
    int staffAtY (double y) const;     // nearest staff
    const Head* headAt (double x, double y, int* staffOut = nullptr, int* elementOut = nullptr) const;
};

// Layout widths of the symbols, in spaces. Bravura's own metrics, so the
// layout makes room for exactly what the font draws.
inline constexpr double headWidth = 1.18;
inline constexpr double wholeHeadWidth = 1.688;
inline constexpr double stemThickness = 0.12;
inline constexpr double beamThickness = 0.5;
inline constexpr double beamSpacing = 0.25;
inline constexpr double staffLineThickness = 0.13;
inline constexpr double ledgerExtension = 0.4;

Layout layout (const Score& score, const Options& options = {});

// Which staff a sounding pitch goes on for an instrument with several.
int staffForPitch (const Instrument& inst, int pitch);

// The staff position of a written pitch in a clef and key.
int positionOf (int writtenPitch, Clef clef, const KeyContext& key, int* accidental = nullptr);

// The sounding pitch a click at a staff position means: the letter on that
// line or space with the key signature's accidental, back to sounding pitch.
int pitchAtPosition (int pos, Clef clef, const KeyContext& key, const Instrument& inst, bool transposedScore);

// The written values a duration starting at `pos` in a bar can be split into.
struct Value { Tick ticks; int den; int dots; int tuplet; };
std::vector<Value> splitDuration (Tick pos, Tick ticks, Tick barTicks, bool rest, Tick beatTicks);
const Value* singleValue (Tick ticks);

// General MIDI drum map: where each piece sits on the five-line staff.
struct DrumNote { int pos; bool cross; int voice; };
DrumNote drumNote (int pitch);

} // namespace nt::engrave
