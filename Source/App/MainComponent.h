/*
    MainComponent - the window: toolbar, page, panels, status line, and the
    keys and menus that drive them.

    Keys follow the notation programs people already know: letters write
    notes, numbers choose note values (MuseScore's: 5 is a quarter), the
    arrows move and transpose, Space plays.
*/

#pragma once

#include "AudioEngine.h"
#include "Controller.h"
#include "BlocksPanel.h"
#include "GeneratorPanel.h"
#include "Panels.h"
#include "ScoreView.h"

#include <juce_gui_extra/juce_gui_extra.h>

namespace nt
{

class MainComponent : public juce::Component,
                      public juce::MenuBarModel,
                      public juce::FileDragAndDropTarget,
                      private juce::ChangeListener
{
public:
    MainComponent();
    ~MainComponent() override;

    void resized() override;
    void paint (juce::Graphics&) override;
    bool keyPressed (const juce::KeyPress&) override;

    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex (int index, const juce::String& name) override;
    void menuItemSelected (int itemId, int menuIndex) override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

    // Asks before throwing away unsaved changes; `then` runs if it is fine to go on.
    void checkSaved (std::function<void()> then);
    bool hasUnsavedChanges() const { return controller.dirty; }
    void openFile (const juce::File& f);
    Controller& getController() { return controller; }

private:
    AudioEngine audio;
    Controller controller { audio };
    Toolbar toolbar { controller };
    ScoreView view { controller };
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    GeneratorPanel generatorPanel { controller };
    BlocksPanel blocksPanel { controller };
    PartsPanel partsPanel { controller };
    ScorePanel scorePanel { controller };
    StatusBar statusBar { controller };
    std::unique_ptr<juce::FileChooser> chooser;
    juce::ApplicationProperties preferences;   // the page colour and zoom, kept between launches
    juce::File lastFolder;

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void showNewMenu();
    juce::PopupMenu templateMenu();
    void showExportMenu();
    void showSettingsMenu();
    void openDialog();
    void saveDialog (bool saveAs, std::function<void()> then = {});
    void importDialog();
    enum class ExportKind { midi, audio, musicXml };
    void exportDialog (ExportKind kind, bool selectedBars);
    void audioSettingsDialog();
    void showHelp();
    void updateTitle();
};

} // namespace nt
