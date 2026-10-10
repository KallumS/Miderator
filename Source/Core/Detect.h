/*
    Detect - the chords and the scale the music is in, read off the score.

    Chord names are ScaleView's (`scaleview::chordName`, unchanged): this file
    only decides *where* one chord stops and the next starts, and which notes
    belong to it. Key finding is Midi Suggester's `detectKey`, ported - the
    Krumhansl-Kessler profiles, the penalty for time spent outside the scale,
    the bonus for time spent on the tonic triad and the nudge from the first
    and last bass note - with its weights as they stand there.
*/

#pragma once

#include "Score.h"

#include <string>
#include <vector>

namespace nt
{

struct ChordSpan
{
    Tick start = 0, end = 0;
    std::string name;         // "Dmin7/F", or empty for a single note
    std::vector<int> pitches; // what was named, low to high, one per pitch class
};

struct KeyCandidate
{
    int root = 0;             // scaleview root index
    int scale = 0;            // 0 Major, 1 Minor (Natural)
    double score = 0.0;
};

struct KeySpan
{
    Tick start = 0, end = 0;
    int root = 0, scale = 0;
    std::string label;
};

struct TimedPitch
{
    int pitch;
    Tick start, length;
};

// Every non-percussion note of the score (or of the listed parts) in a range.
std::vector<TimedPitch> collectPitches (const Score& score, Tick from, Tick to,
                                        const std::vector<uint32_t>& onlyParts = {});

// Midi Suggester's ranking, best first. firstPc and lastPc may be -1.
std::vector<KeyCandidate> rankKeys (const std::vector<TimedPitch>& notes, int firstPc, int lastPc);

// The chords along the score, cut at the beat at the finest and merged where
// the harmony has not moved. The key decides spelling only.
std::vector<ChordSpan> detectChords (const Score& score, Tick from, Tick to,
                                     const std::vector<uint32_t>& onlyParts = {});

// The key, bar by bar, smoothed so that a change has to last a phrase. With
// `leanOnSignature`, a key the score is written in wins a near tie: four
// notes of C major are as much A minor, and the signature is what settles it.
std::vector<KeySpan> detectKeys (const Score& score, bool leanOnSignature = false);

// Whatever is sounding at one moment, named - what a click on a chord plays.
std::string nameSounding (const std::vector<int>& pitches, int keyRoot, int keyScale);

// The same, read from a root known in advance (decision 0046): ScaleView's own
// reading from that root, in ScaleView's words, the bass after a slash as
// ScaleView would put it. A root not among the pitches: nameSounding.
std::string nameFromRoot (const std::vector<int>& pitches, int root, int keyRoot, int keyScale);

} // namespace nt
