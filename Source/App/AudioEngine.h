/*
    AudioEngine - playback, previews and MIDI input.

    Everything that sounds goes through one synth - or a rack of them, one
    per sixteen channels, for a big score (decision 0023). On a Mac that is
    Apple's own General MIDI instrument set (the DLSMusicDevice Audio Unit every Mac
    has), so a score plays with a real orchestra the moment the app opens and
    nothing has to be downloaded. Anywhere it cannot be loaded, a small synth
    built in here stands in (decision 0007). VST3 and CLAP instruments per part
    are the next step and slot in behind the same SynthBackend.

    The audio thread never allocates or locks: the sequence is swapped in
    whole through an atomic shared_ptr, and messages from the window or a MIDI
    keyboard are taken with a try-lock.
*/

#pragma once

#include "Perform.h"
#include "Score.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include <array>
#include <atomic>
#include <memory>

namespace nt
{

class SynthBackend
{
public:
    virtual ~SynthBackend() = default;
    virtual void prepare (double sampleRate, int blockSize) = 0;
    virtual void render (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) = 0;
    virtual void release() {}
    virtual juce::String name() const = 0;
};

// Apple's General MIDI set where there is one, else the built-in synth.
// `preferBuiltIn` skips the search.
std::unique_ptr<SynthBackend> createSynth (bool preferBuiltIn, juce::String& description);

// MIDI for each bank of sixteen channels.
using BankMidi = std::array<juce::MidiBuffer, maxBanks>;

// Synths side by side, one for each bank of sixteen channels, so a score of
// more than fifteen instruments gives each its own channel (decision 0023).
class SynthRack
{
public:
    explicit SynthRack (bool preferBuiltIn);
    ~SynthRack();

    // Makes synths until there are `banks`. On the message thread only: an
    // Audio Unit is made there. A rack in use is swapped in under a lock.
    void ensureBanks (int banks);
    int banks() const { return static_cast<int> (synths.size()); }
    const juce::String& description() const { return desc; }

    void prepare (double sampleRate, int blockSize);
    void release();
    // Each bank's MIDI through its own synth, mixed into `buffer`. MIDI for a
    // bank with no synth is dropped.
    void render (juce::AudioBuffer<float>& buffer, BankMidi& midi);

private:
    bool builtIn;
    juce::String desc;
    std::vector<std::unique_ptr<SynthBackend>> synths;
    juce::AudioBuffer<float> scratch;
    double rate = 0;
    int block = 0;
};

// The banks a score needs to play every part on a channel of its own.
int banksFor (const Score& score);

// The score as timed MIDI messages, in seconds from `from`.
struct Sequence
{
    struct Event { double seconds; juce::MidiMessage message; uint32_t noteId; int part; int bank; };
    std::vector<Event> events;
    double length = 0;
    Tick from = 0;
    int banks = 1;
};

std::shared_ptr<const Sequence> makeSequence (const Score& score, Tick from, Tick to, const PerformOptions& options);

class AudioEngine : private juce::AudioIODeviceCallback,
                    private juce::MidiInputCallback
{
public:
    AudioEngine();
    ~AudioEngine() override;

    juce::AudioDeviceManager& devices() { return deviceManager; }
    juce::String synthName() const { return synth != nullptr ? synth->description() : juce::String(); }
    void useBuiltInSynth (bool builtIn);
    bool usingBuiltInSynth() const { return builtInOnly; }

    // Playback of the score from a tick. `score` is copied into a sequence.
    void play (const Score& score, Tick from, Tick to = -1);
    // While playing, picks up an edit without stopping.
    void update (const Score& score);
    void stop();
    bool isPlaying() const { return playing.load(); }
    // Where playback started, and how far it has gone since, in seconds.
    Tick playheadTick() const;
    double playheadSeconds() const { return positionSeconds.load(); }
    // The notes sounding now, by id, for the page to light up.
    std::vector<uint32_t> soundingNotes() const;

    // A few notes, now, for as long as `seconds`, on an instrument.
    void preview (const std::vector<int>& pitches, const std::string& instrumentId, double seconds = 0.9, int velocity = 96);
    // Notes one after another, `step` seconds apart: a scale.
    void previewSequence (const std::vector<int>& pitches, const std::string& instrumentId, double step);
    void allNotesOff();

    // Called on the message thread for every note-on and note-off from a
    // MIDI keyboard; the notes also sound straight away.
    std::function<void (const juce::MidiMessage&)> onMidiInput;
    std::function<void()> onPlaybackFinished;

    void setLiveInstrument (const std::string& instrumentId);

private:
    juce::AudioDeviceManager deviceManager;
    std::unique_ptr<SynthRack> synth;
    bool builtInOnly = false;
    juce::CriticalSection synthLock;   // held only while swapping the synth, never by the audio thread for long

    std::shared_ptr<const Sequence> sequence;
    std::atomic<bool> playing { false };
    std::atomic<bool> sequenceChanged { false };
    std::atomic<bool> stopRequested { false };
    std::atomic<double> positionSeconds { 0.0 };
    double sampleRate = 44100.0;
    size_t cursor = 0;
    double engineClock = 0;                       // seconds of audio rendered

    struct Scheduled { double at; juce::MidiMessage message; };
    juce::CriticalSection queueLock;
    std::vector<Scheduled> queue;
    std::vector<Scheduled> pending;               // audio thread's own, from the queue
    juce::MidiMessageCollector midiCollector;
    BankMidi blockMidi;                           // the audio thread's, kept to save allocating

    mutable juce::SpinLock soundingLock;
    std::vector<uint32_t> sounding;
    std::string previewInstrument;
    int previewProgram = -1;
    int liveProgram = -1;

    void audioDeviceIOCallbackWithContext (const float* const* inputs, int numIns, float* const* outputs, int numOuts,
                                           int numSamples, const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart (juce::AudioIODevice*) override;
    void audioDeviceStopped() override;
    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage&) override;

    void schedule (double delaySeconds, const juce::MidiMessage& m);
    void ensureBanks (int banks);
    void openMidiInputs();
};

} // namespace nt
