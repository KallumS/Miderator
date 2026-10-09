/*
    Roll - what the piano roll needs from the music, with no drawing in it.

    Miderator shows the score as a DAW does: a note is a bar on a grid, as
    long as it sounds, at the height of its pitch. Nothing about that view is
    stored either (decision 0002 still holds): the grid is a choice in the
    window, and every change here is a plain function on the score, so it is
    tested without a window and undone by keeping the score from before
    (decision 0005).

    These sit beside Edit.h rather than in it so the files the two apps share
    (Score, Edit, Engrave, the generators...) stay identical and can be
    copied across (decision 0026).

    The warnings are the engraver's checks of what an instrument can play
    (decision 0006), read from the notes themselves instead of from a page:
    out of range, outside its sweet register, more notes at once than it
    has, faster than it plays cleanly, a wider leap than it takes.
*/

#pragma once

#include "Edit.h"
#include "Instruments.h"
#include "Score.h"

#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace nt::roll
{

//==============================================================================
// The grid notes snap to.

struct Grid
{
    Tick base = PPQ / 4;     // a sixteenth
    bool triplet = false;
    bool snap = true;

    // One step of the grid; a triplet grid fits three in the space of two.
    Tick step() const { return std::max<Tick> (1, triplet ? base * 2 / 3 : base); }
    // "1/16", "1/8T": how the window names it.
    std::string name() const;
};

// The grid values offered, longest first: a bar of 4/4 down to a 1/32.
const std::vector<Tick>& gridValues();

// The nearest grid line to `t`, or `t` itself when snapping is off. Grid
// lines are counted from each bar line, so a quarter grid in 7/8 still
// meets every bar line.
Tick snap (const Score& score, Tick t, const Grid& grid);
// The grid line at or before `t`.
Tick snapDown (const Score& score, Tick t, const Grid& grid);

//==============================================================================
// What an instrument cannot do, note by note.

struct Warning
{
    bool outOfRange = false;     // the instrument cannot play it
    bool outsideSweet = false;   // it can, but it is not where it sounds like itself
    bool tooManyNotes = false;   // more notes start together than it sounds at once
    bool tooFast = false;        // closer to the last note than it manages cleanly
    bool tooWide = false;        // a wider leap than it takes happily in passing

    // The ones marked with a red flag on the note, not its colour.
    bool flagged() const { return tooManyNotes || tooFast || tooWide; }
    bool any() const { return outOfRange || outsideSweet || flagged(); }
};

using Warnings = std::unordered_map<uint32_t, Warning>;   // note id -> what is wrong; only notes with something

Warnings check (const Score& score);

//==============================================================================
// Edits, as a piano roll makes them.

// Draws a note. Unlike step input it does not clear the voice: it only
// takes the place of the same pitch where they overlap, so a held note and
// a moving line can be drawn over each other. Returns the new note's id.
uint32_t drawNote (Score& score, uint32_t partId, Tick start, Tick length, int pitch, int velocity = 100);

// Moves notes in time and pitch together, as a drag does. Refused, changing
// nothing, if any note would go before the start or out of 0..127.
bool moveNotes (Score& score, const Selection& ids, Tick by, int semitones);

// Copies of the notes, moved by `by` and `semitones`, in their own parts;
// the originals stay. Returns the copies' ids (empty if refused).
std::vector<uint32_t> copyNotesBy (Score& score, const Selection& ids, Tick by, int semitones);

// Lengthens or shortens each note by `by`, none shorter than `minLength`.
void resizeNotes (Score& score, const Selection& ids, Tick by, Tick minLength);

// Moves each note's start by `by`, keeping its end, none shorter than
// `minLength` or before the start of the score.
void resizeStarts (Score& score, const Selection& ids, Tick by, Tick minLength);

void setVelocities (Score& score, const std::map<uint32_t, int>& velocities);

// Starts to the nearest grid line; with `lengths`, ends too (never shorter
// than one step). Notes that land on the same pitch and start merge into one.
void quantise (Score& score, const Selection& ids, Tick step, bool lengths = true);

//==============================================================================
// Names for the keyboard down the side.

// "C4" for 60, as Noterator's tooltips name notes.
std::string keyName (int pitch);
// The General MIDI drum on that key ("Snare", "Closed hi-hat"), or "".
std::string drumName (int pitch);
inline bool isBlackKey (int pitch) { const int pc = ((pitch % 12) + 12) % 12; return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10; }

} // namespace nt::roll
