/*
    Panels - the toolbar along the top, the Parts panel at the
    side, and the status line along the bottom. Each only shows the
    controller's state and asks the controller for changes.
*/

#pragma once

#include "Controller.h"
#include "Theme.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace nt
{

// Return to start (|<) and skip to end (>|), drawn rather than typed, so
// they look the same whatever fonts the Mac has (decision 0035).
class TransportButton : public juce::Button
{
public:
    enum class Kind { start, end };
    TransportButton (const juce::String& name, Kind k) : juce::Button (name), kind (k) {}
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    Kind kind;
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

    std::function<void()> onFile, onZoomIn, onZoomOut, onStart, onEnd;

private:
    Controller& controller;
    // New, Open, Save, Export, Undo, the score's settings, the sound, the
    // input mode and the look: one menu (decisions 0037, 0038, 0039)
    juce::TextButton fileButton { "File" };
    TransportButton startButton { "Return to start", TransportButton::Kind::start };
    juce::TextButton playButton { "Play" };
    TransportButton endButton { "Skip to end", TransportButton::Kind::end };
    juce::TextButton followButton { "Follow" };
    juce::TextButton selectButton { "Select" }, drawButton { "Draw" };
    juce::Label gridLabel { {}, "Grid" };
    juce::ComboBox gridBox;
    juce::TextButton tripletButton { "Triplet" }, snapButton { "Snap" };
    juce::TextButton quantiseButton { "Quantise" };
    juce::TextButton zoomOut { "-" }, zoomIn { "+" };

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
    juce::TextButton addButton { "Add instrument..." };
    juce::Label info;
    size_t lastCount = 0;

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void rebuild();
    void layoutRows();
    void showAddMenu();
};

class StatusBar : public juce::Component, private juce::ChangeListener, private juce::Timer
{
public:
    explicit StatusBar (Controller& c);
    ~StatusBar() override;
    void paint (juce::Graphics&) override;

private:
    Controller& controller;
    void changeListenerCallback (juce::ChangeBroadcaster*) override { repaint(); }
    void timerCallback() override { repaint(); }
};

} // namespace nt
