/*
    Panels - the toolbar along the top, the Parts panel at the
    side, and the status line under the toolbar. Each only shows the
    controller's state and asks the controller for changes.
*/

#pragma once

#include "Controller.h"
#include "Theme.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace nt
{

// Small pictures rather than characters, so they look the same whatever
// fonts the Mac has: back to the start (|<) and on to the end (>|)
// (decision 0035), and a part's move up, move down and remove (0048).
class IconButton : public juce::Button
{
public:
    enum class Kind { start, end, up, down, remove };
    IconButton (const juce::String& name, Kind k) : juce::Button (name), kind (k) {}
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    Kind kind;
};

// Play, with the triangle everyone knows; Stop, with the square, while it
// plays (decision 0048).
class PlayButton : public juce::TextButton
{
public:
    PlayButton() : juce::TextButton ("Play") {}
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
};

class Toolbar : public juce::Component, private juce::ChangeListener
{
public:
    explicit Toolbar (Controller& c);
    ~Toolbar() override;
    void resized() override;
    void paint (juce::Graphics&) override;

    // The File button, for its menu to open under.
    juce::Component& fileAnchor() { return fileButton; }
    // One row when it fits, two when the window is narrow or the interface
    // is large (decision 0048).
    int heightFor (int width) const;

    std::function<void()> onFile, onZoomIn, onZoomOut, onStart, onEnd, onClear;

private:
    Controller& controller;
    // New, Open, Save, Export, Undo, the score's settings, the sound, the
    // input mode and the look: one menu (decisions 0037, 0038, 0039)
    juce::TextButton fileButton { "File" };
    IconButton startButton { "Return to Start", IconButton::Kind::start };
    PlayButton playButton;
    IconButton endButton { "Skip to End", IconButton::Kind::end };
    juce::TextButton followButton { "Follow" };
    juce::TextButton selectButton { "Select" }, drawButton { "Draw" };
    juce::Label gridLabel { {}, "Grid" };
    juce::ComboBox gridBox;
    juce::TextButton tripletButton { "Triplet" }, snapButton { "Snap" };
    juce::TextButton quantiseButton { "Quantise" };
    juce::TextButton clearButton { "Clear All" };   // every note out (decision 0057)
    juce::TextButton zoomOut { juce::String::charToString (0x2212) }, zoomIn { "+" };

    static constexpr int rowHeight = 46;

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void refresh();
};

class PartsPanel : public juce::Component, private juce::ChangeListener
{
public:
    explicit PartsPanel (Controller& c);
    ~PartsPanel() override;
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    struct Row;
    Controller& controller;
    juce::Viewport viewport;
    juce::Component rows;
    std::vector<std::unique_ptr<Row>> rowList;
    juce::TextButton addButton { juce::String ("Add Instrument") + juce::String::charToString (0x2026) };
    juce::Label info;
    size_t lastCount = 0;

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void rebuild();
    void layoutRows();
    void showAddMenu();
};

// The line under the toolbar: the last thing that happened, where the caret
// is, and what is making the sound. It was along the bottom; the Mac's
// guidelines keep what matters away from a window's bottom edge, which is
// often below the screen (decision 0048). What it says is read out by
// VoiceOver as it changes.
class StatusBar : public juce::Component, public juce::SettableTooltipClient, private juce::ChangeListener, private juce::Timer
{
public:
    explicit StatusBar (Controller& c);
    ~StatusBar() override;
    void paint (juce::Graphics&) override;
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

    // The words on the left: the last message, or the mode.
    juce::String message() const;

private:
    Controller& controller;
    juce::String announced;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override { repaint(); }
};

} // namespace nt
