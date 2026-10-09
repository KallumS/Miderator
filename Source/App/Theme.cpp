#include "Theme.h"

namespace nt::theme
{

juce::Colour shade (juce::Colour c, float amount)
{
    // Keep the alpha: a colour handed back transparent is the bug this
    // function has had before, in the ReaScripts.
    if (amount >= 0) return c.interpolatedWith (juce::Colours::white.withAlpha (c.getFloatAlpha()), amount);
    return c.interpolatedWith (juce::Colours::black.withAlpha (c.getFloatAlpha()), -amount);
}

Page darkPage()
{
    Page p;
    p.paper = roll;
    p.ink = juce::Colour (0xffDDE1E7);
    p.dim = juce::Colour (0xff6D7581);
    p.accent = accent;
    p.warn = warn;
    p.lane = juce::Colour (0xffBFC5CE);
    return p;
}

Page lightPage()
{
    Page p;
    p.paper = juce::Colour (0xffFBFBFD);
    p.ink = juce::Colour (0xff14171C);
    p.dim = juce::Colour (0xff8A919C);
    // A saturated yellow vanishes on white: the same accent, shaded down.
    p.accent = shade (accent, -0.45f);
    p.warn = warn;
    p.lane = juce::Colour (0xff3A404A);
    p.light = true;
    return p;
}

RollColours rollColours (bool light)
{
    RollColours r;
    r.page = light ? lightPage() : darkPage();
    if (light)
    {
        r.whiteRow = juce::Colour (0xffFBFBFD);
        r.blackRow = juce::Colour (0xffECEEF2);
        r.barLine = juce::Colour (0xff8A919C);
        r.beatLine = juce::Colour (0xffCDD2D9);
        r.stepLine = juce::Colour (0xffE4E7EC);
        r.note = juce::Colour (0xff4A515C);
        r.noteEdge = juce::Colour (0xff14171C);
        r.ghost = juce::Colour (0x223A404A);
        r.faint = juce::Colour (0xff9AA1AC);
        r.selected = shade (accent, -0.35f);
        r.sounding = juce::Colour (0xff14171C);
        r.whiteKey = juce::Colour (0xffFFFFFF);
        r.blackKey = juce::Colour (0xff3A404A);
        r.keyText = juce::Colour (0xff3A404A);
        r.trackRow = juce::Colour (0xffF4F5F8);
        r.trackRowAlt = juce::Colour (0xffEDEFF3);
        r.header = juce::Colour (0xffDDE1E7);
        r.headerActive = juce::Colour (0xffC5CBD3);
        r.headerText = juce::Colour (0xff14171C);
    }
    else
    {
        r.whiteRow = juce::Colour (0xff1C2026);
        r.blackRow = juce::Colour (0xff15181D);
        r.barLine = juce::Colour (0xff535A66);
        r.beatLine = juce::Colour (0xff30353D);
        r.stepLine = juce::Colour (0xff24282F);
        r.note = juce::Colour (0xffA9AFBA);
        r.noteEdge = juce::Colour (0xff0D0F13);
        r.ghost = juce::Colour (0x26A9AFBA);
        r.faint = juce::Colour (0xff6D7581);
        r.selected = accent;
        r.sounding = juce::Colour (0xffF2F4F7);
        r.whiteKey = juce::Colour (0xffDDE1E7);
        r.blackKey = juce::Colour (0xff111419);
        r.keyText = juce::Colour (0xff3A404A);
        r.trackRow = juce::Colour (0xff1C2026);
        r.trackRowAlt = juce::Colour (0xff181B21);
        r.header = frameActive;
        r.headerActive = juce::Colour (0xff3A404A);
        r.headerText = text;
    }
    return r;
}

LookAndFeel::LookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, ground);
    setColour (juce::DocumentWindow::textColourId, text);
    setColour (juce::TextButton::buttonColourId, control);
    setColour (juce::TextButton::buttonOnColourId, accent);
    setColour (juce::TextButton::textColourOffId, ink);
    setColour (juce::TextButton::textColourOnId, ink);
    setColour (juce::ComboBox::backgroundColourId, control);
    setColour (juce::ComboBox::textColourId, ink);
    setColour (juce::ComboBox::arrowColourId, ink);
    setColour (juce::ComboBox::outlineColourId, ink);
    setColour (juce::PopupMenu::backgroundColourId, popup);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accent);
    setColour (juce::PopupMenu::highlightedTextColourId, ink);
    setColour (juce::Label::textColourId, text);
    setColour (juce::TextEditor::backgroundColourId, sunken);
    setColour (juce::TextEditor::textColourId, text);
    setColour (juce::TextEditor::outlineColourId, rule);
    setColour (juce::TextEditor::focusedOutlineColourId, controlHover);
    setColour (juce::TextEditor::highlightColourId, accent.withAlpha (0.35f));
    setColour (juce::ListBox::backgroundColourId, sunken);
    setColour (juce::ListBox::textColourId, text);
    setColour (juce::ScrollBar::thumbColourId, scrollGrab);
    setColour (juce::ScrollBar::trackColourId, sunken);
    setColour (juce::Slider::thumbColourId, control);
    setColour (juce::Slider::trackColourId, rule);
    setColour (juce::Slider::backgroundColourId, sunken);
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::Slider::textBoxBackgroundColourId, sunken);
    setColour (juce::Slider::textBoxOutlineColourId, rule);
    setColour (juce::ToggleButton::textColourId, text);
    setColour (juce::ToggleButton::tickColourId, accent);
    setColour (juce::ToggleButton::tickDisabledColourId, textDim);
    setColour (juce::TooltipWindow::backgroundColourId, popup);
    setColour (juce::TooltipWindow::textColourId, text);
    setColour (juce::TooltipWindow::outlineColourId, rule);
    setColour (juce::AlertWindow::backgroundColourId, ground);
    setColour (juce::AlertWindow::textColourId, text);
    setColour (juce::AlertWindow::outlineColourId, rule);
    setColour (juce::CaretComponent::caretColourId, accent);
    setColour (juce::TabbedButtonBar::tabTextColourId, ink);
    setColour (juce::TabbedButtonBar::frontTextColourId, ink);
    setColour (juce::TabbedButtonBar::tabOutlineColourId, ink);
    setColour (juce::TabbedButtonBar::frontOutlineColourId, ink);
    setColour (juce::DirectoryContentsDisplayComponent::highlightColourId, accent);
    setColour (juce::DirectoryContentsDisplayComponent::textColourId, text);
    setColour (juce::DirectoryContentsDisplayComponent::highlightedTextColourId, ink);
    setColour (juce::FileChooserDialogBox::titleTextColourId, text);
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bg, bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    juce::Colour fill = b.getToggleState() ? accent : bg;
    if (! b.isEnabled()) fill = fill.withAlpha (0.4f);
    else if (down) fill = b.getToggleState() ? shade (accent, -0.18f) : controlHeld;
    else if (highlighted) fill = b.getToggleState() ? shade (accent, 0.18f) : controlHover;
    g.setColour (fill);
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (ink);
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
}

juce::Font LookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return juce::Font (juce::FontOptions (juce::jmin (14.0f, buttonHeight * 0.55f)));
}

void LookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    // Ink on every button, chosen or not: the controls are lighter than the
    // ground, so the window's own light text would vanish on them.
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour (b.isEnabled() ? ink : ink.withAlpha (0.5f));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 2), juce::Justification::centred, 2);
}

juce::Font LookAndFeel::getComboBoxFont (juce::ComboBox& box)
{
    return juce::Font (juce::FontOptions (juce::jmin (14.0f, box.getHeight() * 0.55f)));
}

void LookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool down, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, static_cast<float> (width), static_cast<float> (height)).reduced (0.5f);
    g.setColour (down ? controlHeld : (box.isMouseOver (true) ? controlHover : control));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (ink);
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
    juce::Path arrow;
    const float ax = static_cast<float> (width) - 12.0f, ay = static_cast<float> (height) * 0.5f;
    arrow.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
    g.fillPath (arrow);
}

void LookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    const float h = static_cast<float> (b.getHeight());
    auto box = juce::Rectangle<float> (2.0f, (h - 14.0f) * 0.5f, 14.0f, 14.0f);
    g.setColour (b.getToggleState() ? accent : (highlighted ? controlHover : control));
    g.fillRoundedRectangle (box, 2.5f);
    g.setColour (ink);
    g.drawRoundedRectangle (box, 2.5f, 1.0f);
    if (b.getToggleState())
    {
        juce::Path tick;
        tick.startNewSubPath (box.getX() + 3.5f, box.getCentreY());
        tick.lineTo (box.getX() + 6.0f, box.getBottom() - 3.5f);
        tick.lineTo (box.getRight() - 3.0f, box.getY() + 3.5f);
        g.strokePath (tick, juce::PathStrokeType (1.8f));
    }
    g.setColour (b.isEnabled() ? text : textDim);
    g.setFont (juce::FontOptions (13.5f));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().withTrimmedLeft (22), juce::Justification::centredLeft, 2);
}

void LookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int width, int height, bool vertical,
                                 int thumbStart, int thumbSize, bool over, bool down)
{
    g.setColour (sunken);
    g.fillRect (x, y, width, height);
    auto thumb = vertical ? juce::Rectangle<int> (x + 2, thumbStart, width - 4, thumbSize)
                          : juce::Rectangle<int> (thumbStart, y + 2, thumbSize, height - 4);
    g.setColour (down ? control : (over ? scrollHover : scrollGrab));
    g.fillRoundedRectangle (thumb.toFloat(), 3.0f);
}

// Tabs are buttons like any other: the chosen one in the accent, all in ink.
void LookAndFeel::drawTabButton (juce::TabBarButton& b, juce::Graphics& g, bool over, bool)
{
    auto r = b.getActiveArea().toFloat().reduced (2.0f, 3.0f);
    const bool front = b.isFrontTab();
    g.setColour (front ? accent : (over ? controlHover : control));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (ink);
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
    g.setFont (juce::FontOptions (14.0f));
    g.drawText (b.getButtonText(), r, juce::Justification::centred);
}

void LookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& t, int width, int height)
{
    g.fillAll (popup);
    g.setColour (rule);
    g.drawRect (0, 0, width, height);
    g.setColour (text);
    g.setFont (juce::FontOptions (13.0f));
    g.drawFittedText (t, juce::Rectangle<int> (0, 0, width, height).reduced (6, 4), juce::Justification::centredLeft, 20);
}

} // namespace nt::theme
