/*
    Exporter - a score, or some of its bars, out as a MIDI file or as audio.

    Both read the same performance playback plays (Perform.h). A MIDI file
    carries all four of AutoCC's lanes, for a sample library; audio is
    rendered through the same synth the app plays with, off the audio thread
    and faster than real time.
*/

#pragma once

#include "AudioEngine.h"
#include "Score.h"

#include <juce_audio_formats/juce_audio_formats.h>

namespace nt
{

struct ExportRange
{
    Tick from = 0;
    Tick to = -1;   // -1: to the end
};

bool exportMidi (const Score& score, ExportRange range, bool withAutoCC, const juce::File& file, juce::String& error);

// The score, or the bars `range` covers, as MusicXML. A range is cut on bar
// lines (it is always whole bars from the window).
bool exportMusicXml (const Score& score, ExportRange range, const juce::File& file, juce::String& error);

// Renders on the calling thread. `progress` gets 0..1 and returns false to
// cancel. The synth is made by the caller, on the message thread, because an
// Audio Unit has to be.
bool renderAudio (const Score& score, ExportRange range, SynthRack& synth, const juce::File& file,
                  const std::function<bool (double)>& progress, juce::String& error);

} // namespace nt
