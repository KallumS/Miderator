/*
    Miderator - a piano-roll DAW for the family of generators.

    The application: one window, the house look-and-feel, tooltips, and the
    files the system hands us (a .miderator or a .mid double-clicked in the
    Finder arrives through anotherInstanceStarted).
*/

#include "MainComponent.h"
#include "Theme.h"

#include <juce_gui_extra/juce_gui_extra.h>

namespace nt
{

class MainWindow : public juce::DocumentWindow
{
public:
    explicit MainWindow (const juce::String& name)
        : juce::DocumentWindow (name, theme::ground, juce::DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar (true);
        content = new MainComponent();
        setContentOwned (content, true);
        setResizable (true, true);
        centreWithSize (getWidth(), getHeight());
        content->windowReady();
        setVisible (true);
        content->grabKeyboardFocus();
    }

    // When the window itself is given the keyboard - as it opens, or comes
    // back to the front - it hands it to the roll, so the keys and Tab work at
    // once rather than after a click (decision 0048).
    void focusGained (FocusChangeType) override
    {
        if (content != nullptr) content->grabKeyboardFocus();
    }

    void closeButtonPressed() override
    {
        content->checkSaved ([] { juce::JUCEApplication::getInstance()->systemRequestedQuit(); });
    }

    MainComponent* content = nullptr;
};

class Application : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return JUCE_APPLICATION_NAME_STRING; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override { return false; }

    void initialise (const juce::String& commandLine) override
    {
        juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
        tooltips = std::make_unique<juce::TooltipWindow> (nullptr, 600);
        window = std::make_unique<MainWindow> (getApplicationName());
        openFromCommandLine (commandLine);
    }

    void shutdown() override
    {
        window = nullptr;
        tooltips = nullptr;
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    }

    void systemRequestedQuit() override
    {
        if (window != nullptr && window->content != nullptr && window->content->hasUnsavedChanges())
        {
            window->content->checkSaved ([this]
            {
                window->content->getController().dirty = false;
                quit();
            });
            return;
        }
        quit();
    }

    void anotherInstanceStarted (const juce::String& commandLine) override { openFromCommandLine (commandLine); }

private:
    theme::LookAndFeel lookAndFeel;
    std::unique_ptr<juce::TooltipWindow> tooltips;
    std::unique_ptr<MainWindow> window;

    void openFromCommandLine (const juce::String& commandLine)
    {
        const auto path = commandLine.unquoted().trim();
        if (path.isEmpty() || window == nullptr) return;
        const juce::File f (path);
        if (f.existsAsFile()) window->content->checkSaved ([this, f] { window->content->openFile (f); });
    }
};

} // namespace nt

START_JUCE_APPLICATION (nt::Application)
