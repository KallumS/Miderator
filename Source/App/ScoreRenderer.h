/*
    ScoreRenderer - the ink. Turns an engraved layout into marks on a
    juce::Graphics, with Bravura, the reference SMuFL font, for every symbol.

    It decides nothing about where anything goes: that is Engrave.cpp's, in
    staff spaces. Here a staff space becomes `space` pixels and a symbol
    becomes a glyph. Shared by the app and by NoteratorRender, which draws the
    same page into a PNG so a drawing change can be checked without a window
    (decision 0010).
*/

#pragma once

#include "Engrave.h"
#include "Theme.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <set>

namespace nt
{

// The SMuFL code points used.
namespace smufl
{
enum : juce::juce_wchar
{
    gClef = 0xE050, gClef8vb = 0xE052, cClef = 0xE05C, fClef = 0xE062, percussionClef = 0xE069,
    timeSig0 = 0xE080,
    noteheadWhole = 0xE0A2, noteheadHalf = 0xE0A3, noteheadBlack = 0xE0A4,
    noteheadXWhole = 0xE0A7, noteheadXHalf = 0xE0A8, noteheadXBlack = 0xE0A9,
    augmentationDot = 0xE1E7,
    flag8thUp = 0xE240, flag8thDown = 0xE241,
    accidentalFlat = 0xE260, accidentalNatural = 0xE261, accidentalSharp = 0xE262,
    accidentalDoubleSharp = 0xE263, accidentalDoubleFlat = 0xE264,
    restWhole = 0xE4E3, restHalf = 0xE4E4, restQuarter = 0xE4E5, rest8th = 0xE4E6,
    tuplet0 = 0xE880,
    brace = 0xE000
};
} // namespace smufl

// Bravura's outlines, cached as paths one staff space to the unit, origin at
// the glyph's own origin and y running down.
class Glyphs
{
public:
    static Glyphs& get();
    const juce::Path& path (juce::juce_wchar codePoint);
    void draw (juce::Graphics& g, juce::juce_wchar cp, float x, float y, float space);
    juce::Rectangle<float> bounds (juce::juce_wchar cp) { return path (cp).getBounds(); }

private:
    Glyphs();
    juce::Typeface::Ptr typeface;
    float unitsPerSpace = 1.0f;
    std::map<juce::juce_wchar, juce::Path> cache;
    juce::Path makePath (juce::juce_wchar cp, float height);
};

struct RenderStyle
{
    float space = 8.0f;                 // pixels per staff space
    theme::Page page = theme::darkPage();
    std::set<uint32_t> selected;
    std::set<uint32_t> sounding;
    bool showWarnings = true;
    bool showBarNumbers = true;
};

class ScoreRenderer
{
public:
    // Draws the layout with its (0, 0) at `origin`, skipping whatever lies
    // outside `clip` (pixels).
    static void draw (juce::Graphics& g, const engrave::Layout& layout, const Score& score,
                      const RenderStyle& style, juce::Point<float> origin, juce::Rectangle<float> clip);

    // The instruments' names, right-aligned to `right`, one beside each part.
    static void drawNames (juce::Graphics& g, const engrave::Layout& layout, const Score& score,
                           const RenderStyle& style, float right, float originY, bool shortNames);

    // The clef, key and time signature at the left of a staff, for a fixed
    // header that stays put while the music scrolls.
    static void drawStaffStart (juce::Graphics& g, const engrave::Staff& staff, const engrave::Measure& measure,
                                const RenderStyle& style, float x, float y, bool withTime);
};

} // namespace nt
