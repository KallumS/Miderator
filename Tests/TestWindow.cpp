/*
    The window against Apple's guidelines (decision 0048), without opening
    it: text colours meet the contrast Apple asks for, every control has a
    name VoiceOver can read and leaves the keyboard with the score when it
    is clicked, the toolbar folds into two rows when it is narrow, and
    Reduce Motion turns a page at a time rather than scrolling.
*/

#include "Check.h"

#include "BlocksPanel.h"
#include "Controller.h"
#include "GeneratorPanel.h"
#include "Panels.h"
#include "Theme.h"

#include <cmath>

using namespace nt;

namespace
{
AudioEngine& windowAudio()
{
    static AudioEngine engine;
    return engine;
}

// WCAG's contrast ratio, the one Apple's Accessibility Inspector uses.
double contrast (juce::Colour a, juce::Colour b)
{
    const auto lum = [] (juce::Colour c)
    {
        const auto lin = [] (float v) { return v <= 0.03928f ? v / 12.92 : std::pow ((v + 0.055) / 1.055, 2.4); };
        return 0.2126 * lin (c.getFloatRed()) + 0.7152 * lin (c.getFloatGreen()) + 0.0722 * lin (c.getFloatBlue());
    };
    const double la = lum (a), lb = lum (b);
    return (std::max (la, lb) + 0.05) / (std::min (la, lb) + 0.05);
}

// Every control inside `c`, depth first.
void controlsIn (juce::Component& c, std::vector<juce::Component*>& out)
{
    if (dynamic_cast<juce::Button*> (&c) != nullptr || dynamic_cast<juce::ComboBox*> (&c) != nullptr
        || dynamic_cast<juce::Slider*> (&c) != nullptr || dynamic_cast<juce::TextEditor*> (&c) != nullptr)
        out.push_back (&c);
    for (auto* child : c.getChildren()) controlsIn (*child, out);
}

// What VoiceOver would call it: its title, or a button's own words.
juce::String spokenName (juce::Component& c)
{
    if (c.getTitle().isNotEmpty()) return c.getTitle();
    if (auto* b = dynamic_cast<juce::Button*> (&c)) return b->getButtonText();
    return {};
}

void checkControls (juce::Component& panel, const char* what)
{
    std::vector<juce::Component*> controls;
    controlsIn (panel, controls);
    CHECK (! controls.empty());
    for (auto* c : controls)
    {
        const auto name = spokenName (*c);
        // A name of one letter or a sign ("M", "^", "+") says nothing aloud.
        if (name.length() < 2 || ! name.containsAnyOf ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"))
            check::fail (__FILE__, __LINE__, std::string (what) + ": a control VoiceOver would call \"" + name.toStdString() + "\"");
        if (dynamic_cast<juce::TextEditor*> (c) == nullptr && c->getMouseClickGrabsKeyboardFocus())
            check::fail (__FILE__, __LINE__, std::string (what) + ": clicking \"" + name.toStdString() + "\" takes the keyboard from the score");
    }
}
} // namespace

TEST ("window: text meets Apple's contrast, with and without Increase Contrast")
{
    for (const bool high : { false, true })
    {
        theme::useIncreasedContrast (high);
        // Small text: 7:1 is what Apple asks of custom colours (Dark Mode).
        for (const auto bg : { theme::ground, theme::sunken, theme::popup })
        {
            CHECK (contrast (theme::text, bg) >= 7.0);
            CHECK (contrast (theme::textDim, bg) >= 7.0);
            CHECK (contrast (theme::stepNumber, bg) >= 7.0);
            CHECK (contrast (theme::warnText, bg) >= 7.0);
        }
        // Dark ink on every control, chosen or not, hovered or not.
        for (const auto bg : { theme::control, theme::controlHover, theme::accent })
            CHECK (contrast (theme::ink, bg) >= 7.0);
        CHECK (contrast (theme::ink, theme::controlHeld) >= 4.5);
        // The tracks and the roll, dark and light.
        for (const bool light : { false, true })
        {
            const auto r = theme::rollColours (light);
            CHECK (contrast (r.headerText, r.header) >= 7.0);
            CHECK (contrast (r.headerText, r.headerActive) >= 7.0);
            CHECK (contrast (r.headerDim, r.header) >= 6.0);
            CHECK (contrast (r.headerDim, r.headerActive) >= 6.0);
            CHECK (contrast (r.keyText, r.whiteKey) >= 7.0);
            // A note and a warning are marks, not text: 3:1 on the rows.
            CHECK (contrast (r.note, r.whiteRow) >= 3.0);
            CHECK (contrast (r.page.warn, r.whiteRow) >= 3.0);
        }
    }
    // The house red is too dark to be text on the ground: errors use warnText.
    CHECK (contrast (theme::warn, theme::ground) < 4.5);
    theme::useIncreasedContrast (false);
}

TEST ("window: Increase Contrast makes secondary text and rules stronger")
{
    theme::useIncreasedContrast (false);
    const double dim = contrast (theme::textDim, theme::ground), rule = contrast (theme::rule, theme::ground);
    theme::useIncreasedContrast (true);
    CHECK (contrast (theme::textDim, theme::ground) > dim);
    CHECK (contrast (theme::rule, theme::ground) > rule);
    theme::useIncreasedContrast (false);
}

TEST ("window: every control has a name VoiceOver can say, and clicking it leaves the keys with the score")
{
    Controller c (windowAudio());
    c.newScore ("String Quartet");
    Toolbar toolbar (c);
    checkControls (toolbar, "toolbar");
    PartsPanel parts (c);
    parts.setSize (380, 700);
    checkControls (parts, "Parts");
    GeneratorPanel generate (c);
    generate.setSize (380, 800);
    checkControls (generate, "Generate");
    BlocksPanel blocks (c);
    blocks.setSize (380, 800);
    checkControls (blocks, "Blocks");
    // A part added later gets its row the same way.
    c.addPart ("vln");
    checkControls (parts, "Parts, after adding a part");
}

TEST ("window: the toolbar is one row when it fits and two when it does not")
{
    Controller c (windowAudio());
    Toolbar toolbar (c);
    const int one = toolbar.heightFor (1440);
    const int two = toolbar.heightFor (700);
    CHECK (two > one);
    toolbar.setBounds (0, 0, 700, two);
    // Nothing off the right edge or over anything else.
    std::vector<juce::Component*> controls;
    controlsIn (toolbar, controls);
    for (auto* a : controls)
    {
        CHECK (toolbar.getLocalBounds().contains (a->getBounds()));
        for (auto* b : controls)
            if (a != b) CHECK (! a->getBounds().intersects (b->getBounds()));
    }
}

TEST ("window: Reduce Motion turns a page at a time while it plays")
{
    Controller c (windowAudio());
    c.followStyle = FollowStyle::smooth;
    CHECK (c.followStyleNow() == FollowStyle::smooth);
    c.reduceMotion = true;
    CHECK (c.followStyleNow() == FollowStyle::page);
    // The choice itself is kept for when Reduce Motion is off again.
    CHECK (c.followStyle == FollowStyle::smooth);
}

TEST ("window: the tracks and the roll are described in words for VoiceOver")
{
    Controller c (windowAudio());
    c.newScore ("String Quartet");
    c.setCaret (c.score.parts[1].id, 0);
    const auto d = c.describe();
    CHECK (d.contains ("4 parts"));
    CHECK (d.contains ("Violin II"));
    CHECK (d.contains ("bar 1"));
}
