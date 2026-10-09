/*
    MideratorRender - draws the tracks and the piano roll into a PNG with no
    window, through the very components the app uses, so a drawing change
    can be checked by looking at it (decision 0010).

        MideratorRender demo out.png [light]          a string quartet, Good Idea in bars 1-4
        MideratorRender song.mid out.png [light]      any MIDI, MusicXML or project file
        MideratorRender song.mid out.png light 3      the third part in the roll
*/

#include "Controller.h"
#include "Workspace.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <cstdio>

using namespace nt;

namespace
{
// A quartet with Good Idea filling the first four bars, and a few notes drawn
// by hand that the violin cannot play well, so the warnings show too.
void makeDemo (Controller& c)
{
    c.newScore ("String Quartet");
    c.setBars (8);
    c.selectRange (0, 3, 0, 3);
    // A Measure - tune, chords and bass - so every part gets some.
    c.lua.set ("good-idea", "kind", 2, c.generatorContext (false));
    const GeneratedResult* chosen = nullptr;
    GeneratorOutput out;
    for (int seed = 1; seed < 40 && chosen == nullptr; ++seed)
    {
        out = c.lua.generate ("good-idea", c.generatorContext (false), seed, 4);
        for (const auto& r : out.results)
            if (r.parts.size() >= 3) { chosen = &r; break; }
    }
    if (chosen == nullptr && ! out.results.empty()) chosen = &out.results.front();
    if (chosen != nullptr) c.insertGenerated (*chosen, false, "good-idea");
    c.lua.reset ("good-idea");
    const auto violin = c.score.parts[0].id;
    c.select ({});
    c.drawNoteAt (violin, c.score.barStart (5), 52, PPQ);                 // below its range
    c.drawNoteAt (violin, c.score.barStart (5) + PPQ, 76, PPQ / 2);
    c.drawNoteAt (violin, c.score.barStart (5) + PPQ * 3 / 2, 96, PPQ / 2);   // a leap of 20
    c.drawNoteAt (violin, c.score.barStart (6), 74, PPQ);
    c.drawNoteAt (violin, c.score.barStart (6), 79, PPQ);                 // a chord on one line
    c.selectRange (0, 3, 0, 3);
    c.setCaret (violin, c.score.barStart (5));
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc < 3)
    {
        std::printf ("usage: MideratorRender <demo|file> <out.png> [light] [part number]\n");
        return 1;
    }
    theme::LookAndFeel lookAndFeel;
    juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);

    AudioEngine audio;
    Controller controller (audio);
    const juce::String what (argv[1]);
    if (what == "demo") makeDemo (controller);
    else
    {
        juce::String error;
        if (! controller.load (juce::File::getCurrentWorkingDirectory().getChildFile (what), error))
        {
            std::printf ("could not open %s: %s\n", argv[1], error.toRawUTF8());
            return 1;
        }
    }
    controller.lightTheme = argc > 3 && juce::String (argv[3]) == "light";
    if (argc > 4)
    {
        const int i = juce::String (argv[4]).getIntValue() - 1;
        if (i >= 0 && i < static_cast<int> (controller.score.parts.size())) controller.setCaret (controller.score.parts[static_cast<size_t> (i)].id, controller.caret);
    }

    {
        Workspace workspace (controller);
        workspace.setBounds (0, 0, 1280, 860);
        controller.viewChanged();
        workspace.roll.centreOnPart();
        const auto image = workspace.createComponentSnapshot (workspace.getLocalBounds(), true, 1.0f);
        juce::File out = juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]);
        out.deleteFile();
        juce::FileOutputStream stream (out);
        juce::PNGImageFormat png;
        if (! stream.openedOk() || ! png.writeImageToStream (image, stream))
        {
            std::printf ("could not write %s\n", argv[2]);
            return 1;
        }
        std::printf ("%s: %d parts, %d bars, %s\n", argv[2], static_cast<int> (controller.score.parts.size()), controller.score.bars,
                     controller.lightTheme ? "light" : "dark");
    }
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    return 0;
}
