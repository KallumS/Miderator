/*
    The app's own path, without a window: the controller, a generator into a
    part, MIDI export and back, and audio rendered through each synth. On the
    macOS runner this is what proves Apple's General MIDI synth loads and
    sounds - nothing else can, short of opening the app on a Mac.
*/

#include "Check.h"

#include "Controller.h"
#include "Exporter.h"
#include "MidiFile.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <algorithm>

using namespace nt;

namespace
{
juce::File temp (const juce::String& name)
{
    return juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("noterator-test-" + name);
}

double rmsOf (const juce::File& wav)
{
    juce::WavAudioFormat format;
    std::unique_ptr<juce::AudioFormatReader> reader (format.createReaderFor (wav.createInputStream().release(), true));
    if (reader == nullptr) return -1;
    juce::AudioBuffer<float> buf (static_cast<int> (reader->numChannels), static_cast<int> (reader->lengthInSamples));
    reader->read (&buf, 0, buf.getNumSamples(), 0, true, true);
    return buf.getRMSLevel (0, 0, buf.getNumSamples());
}

AudioEngine& audio()
{
    static AudioEngine engine;
    return engine;
}
} // namespace

TEST ("app: a generator writes into a viola part through the controller")
{
    Controller c (audio());
    c.newScore ("String Quartet");
    CHECK_EQ (c.score.parts.size(), size_t (4));
    c.setCaret (c.score.parts[2].id, 0);
    const auto out = c.lua.generate ("midi-catalogue", c.generatorContext (false), 1, 3);
    CHECK (! out.results.empty());
    if (out.results.empty()) return;
    c.insertGenerated (out.results.front(), false, "midi-catalogue");
    CHECK (! c.score.parts[2].notes.empty());
    CHECK (! c.selection.empty());
    c.undo();
    CHECK (c.score.parts[2].notes.empty());
    c.redo();
    CHECK (! c.score.parts[2].notes.empty());
}

TEST ("app: chosen bars are filled by Good Idea, across the parts chosen")
{
    Controller c (audio());
    c.newScore ("String Quartet");
    c.setBars (8);
    c.setCaret (c.score.parts[0].id, 0);
    for (int letter : { 0, 2, 4, 5 }) c.typeLetter (letter, false);   // bar 1, first violin
    c.selectRange (1, 4, 0, 3);                                       // bars 2-5, all four
    CHECK (c.range.active());
    CHECK_EQ (c.rangeText(), juce::String ("Bars 2-5, Violin I to Cello"));
    const auto ctx = c.generatorContext (false);
    CHECK_EQ (ctx.rangeBars, 4);
    const auto out = c.lua.generate ("good-idea", ctx, 7, 3);
    CHECK (! out.results.empty());
    if (out.results.empty()) return;
    c.insertGenerated (out.results.front(), false, "good-idea");
    // Nothing outside the bars, nothing new beyond the four parts, and the
    // bars stay chosen with the new music selected.
    CHECK_EQ (c.score.parts.size(), size_t (4));
    const Tick from = c.score.barStart (1), to = c.score.barStart (5);
    int inside = 0;
    for (const auto& p : c.score.parts)
        for (const auto& n : p.notes)
        {
            if (n.start < from) continue;
            CHECK (n.start < to);
            CHECK (n.end() <= to);
            ++inside;
        }
    CHECK (inside > 0);
    CHECK_EQ (c.score.parts[0].notes.size() - static_cast<size_t> (std::count_if (c.score.parts[0].notes.begin(), c.score.parts[0].notes.end(),
                                                                                   [&] (const Note& n) { return n.start >= from; })),
              size_t (4));
    CHECK (c.range.active());
    CHECK_EQ (static_cast<int> (c.selection.size()), inside);
    c.select ({});
    CHECK (! c.range.active());
}

TEST ("app: blocks go where the caret is, one after another")
{
    Controller c (audio());
    c.newScore ("Piano");
    c.setCaret (c.score.parts[0].id, PPQ);   // beat 2, not the bar's start
    c.lua.reset ("starting-blocks");
    c.lua.set ("starting-blocks", "cat", 1, c.generatorContext (false));   // Arpeggio
    const auto out = c.lua.generate ("starting-blocks", c.generatorContext (false), 1, 0);
    CHECK_EQ (out.results.size(), size_t (7));
    if (out.results.size() < 5) return;
    c.insertGenerated (out.results[0], false, "starting-blocks");
    CHECK_EQ (c.caret, PPQ + out.results[0].length);
    c.insertGenerated (out.results[4], false, "starting-blocks");
    std::vector<std::pair<Tick, int>> got;
    for (const auto& n : c.score.parts[0].notes) got.push_back ({ n.start, n.pitch % 12 });
    std::sort (got.begin(), got.end());
    CHECK_EQ (got.size(), size_t (6));
    if (got.size() == 6)
    {
        CHECK_EQ (got[0].first, PPQ);
        CHECK_EQ (got[0].second, 0);                                // C, the I
        CHECK_EQ (got[3].first, PPQ + out.results[0].length);
        CHECK_EQ (got[3].second, 7);                                // G, the V
    }
    c.lua.reset ("starting-blocks");
}

TEST ("app: selected bars export as MIDI and read back")
{
    Controller c (audio());
    c.newScore ("Piano");
    c.setCaret (c.score.parts[0].id, 4 * PPQ);
    for (int letter : { 0, 2, 4, 0 }) c.typeLetter (letter, false);
    juce::String error;
    const auto f = temp ("bars.mid");
    CHECK (exportMidi (c.score, { 4 * PPQ, 8 * PPQ }, true, f, error));
    juce::MemoryBlock mb;
    f.loadFileAsData (mb);
    const auto back = readMidiFile (std::vector<uint8_t> (static_cast<const uint8_t*> (mb.getData()),
                                                          static_cast<const uint8_t*> (mb.getData()) + mb.getSize()));
    CHECK (back.ok);
    CHECK_EQ (back.score.parts.size(), size_t (1));
    if (! back.score.parts.empty())
    {
        CHECK_EQ (back.score.parts[0].notes.size(), size_t (4));
        CHECK_EQ (back.score.parts[0].notes[0].start, Tick (0));
    }
    f.deleteFile();
}

TEST ("app: audio renders, and is not silent, through both synths")
{
    Controller c (audio());
    c.newScore ("String Quartet");
    c.setCaret (c.score.parts[0].id, 0);
    for (int letter : { 0, 2, 4, 5, 4, 2, 0 }) c.typeLetter (letter, false);
    for (bool builtIn : { false, true })
    {
        juce::String error;
        SynthRack synth (builtIn);
        std::printf ("  (rendering through %s)\n", synth.description().toRawUTF8());
        const auto f = temp (builtIn ? "builtin.wav" : "system.wav");
        CHECK (renderAudio (c.score, { 0, 8 * PPQ }, synth, f, {}, error));
        const double rms = rmsOf (f);
        std::printf ("  (rms %.5f)\n", rms);
        CHECK (rms > 0.0005);
        f.deleteFile();
    }
}

TEST ("app: the last part of a full orchestra sounds, through a second synth")
{
    Controller c (audio());
    c.newScore ("Full Orchestra");
    CHECK_EQ (c.score.parts.size(), size_t (28));
    CHECK_EQ (banksFor (c.score), 2);
    // Only the double basses play: their channel is in the second bank.
    c.setCaret (c.score.parts.back().id, 0);
    for (int letter : { 0, 4, 0, 4 }) c.typeLetter (letter, false);
    for (bool builtIn : { false, true })
    {
        juce::String error;
        SynthRack one (builtIn);
        const auto f = temp ("orchestra.wav");
        // A rack with one synth refuses rather than playing them silently.
        CHECK (! renderAudio (c.score, { 0, 4 * PPQ }, one, f, {}, error));
        SynthRack rack (builtIn);
        rack.ensureBanks (banksFor (c.score));
        CHECK_EQ (rack.banks(), 2);
        CHECK (renderAudio (c.score, { 0, 4 * PPQ }, rack, f, {}, error));
        const double rms = rmsOf (f);
        std::printf ("  (double basses through %s, second synth: rms %.5f)\n", rack.description().toRawUTF8(), rms);
        CHECK (rms > 0.0005);
        f.deleteFile();
    }
}

TEST ("app: a project saves and opens again")
{
    Controller c (audio());
    c.newScore ("Band");
    c.setCaret (c.score.parts[0].id, 0);
    c.typeLetter (0, false);
    c.typeLetter (2, true);
    juce::String error;
    const auto f = temp ("song.noterator");
    CHECK (c.save (f, error));
    Controller d (audio());
    CHECK (d.load (f, error));
    CHECK_EQ (d.score.parts.size(), size_t (4));
    CHECK_EQ (d.score.parts[0].notes.size(), size_t (2));
    f.deleteFile();
}

TEST ("app: MusicXML out, bars at a time, and back in, plain and compressed")
{
    Controller c (audio());
    c.newScore ("Piano");
    c.setCaret (c.score.parts[0].id, 4 * PPQ);
    for (int letter : { 0, 2, 4, 5 }) c.typeLetter (letter, false);
    juce::String error;
    const auto f = temp ("bars.musicxml");
    CHECK (exportMusicXml (c.score, { 4 * PPQ, 8 * PPQ }, f, error));
    Score back;
    CHECK (Controller::readScoreFile (f, back, error));
    CHECK_EQ (back.bars, 1);
    CHECK_EQ (back.parts.size(), size_t (1));
    if (! back.parts.empty())
    {
        CHECK_EQ (back.parts[0].notes.size(), size_t (4));
        CHECK_EQ (back.parts[0].instrument, std::string ("pno"));
    }

    // The same file zipped as an .mxl, with the container that names it.
    const auto mxl = temp ("bars.mxl");
    mxl.deleteFile();
    {
        juce::ZipFile::Builder zip;
        const auto container = temp ("container.xml");
        container.replaceWithText ("<?xml version=\"1.0\"?><container><rootfiles><rootfile full-path=\"score/bars.musicxml\"/></rootfiles></container>");
        zip.addFile (container, 9, "META-INF/container.xml");
        zip.addFile (f, 9, "score/bars.musicxml");
        juce::FileOutputStream out (mxl);
        zip.writeToStream (out, nullptr);
        container.deleteFile();
    }
    Score zipped;
    CHECK (Controller::readScoreFile (mxl, zipped, error));
    CHECK_EQ (zipped.parts.size(), size_t (1));
    if (! zipped.parts.empty()) CHECK_EQ (zipped.parts[0].notes.size(), size_t (4));
    f.deleteFile();
    mxl.deleteFile();
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    int ran = 0;
    for (const auto& t : check::all())
    {
        if (argc > 1 && std::strstr (t.name, argv[1]) == nullptr) continue;
        check::current() = t.name;
        t.fn();
        ++ran;
    }
    if (check::failures() > 0) { std::printf ("%d failure(s) in %d tests\n", check::failures(), ran); return 1; }
    std::printf ("%d tests passed\n", ran);
    return 0;
}
