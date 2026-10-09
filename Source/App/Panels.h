/*
    Panels - the toolbar along the top, the Parts and Score panels at the
    side, and the status line along the bottom. Each only shows the
    controller's state and asks the controller for changes.
*/

#pragma once

#include "Controller.h"
#include "Theme.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace nt
{

class Toolbar : public juce::Component, private juce::ChangeListener
{
public:
    explicit Toolbar (Controller& c);
    ~Toolbar() override;
    void resized() override;
    void paint (juce::Graphics&) override;

    std::function<void()> onNew, onOpen, onSave, onExport, onSettings, onZoomIn, onZoomOut;

private:
    Controller& controller;
    juce::TextButton newButton { "New" }, openButton { "Open" }, saveButton { "Save" }, exportButton { "Export" };
    juce::TextButton undoButton { "Undo" }, redoButton { "Redo" };
    juce::TextButton playButton { "Play" };
    juce::TextButton selectButton { "Select" }, drawButton { "Draw" };
    juce::Label gridLabel { {}, "Grid" };
    juce::ComboBox gridBox;
    juce::TextButton tripletButton { "Triplet" }, snapButton { "Snap" };
    juce::TextButton quantiseButton { "Quantise" };
    juce::TextButton stepButton { "Step input" };
    juce::TextButton themeButton { "Light" };
    juce::TextButton zoomOut { "-" }, zoomIn { "+" };
    juce::TextButton settingsButton { "Sound" };

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

class ScorePanel : public juce::Component, private juce::ChangeListener
{
public:
    explicit ScorePanel (Controller& c);
    ~ScorePanel() override;
    void resized() override;
    void paint (juce::Graphics&) override;

    std::function<void()> onAudioSettings;

private:
    Controller& controller;
    juce::Label titleLabel { {}, "Title" }, composerLabel { {}, "Composer" };
    juce::TextEditor title, composer;
    juce::Label tempoLabel { {}, "Tempo" };
    juce::Slider tempo;
    juce::Label meterLabel { {}, "Time signature" };
    juce::ComboBox meterNum, meterDen;
    juce::TextButton meterApply { "Set at caret's bar" };
    juce::Label keyLabel { {}, "Key" };
    juce::ComboBox keyRoot, keyScale;
    juce::TextButton keyApply { "Set at caret's bar" }, keyDetected { "Use the key it hears" };
    juce::Label barsLabel { {}, "Bars" };
    juce::TextButton insertBar { "Insert a bar at the caret" }, deleteBars { "Delete selected bars" }, addBars { "Add 4 bars at the end" };
    juce::Label soundLabel { {}, "Sound" };
    juce::ComboBox synthChoice;
    juce::TextButton audioSettings { "Audio and MIDI devices..." };

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void refresh();
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
