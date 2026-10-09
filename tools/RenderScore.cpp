/*
    NoteratorRender - draws a score into a PNG with no window, through the same
    renderer the app uses (decision 0010).

        NoteratorRender demo out.png                  the built-in test page
        NoteratorRender song.mid out.png [space] [light] [bars]
        NoteratorRender song.noterator out.png
        NoteratorRender demo out.musicxml             write MusicXML instead of a picture
        NoteratorRender song.musicxml out.png         draw a MusicXML file

    `space` is pixels per staff space (default 9), `light` draws black on
    white, `bars` stops after that many bars.
*/

#include "Edit.h"
#include "Engrave.h"
#include "Generators.h"
#include "LuaEngine.h"
#include "MidiFile.h"
#include "MusicXml.h"
#include "ScoreFile.h"
#include "ScoreRenderer.h"
#include "Spelling.h"

#include <juce_gui_basics/juce_gui_basics.h>

using namespace nt;

namespace
{
void note (Score& s, size_t part, double beat, double beats, int pitch, int voice = 0)
{
    Note n;
    n.start = static_cast<Tick> (beat * PPQ);
    n.length = static_cast<Tick> (beats * PPQ);
    n.pitch = pitch;
    n.voice = voice;
    n.id = s.newId();
    s.parts[part].notes.push_back (n);
}

// One page that exercises most of what the engraver does.
Score demo()
{
    Score s;
    s.title = "Engraving test";
    s.keys = { { 0, rootIndexByName ("D"), 0 }, { 4, rootIndexByName ("Bb"), 0 } };
    s.meters = { { 0, 4, 4 }, { 3, 6, 8 } };
    s.tempos = { { 0, 96 } };
    for (const char* id : { "fl", "cl", "vln1", "vla", "vc", "pno", "kit" })
    {
        Part p;
        p.id = s.newId();
        p.instrument = id;
        p.name = instrumentById (id).name;
        s.parts.push_back (p);
    }
    // Flute: eighths, sixteenths, a dotted rhythm, triplets, a tie over the bar.
    double t = 0;
    for (int p : { 74, 76, 78, 79, 81, 79, 78, 76 }) { note (s, 0, t, 0.5, p); t += 0.5; }
    for (int p : { 74, 73, 71, 73 }) { note (s, 0, t, 0.25, p); t += 0.25; }
    note (s, 0, t, 0.75, 74); t += 0.75; note (s, 0, t, 0.25, 76); t += 0.25;
    for (int p : { 78, 79, 81 }) { note (s, 0, t, 1.0 / 3, p); t += 1.0 / 3; }
    note (s, 0, t, 1, 83); t += 1;
    note (s, 0, t, 3, 81);
    // Clarinet: a sustained line with chromatic notes.
    note (s, 1, 0, 2, 62); note (s, 1, 2, 1, 63); note (s, 1, 3, 1, 65); note (s, 1, 4, 4, 66);
    // Violin: a high passage, one note out of the section's range.
    for (int i = 0; i < 8; ++i) note (s, 2, i * 0.5, 0.5, 86 + (i % 4) * 2);
    note (s, 2, 4, 2, 102); note (s, 2, 6, 2, 93);
    // Viola: alto clef, a chord a section cannot play.
    note (s, 3, 0, 4, 57); note (s, 3, 4, 2, 62); note (s, 3, 4, 2, 66); note (s, 3, 6, 2, 64);
    // Cello: whole notes, then a dotted half.
    note (s, 4, 0, 4, 38); note (s, 4, 4, 3, 45); note (s, 4, 7, 1, 50);
    // Piano: a melody over a held bass in the left hand, chords in the right.
    for (int i = 0; i < 4; ++i) { note (s, 5, i, 1, 74); note (s, 5, i, 1, 78); note (s, 5, i, 1, 81); }
    note (s, 5, 0, 4, 38); note (s, 5, 0, 4, 50);
    note (s, 5, 4, 2, 72); note (s, 5, 4, 2, 76); note (s, 5, 6, 2, 71); note (s, 5, 6, 2, 74);
    note (s, 5, 4, 4, 43);
    // Drums: hats, kick and snare.
    for (int i = 0; i < 16; ++i) note (s, 6, i * 0.5, 0.25, 42);
    for (int i : { 0, 2, 4, 6 }) note (s, 6, i, 0.5, 36);
    for (int i : { 1, 3, 5, 7 }) note (s, 6, i, 0.5, 38);
    // Bars 4 onward, in 6/8 and B flat: compound beaming.
    const double b4 = 12.0;
    for (int i = 0; i < 6; ++i) note (s, 0, b4 + i * 0.5, 0.5, 70 + (i % 3) * 2);
    note (s, 0, b4 + 3, 1.5, 74); note (s, 0, b4 + 4.5, 1.5, 72);
    note (s, 4, b4, 3, 46); note (s, 4, b4 + 3, 3, 41);
    s.bars = 6;
    s.normalise();
    return s;
}

// Ideas from the generators, dropped in the way the app drops them.
Score generated()
{
    Score s;
    s.title = "Generated";
    s.tempos = { { 0, 100 } };
    for (const char* id : { "vln1", "vla", "vc" })
    {
        Part p;
        p.id = s.newId();
        p.instrument = id;
        p.name = instrumentById (id).name;
        s.parts.push_back (p);
    }
    LuaEngine e;
    for (size_t i = 0; i < s.parts.size(); ++i)
    {
        auto ctx = contextFor (s, s.parts[i].id, 0, {});
        e.useKey ("midi-catalogue", 0, 0);
        const auto out = e.generate ("midi-catalogue", ctx, 1, 4);
        if (! out.results.empty()) insertResult (s, out.results[i % out.results.size()], s.parts[i].id, 0);
    }
    return s;
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc < 3)
    {
        std::printf ("usage: NoteratorRender <demo|generated|file.mid|file.noterator> <out.png> [space] [light] [bars]\n");
        return 2;
    }
    const juce::String in (argv[1]);
    const juce::File out = juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]);
    const float space = argc > 3 ? static_cast<float> (std::atof (argv[3])) : 9.0f;
    const bool light = argc > 4 && juce::String (argv[4]) == "light";
    const int maxBars = argc > 5 ? std::atoi (argv[5]) : 0;

    Score score;
    if (in == "demo") score = demo();
    else if (in == "generated") score = generated();
    else
    {
        const juce::File f = juce::File::getCurrentWorkingDirectory().getChildFile (in);
        juce::MemoryBlock mb;
        if (! f.loadFileAsData (mb)) { std::printf ("cannot read %s\n", argv[1]); return 1; }
        if (f.hasFileExtension ("musicxml;xml"))
        {
            auto r = readMusicXml (mb.toString().toStdString());
            if (! r.ok) { std::printf ("%s\n", r.error.c_str()); return 1; }
            score = r.score;
        }
        else if (f.hasFileExtension ("noterator"))
        {
            auto r = loadScore (mb.toString().toStdString());
            if (! r.ok) { std::printf ("%s\n", r.error.c_str()); return 1; }
            score = r.score;
        }
        else
        {
            std::vector<uint8_t> bytes (static_cast<const uint8_t*> (mb.getData()), static_cast<const uint8_t*> (mb.getData()) + mb.getSize());
            auto r = readMidiFile (bytes);
            if (! r.ok) { std::printf ("%s\n", r.error.c_str()); return 1; }
            score = r.score;
        }
    }
    if (maxBars > 0 && score.bars > maxBars) score.bars = maxBars;

    // A .musicxml name writes the score as MusicXML instead of drawing it.
    if (out.hasFileExtension ("musicxml"))
    {
        out.replaceWithText (writeMusicXml (score));
        std::printf ("wrote %s\n", out.getFullPathName().toRawUTF8());
        return 0;
    }

    const auto lay = engrave::layout (score);
    RenderStyle style;
    style.space = space;
    style.page = light ? theme::lightPage() : theme::darkPage();

    const float gutter = 150.0f;
    const int w = static_cast<int> (gutter + (lay.width + 4) * space);
    const int h = static_cast<int> ((lay.height + 6) * space);
    juce::Image img (juce::Image::ARGB, std::max (16, w), std::max (16, h), true);
    {
        juce::Graphics g (img);
        g.fillAll (style.page.paper);
        const juce::Point<float> origin (gutter, 4.0f * space);
        ScoreRenderer::drawNames (g, lay, score, style, gutter - 2.6f * space, origin.y, false);
        ScoreRenderer::draw (g, lay, score, style, origin, img.getBounds().toFloat());
    }
    out.deleteFile();
    juce::FileOutputStream stream (out);
    juce::PNGImageFormat png;
    if (! stream.openedOk() || ! png.writeImageToStream (img, stream)) { std::printf ("cannot write %s\n", argv[2]); return 1; }
    std::printf ("wrote %s (%d x %d, %d bars, %zu staves)\n", out.getFullPathName().toRawUTF8(), w, h, score.bars, lay.staves.size());
    return 0;
}
