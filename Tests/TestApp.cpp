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
    return juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("miderator-test-" + name);
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

// Quarter notes one after another, drawn as the piano roll draws them.
void drawLine (Controller& c, size_t partIndex, Tick from, std::initializer_list<int> pitches)
{
    const auto id = c.score.parts[partIndex].id;
    Tick at = from;
    for (int p : pitches) { c.drawNoteAt (id, at, p, PPQ); at += PPQ; }
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
    drawLine (c, 0, 0, { 72, 74, 76, 77 });                           // bar 1, first violin
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
    drawLine (c, 0, 4 * PPQ, { 60, 62, 64, 60 });
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
    drawLine (c, 0, 0, { 72, 74, 76, 77, 76, 74, 72 });
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
    drawLine (c, c.score.parts.size() - 1, 0, { 36, 40, 36, 40 });
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
    c.drawNoteAt (c.score.parts[0].id, 0, 60);
    c.drawNoteAt (c.score.parts[0].id, 0, 64);
    juce::String error;
    const auto f = temp ("song.miderator");
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
    drawLine (c, 0, 4 * PPQ, { 60, 62, 64, 65 });
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

TEST ("app: notes drawn, dragged, copied and stretched on the roll, each one undo")
{
    Controller c (audio());
    c.newScore ("Piano");
    const auto piano = c.score.parts[0].id;
    c.grid.base = PPQ / 4;
    const auto id = c.drawNoteAt (piano, PPQ, 60);
    CHECK (id != 0);
    CHECK_EQ (c.score.parts[0].notes.size(), size_t (1));
    CHECK_EQ (c.score.parts[0].notes[0].length, PPQ / 4);   // a drawn note is a grid step
    CHECK (c.selection == Selection { id });

    c.dragSelection (PPQ, 7, false);                         // a beat later, a fifth up
    CHECK_EQ (c.score.parts[0].notes[0].start, 2 * PPQ);
    CHECK_EQ (c.score.parts[0].notes[0].pitch, 67);
    c.dragSelection (-4 * PPQ, 0, false);                    // past the start: refused
    CHECK_EQ (c.score.parts[0].notes[0].start, 2 * PPQ);
    CHECK_EQ (c.status, juce::String ("That would go past the start, or the ends of the MIDI range."));

    c.dragSelection (PPQ, 0, true);                          // Alt-drag: a copy
    CHECK_EQ (c.score.parts[0].notes.size(), size_t (2));
    CHECK_EQ (c.selection.size(), size_t (1));
    CHECK (c.selection.count (id) == 0);                     // the copy is what is selected

    c.stretchSelection (PPQ / 2, false);
    CHECK_EQ (c.score.parts[0].notes[1].length, PPQ * 3 / 4);
    c.stretchSelection (-4 * PPQ, false);                    // never shorter than the shortest
    CHECK_EQ (c.score.parts[0].notes[1].length, PPQ / 8);

    c.undo(); c.undo(); c.undo();
    CHECK_EQ (c.score.parts[0].notes.size(), size_t (1));
    c.undo(); c.undo();
    CHECK (c.score.parts[0].notes.empty());
}

TEST ("app: quantise, duplicate and velocities through the controller")
{
    Controller c (audio());
    c.newScore ("Piano");
    const auto piano = c.score.parts[0].id;
    c.edit ("Played in", [piano] (Score& s)
    {
        for (Tick at : { Tick (30), Tick (PPQ - 50), Tick (2 * PPQ + 70) })
        {
            Note n;
            n.start = at;
            n.length = PPQ - 40;
            n.pitch = 60;
            n.id = s.newId();
            s.partById (piano)->notes.push_back (n);
        }
    });
    c.setCaret (piano, 0);
    c.select ({});
    c.setGrid (PPQ, false);
    c.quantiseSelection();                                    // nothing selected: the whole part
    const auto& n = c.score.parts[0].notes;
    CHECK_EQ (n.size(), size_t (3));
    CHECK_EQ (n[0].start, Tick (0));
    CHECK_EQ (n[1].start, PPQ);
    CHECK_EQ (n[2].start, 2 * PPQ);
    CHECK_EQ (n[2].length, PPQ);

    Selection all;
    for (const auto& x : c.score.parts[0].notes) all.insert (x.id);
    c.select (all);
    c.duplicateSelection();                                   // straight after: three beats on
    CHECK_EQ (c.score.parts[0].notes.size(), size_t (6));
    CHECK_EQ (c.score.parts[0].notes[3].start, 3 * PPQ);
    CHECK_EQ (c.selection.size(), size_t (3));

    std::map<uint32_t, int> v;
    for (const auto x : c.selection) v[x] = 40;
    c.setVelocities (v);
    int soft = 0;
    for (const auto& x : c.score.parts[0].notes) if (x.velocity == 40) ++soft;
    CHECK_EQ (soft, 3);
}

TEST ("app: step input writes a grid step at the caret, and keys together make a chord")
{
    Controller c (audio());
    c.newScore ("Piano");
    c.setCaret (c.score.parts[0].id, PPQ + 10);
    c.setGrid (PPQ / 2, false);
    c.toggleStepInput();
    CHECK (c.stepInput);
    CHECK_EQ (c.caret, PPQ);                                  // onto the grid
    c.writePitch (60, false);
    c.writePitch (64, true);                                  // pressed with it
    c.writePitch (67, false);
    const auto& n = c.score.parts[0].notes;
    CHECK_EQ (n.size(), size_t (3));
    CHECK_EQ (n[0].start, PPQ);
    CHECK_EQ (n[1].start, PPQ);
    CHECK_EQ (n[1].pitch, 64);
    CHECK_EQ (n[2].start, PPQ + PPQ / 2);
    CHECK_EQ (n[2].length, PPQ / 2);
    CHECK_EQ (c.caret, 2 * PPQ);
}

TEST ("app: the warnings follow every edit")
{
    Controller c (audio());
    c.newScore ("String Quartet");
    const auto violin = c.score.parts[0].id;
    const auto low = c.drawNoteAt (violin, 0, 50);           // under the violin's G
    CHECK (c.warnings.count (low) == 1);
    CHECK (c.warnings.at (low).outOfRange);
    c.dragSelection (0, 17, false);                           // up to G4: fine
    CHECK (c.warnings.count (low) == 0);
}

TEST ("app: Space plays from bar 1, Shift+Space from the caret; Home and End go to the start and the end")
{
    Controller c (audio());
    c.newScore ("Piano");
    drawLine (c, 0, 4 * PPQ, { 60, 62, 64, 65 });                     // bar 2
    CHECK_EQ (c.musicEnd(), 8 * PPQ);                                 // the bar line after the last note
    c.setCaret (c.score.parts[0].id, 5 * PPQ);

    c.togglePlay (true);                                              // Space
    CHECK (c.audio.isPlaying());
    CHECK_EQ (c.audio.playheadTick(), Tick (0));
    c.togglePlay (true);                                              // Space again stops it
    CHECK (! c.audio.isPlaying());

    c.togglePlay (false);                                             // Shift+Space, or the Play button
    CHECK_EQ (c.audio.playheadTick(), 5 * PPQ);
    c.returnToStart();                                                // while playing: on from bar 1
    CHECK (c.audio.isPlaying());
    CHECK_EQ (c.audio.playheadTick(), Tick (0));
    CHECK_EQ (c.caret, Tick (0));

    c.skipToEnd();                                                    // stops, and the caret is at the end
    CHECK (! c.audio.isPlaying());
    CHECK_EQ (c.caret, 8 * PPQ);
    c.returnToStart();                                                // stopped: just the caret
    CHECK (! c.audio.isPlaying());
    CHECK_EQ (c.caret, Tick (0));

    Controller empty (audio());
    empty.newScore ("Piano");
    CHECK_EQ (empty.musicEnd(), empty.score.endTick());               // no notes: the end of the song
    c.stop();
}

TEST ("app: Generate Notes' chords into a violin come one note at a time; Blocks go in as they are")
{
    Controller c (audio());
    c.newScore ("String Quartet");
    const auto violin = c.score.parts[0].id;
    c.setCaret (violin, 0);
    auto ctx = c.generatorContext (false);
    c.lua.reset ("good-idea");
    for (const auto& st : c.lua.settings ("good-idea", ctx))
        for (size_t i = 0; i < st.names.size(); ++i)
            if ((st.id == "kind" && st.names[i] == "Phrase") || (st.id == "content" && st.names[i] == "Chords"))
                c.lua.set ("good-idea", st.id, static_cast<int> (i), ctx);
    const auto out = c.lua.generate ("good-idea", c.generatorContext (false), 3, 1);
    c.lua.reset ("good-idea");
    CHECK (! out.results.empty());
    if (out.results.empty()) return;
    CHECK (polyphonyOf (out.results.front().parts.front().notes) > 1);   // the idea itself is chords
    c.insertGenerated (out.results.front(), false, "good-idea");
    CHECK (! c.score.parts[0].notes.empty());
    CHECK_EQ (polyphonyOf (c.score.parts[0].notes), 1);
    CHECK (c.status.contains ("Violin I: top notes only"));

    // A chord block from the toolbox into the same violin: as it always was.
    Controller d (audio());
    d.newScore ("String Quartet");
    d.setCaret (d.score.parts[0].id, 0);
    d.lua.reset ("starting-blocks");
    const auto blocks = d.lua.generate ("starting-blocks", d.generatorContext (false), 1, 0);
    CHECK (! blocks.results.empty());
    if (blocks.results.empty()) return;
    d.insertGenerated (blocks.results.front(), false, "starting-blocks");
    int most = 0;
    for (const auto& p : d.score.parts) most = std::max (most, polyphonyOf (p.notes));
    CHECK (most > 1);
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
