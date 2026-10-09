#include "Exporter.h"

#include "Edit.h"
#include "MidiFile.h"
#include "MusicXml.h"

namespace nt
{

bool exportMidi (const Score& score, ExportRange range, bool withAutoCC, const juce::File& file, juce::String& error)
{
    PerformOptions o;
    o.from = range.from;
    o.to = range.to;
    o.autoCC = withAutoCC;
    o.honourMuteAndSolo = false;   // a file holds every part; muting is for listening
    const auto bytes = writeMidiFile (score, o);
    file.deleteFile();
    if (! file.replaceWithData (bytes.data(), bytes.size()))
    {
        error = "Could not write " + file.getFullPathName();
        return false;
    }
    return true;
}

bool exportMusicXml (const Score& score, ExportRange range, const juce::File& file, juce::String& error)
{
    Score slice = score;
    if (range.from > 0 || range.to >= 0)
    {
        const int first = score.barAt (range.from);
        const int last = range.to < 0 ? score.bars - 1 : std::max (first, score.barAt (range.to - 1));
        if (last + 1 < slice.bars) deleteBars (slice, last + 1, slice.bars - last - 1);
        if (first > 0) deleteBars (slice, 0, first);
    }
    if (! file.replaceWithText (writeMusicXml (slice), false, false, "\n"))
    {
        error = "Could not write " + file.getFullPathName();
        return false;
    }
    return true;
}

bool renderAudio (const Score& score, ExportRange range, SynthRack& synth, const juce::File& file,
                  const std::function<bool (double)>& progress, juce::String& error)
{
    constexpr double rate = 48000.0;
    constexpr int block = 512;
    PerformOptions o;
    o.autoCCControllers = { 7, 11 };
    const auto seq = makeSequence (score, range.from, range.to, o);
    const double tail = 2.5;
    const auto total = static_cast<juce::int64> ((seq->length + tail) * rate);

    file.deleteFile();
    std::unique_ptr<juce::OutputStream> stream (file.createOutputStream().release());
    if (stream == nullptr) { error = "Could not write " + file.getFullPathName(); return false; }
    juce::WavAudioFormat wav;
    auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions().withSampleRate (rate).withNumChannels (2).withBitsPerSample (24));
    if (writer == nullptr) { error = "Could not start a WAV file."; return false; }

    synth.prepare (rate, block);
    juce::AudioBuffer<float> buffer (2, block);
    // The rack must already have a synth for every bank: an Audio Unit is
    // made on the message thread, and this runs on its own (decision 0023).
    if (synth.banks() < seq->banks) { error = "Not enough synths were made for this score."; return false; }
    BankMidi midi;
    // Silence on every channel to start, whatever the synths were doing.
    for (auto& bank : midi)
        for (int ch = 1; ch <= 16; ++ch) bank.addEvent (juce::MidiMessage::allNotesOff (ch), 0);
    size_t cursor = 0;
    for (juce::int64 done = 0; done < total; done += block)
    {
        const double t0 = static_cast<double> (done) / rate, t1 = static_cast<double> (done + block) / rate;
        while (cursor < seq->events.size() && seq->events[cursor].seconds < t1)
        {
            const auto& e = seq->events[cursor++];
            midi[static_cast<size_t> (e.bank)].addEvent (e.message, juce::jlimit (0, block - 1, static_cast<int> ((e.seconds - t0) * rate)));
        }
        synth.render (buffer, midi);
        for (auto& bank : midi) bank.clear();
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            for (int i = 0; i < block; ++i) d[i] = std::tanh (d[i] * 0.9f);
        }
        const int n = static_cast<int> (std::min<juce::int64> (block, total - done));
        writer->writeFromAudioSampleBuffer (buffer, 0, n);
        if (progress && ! progress (static_cast<double> (done) / static_cast<double> (total)))
        {
            writer.reset();
            file.deleteFile();
            error = "Cancelled.";
            synth.release();
            return false;
        }
    }
    synth.release();
    return true;
}

} // namespace nt
