/*
    Edit - every change the editor can make to a score, as plain functions.

    The window never touches a note directly: it calls one of these, so every
    edit is testable without a window and undo is just keeping the score from
    before (decision 0005).

    Note input overwrites, the way step-time input does in every notation
    program: writing a quarter note on beat two replaces whatever that voice
    had on beat two. Adding to a chord is the one input that does not.
*/

#pragma once

#include "Score.h"

#include <set>
#include <vector>

namespace nt
{

using Selection = std::set<uint32_t>;   // note ids

// Clears [start, start + length) in one voice of a part: notes starting
// inside it go, notes running into it from before are shortened to end at
// `start`.
void clearRange (Part& part, Tick start, Tick length, int voice);

// Writes one note, overwriting the voice. Returns the new note's id.
uint32_t writeNote (Score& score, uint32_t partId, Tick start, Tick length, int pitch, int voice, int velocity = 100);

// Adds a note to whatever chord starts at `start` in that voice, taking the
// chord's length; or writes it plainly if nothing starts there.
uint32_t addToChord (Score& score, uint32_t partId, Tick start, Tick length, int pitch, int voice, int velocity = 100);

// A rest is a cleared range: there is nothing else to store.
void writeRest (Score& score, uint32_t partId, Tick start, Tick length, int voice);

void deleteNotes (Score& score, const Selection& ids);
// Returns false and changes nothing if any note would leave 0..127.
bool transposeNotes (Score& score, const Selection& ids, int semitones);
// Moves notes in time; a move that would put a note before the start is refused.
bool moveNotes (Score& score, const Selection& ids, Tick by);
void setLengths (Score& score, const Selection& ids, Tick length);
void setVelocity (Score& score, const Selection& ids, int velocity);
void setVoice (Score& score, const Selection& ids, int voice);

// Notes dropped into a part at `at`, replacing what the part had in the span
// they cover (all voices). The notes' own starts are relative to `at`.
// Returns the new notes' ids.
std::vector<uint32_t> pasteNotes (Score& score, uint32_t partId, Tick at, const std::vector<Note>& notes,
                                  Tick span, bool replace = true);

// Inserts or removes whole bars at `bar`, moving everything after.
void insertBars (Score& score, int bar, int count);
void deleteBars (Score& score, int bar, int count);

// The notes of a selection, copied, with starts relative to the earliest.
std::vector<Note> copyNotes (const Score& score, const Selection& ids, Tick* earliest = nullptr);

// Every note in a span of time, across the listed parts (all parts if empty).
Selection notesInRange (const Score& score, Tick from, Tick to, const std::vector<uint32_t>& parts = {});

// The nearest pitch to `near` with this letter name, in the key: what typing a
// letter writes. `pc` is the pitch class the letter takes in the key.
int nearestPitchWithClass (int pc, int near);

} // namespace nt
