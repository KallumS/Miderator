/*
    Theme - the house colour scheme (Good Idea's docs/COLOUR.md, shared by
    every app in the family), and the look-and-feel that applies it.

    Three colours carry it: a dark cool-grey ground, a light grey for the
    controls raised off it, and one yellow for whatever is switched on. Every
    grey is blue-shifted, R < G < B - a neutral grey reads flat beside the
    yellow - and every button takes the dark ink, chosen or not.

    The tracks and the piano roll are the page here: dark by default, as a
    DAW is (decision 0031), or light on request; the chrome stays dark either
    way. The accent is spent on what is selected and what is sounding, red
    only on what an instrument cannot play.

    Apple's guidelines (decision 0048): text is at least 7:1 against what it
    sits on, secondary text included; the house red marks things but is too
    dark to read as text on the ground, so errors are written in warnText.
    With the Mac's Increase Contrast on, useIncreasedContrast() lifts the
    secondary text, the rules and the outlines further. A control the
    keyboard has reached wears a ring in the text colour (focusRing), never
    the yellow, which means only "on".
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace nt
{
// Text with characters beyond ASCII - the ellipsis, dashes, curly quotes and
// key symbols the Mac's own text uses (decision 0054). A plain literal is
// read as ASCII by juce::String, so these go through here.
inline juce::String ui (const char* utf8) { return juce::String::fromUTF8 (utf8); }
} // namespace nt

namespace nt::theme
{

inline const juce::Colour accent        { 0xffFFF200 };
inline const juce::Colour control       { 0xffA9AFBA };
inline const juce::Colour controlHover  { 0xffC0C6CF };
inline const juce::Colour controlHeld   { 0xff8F96A2 };
inline const juce::Colour ground        { 0xff23272E };
inline const juce::Colour sunken        { 0xff1A1D23 };
inline const juce::Colour popup         { 0xff1B1F25 };
inline const juce::Colour frameHover    { 0xff22262D };
inline const juce::Colour frameActive   { 0xff2A2F37 };
inline const juce::Colour scrollGrab    { 0xff585F6B };
inline const juce::Colour scrollHover   { 0xff6D7581 };
inline const juce::Colour ink           { 0xff14171C };
inline const juce::Colour text          { 0xffDDE1E7 };   // 11.4:1 on the ground
inline const juce::Colour textOff       { 0xff8A919C };   // a control that cannot be used (no target)
inline const juce::Colour roll          { 0xff111419 };
inline const juce::Colour playhead      { 0xffF2F4F7 };
inline const juce::Colour warn          { 0xffD2483F };   // marks; 3.4:1, so never text on the ground

// These change with the Mac's Increase Contrast (useIncreasedContrast).
inline juce::Colour textDim             { 0xffB0B6C0 };   // secondary text, 7.3:1 on the ground
inline juce::Colour stepNumber          { 0xffBFC5CE };   // labels beside settings, 8.6:1
inline juce::Colour warnText            { 0xffFF9E96 };   // an error, written: 7.5:1
inline juce::Colour rule                { 0xff3A404A };
inline bool increasedContrast = false;

// Increase Contrast on or off; the caller repaints.
void useIncreasedContrast (bool on);

// A control reached with Tab is ringed in the text colour, inside its own
// bounds with a gap of ground, so nothing outside it is drawn over. Returns
// what is left for the control itself.
juce::Rectangle<float> focusRing (juce::Graphics&, juce::Component&, juce::Rectangle<float> bounds, float corner = 4.0f);

// Clicking a button, a menu or a slider leaves the keyboard with the page, as
// on the Mac, so the score's keys keep working; Tab reaches them instead.
// Applied to a component and everything inside it, text boxes and lists
// excepted (they are typed in, or browsed with the arrows, once clicked).
void clicksKeepFocus (juce::Component&);

// Hover and held, derived rather than stored: 18% toward white or black.
juce::Colour shade (juce::Colour c, float amount);

struct Page
{
    juce::Colour paper, ink, dim, accent, warn, lane;
    bool light = false;
};

Page darkPage();
Page lightPage();

// The piano roll's and the tracks' colours, light or dark.
struct RollColours
{
    Page page;
    juce::Colour whiteRow, blackRow;           // the lanes behind the notes
    juce::Colour barLine, beatLine, stepLine;  // the grid
    juce::Colour note, noteEdge, ghost, faint; // a note, its outline, another part's, outside the sweet register
    juce::Colour selected, sounding;
    juce::Colour whiteKey, blackKey, keyText;
    juce::Colour trackRow, trackRowAlt, header, headerActive, headerText;
    juce::Colour headerDim;                    // a track's instrument, under its name: 6:1 or more (decision 0048)
};

RollColours rollColours (bool light);

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    void drawComboBox (juce::Graphics&, int width, int height, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int x, int y, int width, int height, bool vertical,
                        int thumbStart, int thumbSize, bool over, bool down) override;
    void drawTooltip (juce::Graphics&, const juce::String& text, int width, int height) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawTextEditorOutline (juce::Graphics&, int width, int height, juce::TextEditor&) override;
    void drawTabButton (juce::TabBarButton&, juce::Graphics&, bool isMouseOver, bool isMouseDown) override;
};

} // namespace nt::theme
