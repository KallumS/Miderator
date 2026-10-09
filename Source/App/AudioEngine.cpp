#include "AudioEngine.h"

#include "Instruments.h"

#include <cmath>

namespace nt
{

//==============================================================================
// The built-in synth: enough of a General MIDI player to hear a score by,
// where the system's own instruments cannot be had. Sixteen channels, a
// program change picks a family of sound, CC7 and CC11 set the level, so
// AutoCC's swells are heard here too.

namespace
{
class BasicSynth final : public SynthBackend
{
public:
    void prepare (double rate, int) override
    {
        sr = rate;
        for (auto& v : voices) v.active = false;
        for (auto& c : channels) c = Channel {};
        channels[9].drums = true;
    }

    juce::String name() const override { return "Noterator's built-in synth"; }

    void render (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override
    {
        buffer.clear();
        int pos = 0;
        for (const auto meta : midi)
        {
            const int at = juce::jlimit (0, buffer.getNumSamples(), meta.samplePosition);
            renderRange (buffer, pos, at - pos);
            pos = at;
            handle (meta.getMessage());
        }
        renderRange (buffer, pos, buffer.getNumSamples() - pos);
    }

private:
    enum Kind { pluck, bowed, brassy, reed, flute, organ, choir, timpani, kick, snare, hat, tom, cymbal };

    struct Channel { int program = 0; float volume = 100.0f / 127.0f; float expression = 1.0f; bool drums = false; };
    struct Voice
    {
        bool active = false;
        int channel = 0, note = 0;
        Kind kind = pluck;
        float velocity = 0;
        double phase = 0, freq = 440, age = 0;
        float env = 0, lp = 0, lp2 = 0;
        bool released = false;
        float attack = 0.01f, decay = 0.3f, sustain = 0.5f, release = 0.2f;
        uint32_t rng = 1;
    };

    double sr = 44100.0;
    std::array<Channel, 16> channels {};
    std::array<Voice, 64> voices {};

    static Kind kindFor (int program, bool drums, int note)
    {
        if (drums)
        {
            if (note == 35 || note == 36) return kick;
            if (note == 38 || note == 40 || note == 37 || note == 39) return snare;
            if (note == 42 || note == 44 || note == 46) return hat;
            if (note == 41 || note == 43 || note == 45 || note == 47 || note == 48 || note == 50) return tom;
            return cymbal;
        }
        if (program == 47) return timpani;
        if (program < 16 || (program >= 24 && program < 40) || program == 46 || (program >= 104 && program < 112)) return pluck;
        if (program < 24) return organ;
        if (program >= 52 && program < 56) return choir;
        if (program < 56) return bowed;
        if (program < 64) return brassy;
        if (program < 72) return reed;
        if (program < 80) return flute;
        return bowed;
    }

    void handle (const juce::MidiMessage& m)
    {
        const int ch = juce::jlimit (0, 15, m.getChannel() - 1);
        auto& c = channels[static_cast<size_t> (ch)];
        if (m.isNoteOn())
        {
            Voice* v = nullptr;
            for (auto& cand : voices) if (! cand.active) { v = &cand; break; }
            if (v == nullptr)
            {
                // Steal the oldest released voice, else the oldest.
                double oldest = -1;
                for (auto& cand : voices)
                    if (cand.age > oldest && (cand.released || v == nullptr)) { oldest = cand.age; v = &cand; }
            }
            if (v == nullptr) return;
            *v = Voice {};
            v->active = true;
            v->channel = ch;
            v->note = m.getNoteNumber();
            v->velocity = m.getFloatVelocity();
            v->kind = kindFor (c.program, c.drums, v->note);
            v->freq = 440.0 * std::pow (2.0, (v->note - 69) / 12.0);
            v->rng = static_cast<uint32_t> (v->note) * 2654435761u + 1u;
            switch (v->kind)
            {
                case pluck:   v->attack = 0.003f; v->decay = static_cast<float> (juce::jlimit (0.3, 3.0, 400.0 / v->freq)); v->sustain = 0.0f; v->release = 0.25f; break;
                case bowed:   v->attack = 0.09f;  v->decay = 0.3f; v->sustain = 0.85f; v->release = 0.25f; break;
                case brassy:  v->attack = 0.04f;  v->decay = 0.2f; v->sustain = 0.8f;  v->release = 0.15f; break;
                case reed:    v->attack = 0.03f;  v->decay = 0.2f; v->sustain = 0.8f;  v->release = 0.1f;  break;
                case flute:   v->attack = 0.05f;  v->decay = 0.2f; v->sustain = 0.85f; v->release = 0.12f; break;
                case organ:   v->attack = 0.01f;  v->decay = 0.1f; v->sustain = 0.9f;  v->release = 0.08f; break;
                case choir:   v->attack = 0.15f;  v->decay = 0.3f; v->sustain = 0.85f; v->release = 0.3f;  break;
                case timpani: v->attack = 0.002f; v->decay = 1.2f; v->sustain = 0.0f;  v->release = 0.6f;  break;
                case kick:    v->attack = 0.001f; v->decay = 0.25f; v->sustain = 0.0f; v->release = 0.1f; break;
                case snare:   v->attack = 0.001f; v->decay = 0.14f; v->sustain = 0.0f; v->release = 0.1f; break;
                case hat:     v->attack = 0.001f; v->decay = v->note == 46 ? 0.35f : 0.05f; v->sustain = 0.0f; v->release = 0.05f; break;
                case tom:     v->attack = 0.001f; v->decay = 0.3f; v->sustain = 0.0f; v->release = 0.1f; break;
                case cymbal:  v->attack = 0.001f; v->decay = 1.2f; v->sustain = 0.0f; v->release = 0.5f; break;
            }
        }
        else if (m.isNoteOff())
        {
            for (auto& v : voices)
                if (v.active && ! v.released && v.channel == ch && v.note == m.getNoteNumber()) v.released = true;
        }
        else if (m.isProgramChange()) c.program = m.getProgramChangeNumber();
        else if (m.isController())
        {
            const int cc = m.getControllerNumber();
            const float val = static_cast<float> (m.getControllerValue()) / 127.0f;
            if (cc == 7) c.volume = val;
            else if (cc == 11) c.expression = val;
            else if (cc == 120 || cc == 123)
                for (auto& v : voices) if (v.channel == ch) { if (cc == 120) v.active = false; else v.released = true; }
        }
    }

    float noise (Voice& v)
    {
        v.rng = v.rng * 1664525u + 1013904223u;
        return static_cast<float> (v.rng >> 8) / 8388608.0f - 1.0f;
    }

    void renderRange (juce::AudioBuffer<float>& buffer, int start, int count)
    {
        if (count <= 0) return;
        auto* left = buffer.getWritePointer (0);
        auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
        const double dt = 1.0 / sr;
        for (auto& v : voices)
        {
            if (! v.active) continue;
            const auto& c = channels[static_cast<size_t> (v.channel)];
            const float level = v.velocity * c.volume * c.expression * 0.22f;
            const float cutoff = static_cast<float> (juce::jlimit (0.02, 0.9, (v.freq * 6.0 + 600.0 * v.velocity) / sr * 6.28));
            for (int i = 0; i < count; ++i)
            {
                // The envelope.
                const auto t = static_cast<float> (v.age);
                if (v.released) v.env *= std::exp (-static_cast<float> (dt) / std::max (0.005f, v.release * 0.35f));
                else if (t < v.attack) v.env = t / v.attack;
                else v.env = v.sustain + (1.0f - v.sustain) * std::exp (-(t - v.attack) / std::max (0.005f, v.decay * 0.4f));
                if ((v.released || v.sustain <= 0.0f) && t > v.attack && v.env < 0.0005f) { v.active = false; break; }

                float s = 0;
                const double ph = v.phase;
                switch (v.kind)
                {
                    case pluck:
                    {
                        const float tri = static_cast<float> (2.0 * std::abs (2.0 * ph - 1.0) - 1.0);
                        s = 0.6f * tri + 0.4f * static_cast<float> (std::sin (ph * juce::MathConstants<double>::twoPi * 2.0)) * std::exp (-t * 3.0f);
                        break;
                    }
                    case bowed: case brassy:
                    {
                        const double vib = v.kind == bowed && t > 0.3f ? 1.0 + 0.004 * std::sin (t * 5.5 * juce::MathConstants<double>::twoPi) : 1.0;
                        s = static_cast<float> (2.0 * ph - 1.0);
                        v.lp += cutoff * (v.kind == brassy ? 1.4f : 1.0f) * (s - v.lp);
                        v.lp2 += cutoff * (v.lp - v.lp2);
                        s = v.lp2 * 1.6f;
                        v.phase += v.freq * dt * (vib - 1.0);
                        break;
                    }
                    case reed:
                        s = ph < 0.35 ? 0.8f : -0.45f;
                        v.lp += cutoff * (s - v.lp);
                        s = v.lp;
                        break;
                    case flute:
                        s = static_cast<float> (std::sin (ph * juce::MathConstants<double>::twoPi)) + 0.04f * noise (v);
                        break;
                    case organ:
                        s = static_cast<float> (0.6 * std::sin (ph * juce::MathConstants<double>::twoPi) + 0.3 * std::sin (ph * 2 * juce::MathConstants<double>::twoPi)
                                                + 0.15 * std::sin (ph * 4 * juce::MathConstants<double>::twoPi));
                        break;
                    case choir:
                        s = static_cast<float> (0.7 * std::sin (ph * juce::MathConstants<double>::twoPi) + 0.25 * std::sin (ph * 3 * juce::MathConstants<double>::twoPi + 0.3 * std::sin (t * 4.0)));
                        break;
                    case timpani:
                        s = static_cast<float> (std::sin (ph * juce::MathConstants<double>::twoPi)) + 0.15f * noise (v) * std::exp (-t * 30.0f);
                        break;
                    case kick:
                        v.freq = 50.0 + 110.0 * std::exp (-t * 30.0);
                        s = static_cast<float> (std::sin (ph * juce::MathConstants<double>::twoPi)) * 1.6f;
                        break;
                    case snare:
                        s = 0.7f * noise (v) + 0.5f * static_cast<float> (std::sin (ph * juce::MathConstants<double>::twoPi)) * std::exp (-t * 25.0f);
                        if (i == 0 && v.age < dt) v.freq = 185.0;
                        break;
                    case hat:
                    {
                        const float n = noise (v);
                        s = (n - v.lp) * 0.5f;
                        v.lp = n;
                        break;
                    }
                    case tom:
                        v.freq = (100.0 + (v.note - 41) * 12.0) * (1.0 + 0.5 * std::exp (-t * 20.0));
                        s = static_cast<float> (std::sin (ph * juce::MathConstants<double>::twoPi)) * 1.2f;
                        break;
                    case cymbal:
                    {
                        const float n = noise (v);
                        s = (n - v.lp) * 0.35f;
                        v.lp = n * 0.6f;
                        break;
                    }
                }
                v.phase += v.freq * dt;
                v.phase -= std::floor (v.phase);
                v.age += dt;
                const float out = s * v.env * level;
                left[start + i] += out;
                if (right != nullptr) right[start + i] += out;
            }
        }
    }
};

//==============================================================================
// A hosted instrument: Apple's General MIDI set today, a VST3 or CLAP per
// part later.
class PluginSynth final : public SynthBackend
{
public:
    explicit PluginSynth (std::unique_ptr<juce::AudioPluginInstance> p) : plugin (std::move (p)) {}

    void prepare (double rate, int block) override
    {
        plugin->setPlayConfigDetails (0, 2, rate, block);
        plugin->prepareToPlay (rate, block);
        scratch.setSize (2, block);
    }

    void release() override { plugin->releaseResources(); }

    void render (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override
    {
        const int n = buffer.getNumSamples();
        if (scratch.getNumSamples() < n) scratch.setSize (std::max (2, plugin->getTotalNumOutputChannels()), n, false, false, true);
        juce::AudioBuffer<float> view (scratch.getArrayOfWritePointers(), scratch.getNumChannels(), n);
        view.clear();
        plugin->processBlock (view, midi);
        buffer.clear();
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.copyFrom (ch, 0, view, std::min (ch, view.getNumChannels() - 1), 0, n);
    }

    juce::String name() const override { return plugin->getName(); }

private:
    std::unique_ptr<juce::AudioPluginInstance> plugin;
    juce::AudioBuffer<float> scratch;
};
} // namespace

std::unique_ptr<SynthBackend> createSynth (bool preferBuiltIn, juce::String& description)
{
   #if JUCE_PLUGINHOST_AU && JUCE_MAC
    if (! preferBuiltIn)
    {
        juce::AudioUnitPluginFormat au;
        juce::OwnedArray<juce::PluginDescription> found;
        au.findAllTypesForFile (found, "AudioUnit:Synths/aumu,dls ,appl");
        if (! found.isEmpty())
        {
            juce::String error;
            if (auto instance = au.createInstanceFromDescription (*found.getFirst(), 44100.0, 512, error))
            {
                description = "Apple General MIDI (built into macOS)";
                return std::make_unique<PluginSynth> (std::move (instance));
            }
        }
    }
   #else
    juce::ignoreUnused (preferBuiltIn);
   #endif
    description = "Noterator's built-in synth";
    return std::make_unique<BasicSynth>();
}

//==============================================================================

SynthRack::SynthRack (bool preferBuiltIn) : builtIn (preferBuiltIn)
{
    synths.push_back (createSynth (builtIn, desc));
}

SynthRack::~SynthRack() { release(); }

void SynthRack::ensureBanks (int banks)
{
    banks = std::clamp (banks, 1, maxBanks);
    while (static_cast<int> (synths.size()) < banks)
    {
        juce::String ignored;
        auto s = createSynth (builtIn, ignored);
        if (block > 0) s->prepare (rate, block);
        synths.push_back (std::move (s));
    }
}

void SynthRack::prepare (double sampleRate, int blockSize)
{
    rate = sampleRate;
    block = blockSize;
    scratch.setSize (2, std::max (1, blockSize) * 2);
    for (auto& s : synths) s->prepare (rate, block);
}

void SynthRack::release()
{
    if (block <= 0) return;
    for (auto& s : synths) s->release();
    block = 0;
}

void SynthRack::render (juce::AudioBuffer<float>& buffer, BankMidi& midi)
{
    const int n = buffer.getNumSamples();
    synths.front()->render (buffer, midi[0]);
    for (size_t b = 1; b < synths.size(); ++b)
    {
        if (scratch.getNumSamples() < n) scratch.setSize (2, n, false, false, true);   // only if the device grew its block
        juce::AudioBuffer<float> view (scratch.getArrayOfWritePointers(), std::min (2, buffer.getNumChannels()), n);
        view.clear();
        synths[b]->render (view, midi[b]);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.addFrom (ch, 0, view, std::min (ch, view.getNumChannels() - 1), 0, n);
    }
}

int banksFor (const Score& score) { return banksForParts (channelsForParts (score)); }

std::shared_ptr<const Sequence> makeSequence (const Score& score, Tick from, Tick to, const PerformOptions& base)
{
    auto seq = std::make_shared<Sequence>();
    PerformOptions o = base;
    o.from = from;
    o.to = to;
    const double origin = score.secondsAt (from);
    for (const auto& e : perform (score, o))
    {
        const int bank = std::min (e.channel / channelsPerBank, maxBanks - 1);
        const int ch = e.channel % channelsPerBank + 1;
        seq->banks = std::max (seq->banks, bank + 1);
        juce::MidiMessage m;
        switch (e.type)
        {
            case PlayEvent::noteOn:     m = juce::MidiMessage::noteOn (ch, e.data1, static_cast<juce::uint8> (juce::jlimit (1, 127, e.data2))); break;
            case PlayEvent::noteOff:    m = juce::MidiMessage::noteOff (ch, e.data1); break;
            case PlayEvent::controller: m = juce::MidiMessage::controllerEvent (ch, e.data1, e.data2); break;
            case PlayEvent::program:    m = juce::MidiMessage::programChange (ch, e.data1); break;
        }
        const double t = score.secondsAt (e.at + from) - origin;
        seq->events.push_back ({ std::max (0.0, t), m, e.noteId, e.part, bank });
        seq->length = std::max (seq->length, t);
    }
    seq->from = from;
    return seq;
}

//==============================================================================

AudioEngine::AudioEngine()
{
    queue.reserve (1024);
    pending.reserve (1024);
    sounding.reserve (512);
    for (auto& m : blockMidi) m.ensureSize (8192);
    synth = std::make_unique<SynthRack> (false);
    deviceManager.initialiseWithDefaultDevices (0, 2);
    deviceManager.addAudioCallback (this);
    openMidiInputs();
}

AudioEngine::~AudioEngine()
{
    deviceManager.removeAudioCallback (this);
    for (const auto& d : juce::MidiInput::getAvailableDevices())
        deviceManager.removeMidiInputDeviceCallback (d.identifier, this);
}

void AudioEngine::openMidiInputs()
{
    // Every keyboard plugged in is listened to: there is no setting to forget.
    for (const auto& d : juce::MidiInput::getAvailableDevices())
    {
        deviceManager.setMidiInputDeviceEnabled (d.identifier, true);
        deviceManager.addMidiInputDeviceCallback (d.identifier, this);
    }
}

void AudioEngine::useBuiltInSynth (bool builtIn)
{
    builtInOnly = builtIn;
    auto fresh = std::make_unique<SynthRack> (builtIn);
    fresh->ensureBanks (synth != nullptr ? synth->banks() : 1);
    if (auto* dev = deviceManager.getCurrentAudioDevice())
        fresh->prepare (dev->getCurrentSampleRate(), dev->getCurrentBufferSizeSamples());
    {
        const juce::ScopedLock sl (synthLock);
        std::swap (synth, fresh);
        previewProgram = liveProgram = -1;
    }
}

void AudioEngine::ensureBanks (int banks)
{
    if (synth == nullptr || synth->banks() >= banks) return;
    // Made here, on the message thread, then added while the audio thread
    // is kept out for a moment.
    const juce::ScopedLock sl (synthLock);
    synth->ensureBanks (banks);
}

void AudioEngine::play (const Score& score, Tick from, Tick to)
{
    PerformOptions o;
    // General MIDI reads CC1 as vibrato: the built-in sounds get expression
    // and volume from AutoCC, a sample library gets all four (decision 0008).
    o.autoCCControllers = { 7, 11 };
    auto seq = makeSequence (score, from, to, o);
    ensureBanks (seq->banks);
    stop();
    std::atomic_store (&sequence, seq);
    positionSeconds.store (0.0);
    sequenceChanged.store (true);
    stopRequested.store (false);
    playing.store (true);
}

void AudioEngine::update (const Score& score)
{
    if (! playing.load()) return;
    auto current = std::atomic_load (&sequence);
    if (current == nullptr) return;
    PerformOptions o;
    o.autoCCControllers = { 7, 11 };
    auto seq = makeSequence (score, current->from, -1, o);
    ensureBanks (seq->banks);
    std::atomic_store (&sequence, seq);
    sequenceChanged.store (true);
}

void AudioEngine::stop()
{
    if (playing.load()) stopRequested.store (true);
    playing.store (false);
    allNotesOff();
    const juce::SpinLock::ScopedLockType sl (soundingLock);
    sounding.clear();
}

Tick AudioEngine::playheadTick() const
{
    auto seq = std::atomic_load (&sequence);
    if (seq == nullptr) return 0;
    return seq->from;   // the window adds the elapsed time itself through the tempo map
}

std::vector<uint32_t> AudioEngine::soundingNotes() const
{
    const juce::SpinLock::ScopedLockType sl (soundingLock);
    return sounding;
}

void AudioEngine::schedule (double delaySeconds, const juce::MidiMessage& m)
{
    const juce::ScopedLock sl (queueLock);
    queue.push_back ({ delaySeconds, m });
}

void AudioEngine::preview (const std::vector<int>& pitches, const std::string& instrumentId, double seconds, int velocity)
{
    const auto& inst = instrumentById (instrumentId);
    const int ch = inst.drums ? 10 : 16;
    if (! inst.drums && inst.program != previewProgram)
    {
        schedule (0.0, juce::MidiMessage::programChange (ch, inst.program));
        schedule (0.0, juce::MidiMessage::controllerEvent (ch, 7, 110));
        schedule (0.0, juce::MidiMessage::controllerEvent (ch, 11, 120));
        previewProgram = inst.program;
    }
    for (int p : pitches)
    {
        schedule (0.0, juce::MidiMessage::noteOn (ch, p, static_cast<juce::uint8> (juce::jlimit (1, 127, velocity))));
        schedule (seconds, juce::MidiMessage::noteOff (ch, p));
    }
}

void AudioEngine::previewSequence (const std::vector<int>& pitches, const std::string& instrumentId, double step)
{
    preview ({}, instrumentId);   // the program change, if it needs one
    const auto& inst = instrumentById (instrumentId);
    const int ch = inst.drums ? 10 : 16;
    double at = 0;
    for (int p : pitches)
    {
        schedule (at, juce::MidiMessage::noteOn (ch, p, static_cast<juce::uint8> (90)));
        schedule (at + step * 0.95, juce::MidiMessage::noteOff (ch, p));
        at += step;
    }
}

void AudioEngine::setLiveInstrument (const std::string& instrumentId)
{
    const auto& inst = instrumentById (instrumentId);
    if (inst.drums || inst.program == liveProgram) return;
    schedule (0.0, juce::MidiMessage::programChange (15, inst.program));
    schedule (0.0, juce::MidiMessage::controllerEvent (15, 7, 110));
    liveProgram = inst.program;
}

void AudioEngine::allNotesOff()
{
    for (int ch = 1; ch <= 16; ++ch)
    {
        schedule (0.0, juce::MidiMessage::allNotesOff (ch));
        schedule (0.0, juce::MidiMessage::allSoundOff (ch));
    }
}

void AudioEngine::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& m)
{
    if (! (m.isNoteOnOrOff() || m.isController() || m.isPitchWheel())) return;
    // Heard at once on the live channel, whatever channel the keyboard sends.
    auto live = m;
    live.setChannel (15);
    midiCollector.addMessageToQueue (live);
    if (m.isNoteOnOrOff() && onMidiInput)
    {
        juce::MidiMessage copy (m);
        juce::MessageManager::callAsync ([this, copy] { if (onMidiInput) onMidiInput (copy); });
    }
}

void AudioEngine::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    sampleRate = device->getCurrentSampleRate();
    midiCollector.reset (sampleRate);
    const juce::ScopedLock sl (synthLock);
    if (synth) synth->prepare (sampleRate, device->getCurrentBufferSizeSamples());
}

void AudioEngine::audioDeviceStopped()
{
    const juce::ScopedLock sl (synthLock);
    if (synth) synth->release();
}

void AudioEngine::audioDeviceIOCallbackWithContext (const float* const*, int, float* const* outputs, int numOuts,
                                                    int numSamples, const juce::AudioIODeviceCallbackContext&)
{
    for (int ch = 0; ch < numOuts; ++ch)
        if (outputs[ch] != nullptr) juce::FloatVectorOperations::clear (outputs[ch], numSamples);

    const juce::ScopedTryLock synthTry (synthLock);
    if (! synthTry.isLocked() || synth == nullptr) return;

    for (auto& m : blockMidi) m.clear();
    // Previews and the MIDI keyboard play on the first synth's own channels.
    auto& midi = blockMidi[0];
    midiCollector.removeNextBlockOfMessages (midi, numSamples);
    const double blockSeconds = numSamples / sampleRate;

    // Messages from the window: due now, or later (a preview's note-offs).
    {
        const juce::ScopedTryLock q (queueLock);
        if (q.isLocked())
        {
            for (auto& s : queue) pending.push_back ({ engineClock + s.at, s.message });
            queue.clear();
        }
    }
    for (size_t i = 0; i < pending.size();)
    {
        if (pending[i].at < engineClock + blockSeconds)
        {
            const int at = juce::jlimit (0, numSamples - 1, static_cast<int> ((pending[i].at - engineClock) * sampleRate));
            midi.addEvent (pending[i].message, at);
            pending[i] = pending.back();
            pending.pop_back();
        }
        else ++i;
    }

    // The score.
    if (stopRequested.exchange (false)) cursor = 0;
    auto seq = std::atomic_load (&sequence);
    if (playing.load() && seq != nullptr)
    {
        const double pos = positionSeconds.load();
        if (sequenceChanged.exchange (false))
        {
            // Start, or an edit while playing: find our place again and let
            // go of whatever was held, so nothing hangs.
            cursor = 0;
            while (cursor < seq->events.size() && seq->events[cursor].seconds < pos) ++cursor;
            for (auto& bank : blockMidi)
                for (int ch = 1; ch <= 16; ++ch) bank.addEvent (juce::MidiMessage::allNotesOff (ch), 0);
            // Programs and levels from the top, so they hold wherever play starts.
            for (const auto& e : seq->events)
            {
                if (e.seconds > 0.0) break;
                if (! e.message.isNoteOn()) blockMidi[static_cast<size_t> (e.bank)].addEvent (e.message, 0);
            }
            const juce::SpinLock::ScopedTryLockType sl (soundingLock);
            if (sl.isLocked()) sounding.clear();
        }
        const double end = pos + blockSeconds;
        bool changed = false;
        while (cursor < seq->events.size() && seq->events[cursor].seconds < end)
        {
            const auto& e = seq->events[cursor];
            const int at = juce::jlimit (0, numSamples - 1, static_cast<int> ((e.seconds - pos) * sampleRate));
            blockMidi[static_cast<size_t> (e.bank)].addEvent (e.message, at);
            if (e.noteId != 0)
            {
                const juce::SpinLock::ScopedTryLockType sl (soundingLock);
                if (sl.isLocked())
                {
                    if (e.message.isNoteOn()) { if (sounding.size() < sounding.capacity()) sounding.push_back (e.noteId); }
                    else
                        for (size_t k = 0; k < sounding.size(); ++k)
                            if (sounding[k] == e.noteId) { sounding[k] = sounding.back(); sounding.pop_back(); break; }
                    changed = true;
                }
            }
            ++cursor;
        }
        juce::ignoreUnused (changed);
        positionSeconds.store (end);
        if (cursor >= seq->events.size() && end > seq->length + 0.25)
        {
            playing.store (false);
            juce::MessageManager::callAsync ([this] { if (onPlaybackFinished) onPlaybackFinished(); });
        }
    }

    juce::AudioBuffer<float> buffer (outputs, std::max (1, std::min (numOuts, 2)), numSamples);
    synth->render (buffer, blockMidi);
    // A gentle ceiling, so a full orchestra does not clip.
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* d = buffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i) d[i] = std::tanh (d[i] * 0.9f);
    }
    for (int ch = 2; ch < numOuts; ++ch)
        if (outputs[ch] != nullptr) juce::FloatVectorOperations::copy (outputs[ch], outputs[ch % 2], numSamples);
    engineClock += blockSeconds;
}

} // namespace nt
