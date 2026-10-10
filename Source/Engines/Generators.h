/*
    Generators - what happens around a generator: the context it is asked in,
    and putting what it made into the score.

    A generator writes for an instrument when it can (Midi Catalogue does);
    when it cannot, what it made is moved by octaves into the part's own
    range, preferring where the instrument sounds like itself, so a Good Idea
    tune dropped onto a cello part lands in the cello's register and not the
    violin's (decision 0006).
*/

#pragma once

#include "Edit.h"
#include "Instruments.h"
#include "LuaEngine.h"

namespace nt
{

// The octave shift that puts the most notes inside the instrument's sweet
// register, then each note still outside its range folded in by octaves.
std::vector<Note> fitToInstrument (std::vector<Note> notes, const Instrument& inst);

// The context a generator is asked in: the key, meter and tempo at `at`, the
// part's instrument, and the selected notes, if any, relative to the bar the
// selection starts in.
GeneratorContext contextFor (const Score& score, uint32_t partId, Tick at, const Selection& selection);

// No more notes at once than the instrument plays (decision 0036). Where
// more start together it keeps the top ones - the tune - or the bottom ones
// for an instrument that takes the bass; a note still sounding when the next
// starts is shortened to end there, so a line stays legato. Drums are left
// as they are. `dropped`, if given, counts the notes taken out.
std::vector<Note> fitToPolyphony (std::vector<Note> notes, const Instrument& inst, int* dropped = nullptr);

struct InsertOptions
{
    bool fitToInstrument = true;
    bool fitPolyphony = true;     // fitToPolyphony every line, for the part it lands in
    bool replace = true;          // clear what the target parts had in the span first
    // With no target part, every line gets a part of its own, chosen as if
    // it were going beside a part playing this instrument.
    std::string contextInstrument = "pno";
};

struct InsertReport
{
    Selection newNotes;
    std::vector<uint32_t> newParts;
    std::vector<uint32_t> thinnedParts;   // parts given fewer notes than the result had, to suit the instrument
    std::vector<std::string> unplaced;    // lines no chosen part could take ("Drums" with no kit), left out
};

// The most notes a line sounds at once.
int polyphonyOf (const std::vector<Note>& notes);

// Drops a result into the score at `at`. Its first part goes into the target
// part (none: 0); the others into parts of their own - an existing part on the
// right instrument that is silent in that span, or a new one. A line with
// chords never goes to a new part on an instrument that plays one note at a
// time; one that must go into such a part is thinned to suit it.
InsertReport insertResult (Score& score, const GeneratedResult& result, uint32_t targetPartId, Tick at,
                           const InsertOptions& options = {});

// Records the root a chord result was made on over [at, at + length), so the
// Chords lane names it from there (decision 0046). Roots recorded there
// before give way. Nothing for a result with no root.
void markChordRoot (Score& score, const GeneratedResult& result, Tick at, Tick length);

// A result made to last exactly `span`: cut where the span ends, and played
// once where the span is longer - the rest left empty (decisions 0019, 0042).
GeneratedResult fitToSpan (const GeneratedResult& result, Tick span);

// A result as it is auditioned (decision 0043): every note of every line, on
// a piano, the drums on a kit, from `at` and cut at `at + length` (0: all of
// it). The parts are added to `score` and soloed if anything else is, so
// they are always heard. Nothing is fitted to an instrument: an audition is
// the music as it was made.
void addAudition (Score& score, const GeneratedResult& result, Tick at, Tick length);

// A line of chords dealt out to `lines` parts, one note each, top note to the
// first: the plainest orchestration of block chords. A chord with fewer notes
// than parts doubles its lowest note in the parts left over; one with more
// gives the extra notes to no one.
std::vector<std::vector<Note>> spreadChords (const std::vector<Note>& notes, int lines);

// All of a result in one part (decision 0041): its lines together, thinned
// to the notes the instrument plays at once (the top ones, the bottom for a
// bass), then moved into its register. Drums go only to a drum kit, and a
// kit takes only drums; a line that fits neither is reported, not placed.
// `span` 0 keeps the result's own length; otherwise it fills the span.
// One line of music and nothing else - a melody, a motif: it goes to one
// part, never shared out (decision 0042). Chords, a tune with chords or two
// lines are shared; so are drums, which go to a kit.
bool isSingleLine (const GeneratedResult& result);

InsertReport insertWhole (Score& score, const GeneratedResult& result, uint32_t partId, Tick at, Tick span,
                          const InsertOptions& options = {});

// Drops a result into [from, to) across chosen parts. One part takes all of
// it (insertWhole); several share it out by what each instrument is
// (Orchestrate.h, decision 0040). No part is ever added. Only the parts that
// receive music are cleared.
InsertReport insertIntoRange (Score& score, const GeneratedResult& result, const std::vector<uint32_t>& parts,
                              Tick from, Tick to, const InsertOptions& options = {});

// The instrument a generated part should be played by when it gets a part of
// its own.
std::string instrumentForGeneratedPart (const GeneratedPart& part, const Instrument& target);

} // namespace nt
