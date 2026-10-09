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

// A result made to last exactly `span`: repeated until it fills it, cut where
// it ends (decision 0019).
GeneratedResult fitToSpan (const GeneratedResult& result, Tick span);

// A line of chords dealt out to `lines` parts, one note each, top note to the
// first: the plainest orchestration of block chords. A chord with fewer notes
// than parts doubles its lowest note in the parts left over; one with more
// gives the extra notes to no one.
std::vector<std::vector<Note>> spreadChords (const std::vector<Note>& notes, int lines);

// Drops a result into a span of bars across chosen parts. One part takes it
// as insertResult would; several share it out by what each instrument is
// (Orchestrate.h, decision 0040). Only the parts that receive music are
// cleared.
InsertReport insertIntoRange (Score& score, const GeneratedResult& result, const std::vector<uint32_t>& parts,
                              Tick from, Tick to, const InsertOptions& options = {});

// The instrument a generated part should be played by when it gets a part of
// its own.
std::string instrumentForGeneratedPart (const GeneratedPart& part, const Instrument& target);

} // namespace nt
