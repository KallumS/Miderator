/*
    The app's own path, without a window: the controller, a generator into a
    part, MIDI export and back, and audio rendered through each synth. On the
    macOS runner this is what proves Apple's General MIDI synth loads and
    sounds - nothing else can, short of opening the app on a Mac.
*/

#include "Check.h"

#include "Controller.h"
#include "Exporter.h"
#include "GeneratorPanel.h"
#include "MidiFile.h"
#include "ScaleModel.h"

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
    c.selectRange (0, 3, 0, 0);                                       // bars 1-4 of the violin alone (0041)
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

TEST ("app: the File menu's Use the key it hears sets the key the Scale lane shows")
{
    Controller c (audio());
    c.newScore ("Piano");
    CHECK (! c.useHeardKey());                                        // nothing written yet
    CHECK (c.status.contains ("Nothing to hear"));
    CHECK_EQ (c.score.keyAtBar (0).root, 0);                          // still C

    // Two bars of E major, so the Scale lane names it.
    drawLine (c, 0, 0, { 64, 68, 71, 76, 66, 69, 73, 75 });
    drawLine (c, 0, 8 * PPQ, { 76, 75, 73, 71, 69, 68, 66, 64 });
    c.setCaret (c.score.parts[0].id, PPQ);
    CHECK (! c.keys.empty());
    CHECK (c.useHeardKey());
    const auto& k = c.score.keyAtBar (0);
    CHECK_EQ (std::string (scaleview::roots[static_cast<size_t> (k.root)].name), std::string ("E"));
    CHECK_EQ (k.scale, 0);                                            // Major
    c.undo();
    CHECK_EQ (c.score.keyAtBar (0).root, 0);                          // one undo step
}

TEST ("app: nothing chosen shares an idea across every part; one part's bars take all of it")
{
    // A phrase of a tune and chords.
    auto phrase = [] (Controller& c)
    {
        auto ctx = c.generatorContext (false);
        c.lua.reset ("good-idea");
        for (const auto& st : c.lua.settings ("good-idea", ctx))
            for (size_t i = 0; i < st.names.size(); ++i)
                if ((st.id == "kind" && st.names[i] == "Phrase") || (st.id == "content" && st.names[i] == "Both"))
                    c.lua.set ("good-idea", st.id, static_cast<int> (i), ctx);
        auto out = c.lua.generate ("good-idea", c.generatorContext (false), 2, 1);
        c.lua.reset ("good-idea");
        return out;
    };

    // Nothing chosen: every part of the quartet plays, from the caret's bar.
    Controller c (audio());
    c.newScore ("String Quartet");
    c.setCaret (c.score.parts[1].id, c.score.barStart (2) + PPQ);
    const auto out = phrase (c);
    CHECK (! out.results.empty());
    if (out.results.empty()) return;
    c.insertGenerated (out.results.front(), false, "good-idea");
    CHECK_EQ (c.score.parts.size(), size_t (4));
    for (const auto& p : c.score.parts)
    {
        CHECK (! p.notes.empty());
        CHECK_EQ (polyphonyOf (p.notes), 1);
        for (const auto& n : p.notes) CHECK (n.start >= c.score.barStart (2));
    }

    // One bar of one part chosen: one bar of it there, and nowhere else.
    Controller d (audio());
    d.newScore ("String Quartet");
    d.selectRange (1, 1, 2, 2);                                       // bar 2 of the viola
    CHECK_EQ (d.generatorContext (false).rangeBars, 1);
    const auto one = phrase (d);
    CHECK (! one.results.empty());
    if (one.results.empty()) return;
    d.insertGenerated (one.results.front(), false, "good-idea");
    CHECK_EQ (d.score.parts.size(), size_t (4));
    for (size_t i = 0; i < 4; ++i) CHECK_EQ (d.score.parts[i].notes.empty(), i != 2);
    for (const auto& n : d.score.parts[2].notes) CHECK (n.start >= d.score.barStart (1) && n.end() <= d.score.barStart (2));

    // Bars 1-4 of one part: all of the idea fills them, in that part alone.
    Controller e (audio());
    e.newScore ("String Quartet");
    e.selectRange (0, 3, 0, 0);
    const auto four = phrase (e);
    if (four.results.empty()) return;
    e.insertGenerated (four.results.front(), false, "good-idea");
    CHECK_EQ (e.score.parts.size(), size_t (4));
    CHECK (! e.score.parts[0].notes.empty());
    for (size_t i = 1; i < 4; ++i) CHECK (e.score.parts[i].notes.empty());
    for (const auto& n : e.score.parts[0].notes) CHECK (n.end() <= e.score.barStart (4));
}

TEST ("app: a single line goes into one part, with nothing chosen or bars of several parts chosen")
{
    auto motif = [] (Controller& c)
    {
        auto ctx = c.generatorContext (false);
        c.lua.reset ("good-idea");
        for (const auto& st : c.lua.settings ("good-idea", ctx))
            for (size_t i = 0; i < st.names.size(); ++i)
                if (st.id == "kind" && st.names[i] == "Motif") c.lua.set ("good-idea", st.id, static_cast<int> (i), ctx);
        auto out = c.lua.generate ("good-idea", c.generatorContext (false), 4, 1);
        c.lua.reset ("good-idea");
        return out;
    };
    auto withNotes = [] (const Controller& c)
    {
        std::vector<size_t> parts;
        for (size_t i = 0; i < c.score.parts.size(); ++i) if (! c.score.parts[i].notes.empty()) parts.push_back (i);
        return parts;
    };

    // Nothing chosen, the caret in the viola: the viola alone.
    Controller c (audio());
    c.newScore ("String Quartet");
    c.setCaret (c.score.parts[2].id, 0);
    const auto out = motif (c);
    CHECK (! out.results.empty());
    if (out.results.empty()) return;
    CHECK (isSingleLine (out.results.front()));
    c.insertGenerated (out.results.front(), false, "good-idea");
    CHECK (withNotes (c) == (std::vector<size_t> { 2 }));

    // Bars of all four chosen, the caret in the cello: the cello alone.
    Controller d (audio());
    d.newScore ("String Quartet");
    d.setCaret (d.score.parts[3].id, 0);
    d.selectRange (0, 1, 0, 3);
    d.setCaret (d.score.parts[3].id, 0);
    const auto two = motif (d);
    if (two.results.empty()) return;
    d.insertGenerated (two.results.front(), false, "good-idea");
    CHECK (withNotes (d) == (std::vector<size_t> { 3 }));
    for (const auto& n : d.score.parts[3].notes) CHECK (n.end() <= d.score.barStart (2));
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

TEST ("app: an audition plays every note on a piano, Generate Notes in place and Blocks alone")
{
    Controller c (audio());
    c.newScore ("String Quartet");
    const auto violin = c.score.parts[0].id;
    c.setCaret (violin, 0);
    c.selectRange (0, 3, 0, 0);                                       // bars 1-4 of the violin alone
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
    const auto& idea = out.results.front();
    Tick from = 0, to = 0;
    const auto heard = c.auditionScore (idea, false, "good-idea", from, to);
    CHECK_EQ (from, Tick (0));
    CHECK_EQ (to, c.score.barStart (4));
    int pianos = 0, most = 0;
    for (const auto& p : heard.parts)
    {
        if (p.instrument == "pno") { ++pianos; most = std::max (most, polyphonyOf (p.notes)); }
        else for (const auto& n : p.notes) CHECK (n.end() <= from || n.start >= to);   // the parts it goes to: silent
    }
    CHECK_EQ (pianos, 1);
    CHECK_EQ (most, polyphonyOf (idea.parts.front().notes));          // the chords whole, not one note

    // A chord block from the toolbox, the caret on the violin: a piano, all of it.
    c.select ({});
    c.lua.reset ("starting-blocks");
    const auto blocks = c.lua.generate ("starting-blocks", c.generatorContext (false), 1, 0);
    CHECK (! blocks.results.empty());
    if (blocks.results.empty()) return;
    const auto alone = c.auditionAlone (blocks.results.front());
    CHECK_EQ (alone.parts.size(), size_t (1));
    if (alone.parts.empty()) return;
    CHECK_EQ (alone.parts[0].instrument, std::string ("pno"));
    CHECK (polyphonyOf (alone.parts[0].notes) > 1);
}

TEST ("app: clicking a part's name lets go of bars chosen in other parts, so an idea goes to it")
{
    Controller c (audio());
    c.newScore ("String Quartet");
    const auto violin = c.score.parts[0].id, viola = c.score.parts[2].id;
    GeneratedResult tune;
    tune.length = 4 * PPQ;
    GeneratedPart melody;
    melody.name = "Melody";
    for (int i = 0; i < 4; ++i) { Note n; n.start = i * PPQ; n.length = PPQ; n.pitch = 64 + i; melody.notes.push_back (n); }
    tune.parts = { melody };

    c.selectRange (0, 1, 0, 0);                         // bars 1-2 of Violin I
    c.insertGenerated (tune, false, "good-idea");
    const auto violinNotes = c.score.partById (violin)->notes.size();
    CHECK_EQ (violinNotes, size_t (4));

    c.choosePart (viola);                               // Viola's name clicked
    CHECK (! c.range.active());
    CHECK_EQ (c.caretPart, viola);
    c.insertGenerated (tune, false, "good-idea");
    CHECK_EQ (c.score.partById (viola)->notes.size(), size_t (4));
    CHECK_EQ (c.score.partById (violin)->notes.size(), violinNotes);

    // Bars chosen that include the part clicked stay chosen.
    c.selectRange (0, 1, 0, 3);
    c.choosePart (viola);
    CHECK (c.range.active());
}

TEST ("app: Escape lets go of everything, the caret's part too, so an idea goes to every part")
{
    Controller c (audio());
    c.newScore ("String Quartet");
    const auto violin = c.score.parts[0].id, viola = c.score.parts[2].id;
    GeneratedResult tune;
    tune.length = 4 * PPQ;
    GeneratedPart melody;
    melody.name = "Melody";
    for (int i = 0; i < 4; ++i) { Note n; n.start = i * PPQ; n.length = PPQ; n.pitch = 64 + i; melody.notes.push_back (n); }
    tune.parts = { melody };
    GeneratedResult chords;
    chords.length = 4 * PPQ;
    GeneratedPart harmony;
    harmony.name = "Chords";
    for (int i = 0; i < 2; ++i)
        for (int pitch : { 60, 64, 67, 72 }) { Note n; n.start = i * 2 * PPQ; n.length = 2 * PPQ; n.pitch = pitch; harmony.notes.push_back (n); }
    chords.parts = { harmony };

    // The viola chosen, bars chosen across it, notes selected.
    c.setCaret (viola, 0);
    c.selectRange (0, 1, 1, 2);
    c.setCaret (viola, 0);
    CHECK (c.range.active());
    CHECK_EQ (c.lineTarget(), viola);

    c.letGoOfEverything();                              // Escape
    CHECK (! c.range.active());
    CHECK (c.selection.empty());
    CHECK (c.noPartChosen);
    CHECK_EQ (c.lineTarget(), violin);                  // a single line: the top part

    c.insertGenerated (tune, false, "good-idea");
    CHECK_EQ (c.score.partById (violin)->notes.size(), size_t (4));
    CHECK (c.score.partById (viola)->notes.empty());
    CHECK (c.noPartChosen);                             // still nothing chosen for the next idea
    c.setCaret (c.caretPart, 4 * PPQ);                  // the caret moved along the ruler: still nothing
    CHECK (c.noPartChosen);

    c.insertGenerated (chords, false, "good-idea");     // chords: every part
    for (const auto& p : c.score.parts) CHECK (! p.notes.empty());
    CHECK (c.noPartChosen);

    // Choosing a part again ends it.
    c.choosePart (viola);
    CHECK (! c.noPartChosen);
    CHECK_EQ (c.lineTarget(), viola);
    c.letGoOfEverything();
    c.caretToPart (1);
    CHECK (! c.noPartChosen);
}

TEST ("app: Undo brings back the ideas a Generate replaced, and Redo the new ones, in step with edits")
{
    Controller c (audio());
    c.newScore ("String Quartet");
    GeneratorPanel panel (c);
    juce::TextButton* generate = nullptr;
    juce::ListBox* list = nullptr;
    for (auto* child : panel.getChildren())
    {
        if (auto* b = dynamic_cast<juce::TextButton*> (child); b != nullptr && b->getButtonText() == "Generate") generate = b;
        if (auto* l = dynamic_cast<juce::ListBox*> (child)) list = l;
        // The tab chooses Generate Notes on its own a moment after it opens.
        if (auto* menu = dynamic_cast<juce::ComboBox*> (child); menu != nullptr && menu->onChange) menu->onChange();
    }
    CHECK (generate != nullptr && list != nullptr);
    if (generate == nullptr || list == nullptr) return;
    // What the list shows, row by row.
    auto shown = [list]
    {
        juce::StringArray rows;
        auto* model = list->getListBoxModel();
        for (int i = 0; i < model->getNumRows(); ++i) rows.add (model->getTooltipForRow (i));
        return rows.joinIntoString ("|");
    };
    auto notes = [&c] { return c.score.parts[0].notes.size(); };

    generate->onClick();
    const auto first = shown();
    CHECK (first.isNotEmpty());
    list->selectRow (2);                                // the idea liked
    c.edit ("A note", [] (Score& s) { Note n; n.start = 0; n.length = PPQ; n.pitch = 72; s.parts[0].notes.push_back (n); });
    CHECK_EQ (notes(), size_t (1));
    generate->onClick();                                // one Generate too many
    const auto second = shown();
    CHECK (second != first);

    c.undo();                                           // the ideas come back, the note stays
    CHECK (shown() == first);
    CHECK_EQ (list->getSelectedRow(), 2);
    CHECK_EQ (notes(), size_t (1));
    c.undo();                                           // then the note goes
    CHECK_EQ (notes(), size_t (0));
    CHECK (shown() == first);
    c.redo();
    CHECK_EQ (notes(), size_t (1));
    c.redo();
    CHECK (shown() == second);

    // A Generate after an Undo drops what Redo had.
    c.undo();
    CHECK (shown() == first);
    generate->onClick();
    const auto third = shown();
    CHECK (third != first && third != second);
    CHECK (! c.canRedo());
    c.undo();
    CHECK (shown() == first);
    c.undo();
    c.undo();                                           // back past the first Generate: an empty list
    CHECK (shown().isEmpty());
    CHECK (! c.canUndo());
}
