/*
    Theme - the house colour scheme (Good Idea's docs/COLOUR.md, shared by
    every app in the family), and the look-and-feel that applies it.

    Three colours carry it: a dark cool-grey ground, a light grey for the
    controls raised off it, and one yellow for whatever is switched on. Every
    grey is blue-shifted, R < G < B - a neutral grey reads flat beside the
    yellow - and every button takes the dark ink, chosen or not.

    The page of music is set in ink on its own paper, and the accent is spent
    on what is selected and what is sounding. The page can be turned over to
    black on white (Starting Blocks Notation's 0012); the chrome stays.
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

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
inline const juce::Colour rule          { 0xff3A404A };
inline const juce::Colour scrollGrab    { 0xff585F6B };
inline const juce::Colour scrollHover   { 0xff6D7581 };
inline const juce::Colour ink           { 0xff14171C };
inline const juce::Colour text          { 0xffDDE1E7 };
inline const juce::Colour textDim       { 0xff8A919C };
inline const juce::Colour stepNumber    { 0xffBFC5CE };
inline const juce::Colour roll          { 0xff111419 };
inline const juce::Colour playhead      { 0xffF2F4F7 };
inline const juce::Colour warn          { 0xffD2483F };

// Hover and held, derived rather than stored: 18% toward white or black.
juce::Colour shade (juce::Colour c, float amount);

struct Page
{
    juce::Colour paper, ink, dim, accent, warn, lane;
    bool light = false;
};

Page darkPage();
Page lightPage();

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
    void drawTabButton (juce::TabBarButton&, juce::Graphics&, bool isMouseOver, bool isMouseDown) override;
};

} // namespace nt::theme
