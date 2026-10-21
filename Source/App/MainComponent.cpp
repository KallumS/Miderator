#include "MainComponent.h"

#include "Exporter.h"
#include "SystemSettings.h"
#include "ScaleModel.h"
#include "Templates.h"

namespace nt
{

namespace
{
enum MenuIds
{
    menuNew = 1, menuOpen, menuImport, menuSave, menuSaveAs, menuExportMidi, menuExportMidiBars, menuExportAudio, menuExportAudioBars,
    menuExportXml, menuExportXmlBars,
    menuUndo = 100, menuRedo, menuCut, menuCopy, menuPaste, menuDelete, menuSelectAll, menuDuplicate, menuQuantise, menuClearAll,
    menuPlay = 200, menuPlayCaret, menuToStart, menuToEnd, menuFollow, menuFollowSmooth, menuFollowPage, menuDrawTool, menuStepInput, menuSnap, menuTriplet, menuLightTheme, menuZoomIn, menuZoomOut, menuAudioSettings, menuHelp,
    menuGridBase = 300,          // + index into roll::gridValues()
    menuScaleBase = 340,         // + index into interfaceScales()
    menuTabBase = 360,           // + the side panel's tab
    menuTitle = 400, menuTempo, menuMeterOther, menuKeyHeard, menuInsertBar, menuDeleteBars, menuAddBars, menuSoundApple, menuSoundBuiltIn,
    menuMeterBase = 500,
    menuTemplateBase = 1000,
    menuKeyBase = 2000           // + root x 32 + scale
};

// The time signatures offered in the menu; anything else is under Other...
const std::vector<std::pair<int, int>>& meterChoices()
{
    static const std::vector<std::pair<int, int>> m { { 2, 2 }, { 2, 4 }, { 3, 4 }, { 4, 4 }, { 5, 4 }, { 6, 4 }, { 7, 4 },
                                                     { 3, 8 }, { 5, 8 }, { 6, 8 }, { 7, 8 }, { 9, 8 }, { 12, 8 } };
    return m;
}

// An ellipsis, as the Mac writes one after a command that asks something first.
const juce::String dots = juce::String::charToString (0x2026);

juce::String key (const juce::String& k)
{
   #if JUCE_MAC
    return k.replace ("Cmd", juce::String::charToString (0x2318));
   #else
    return k.replace ("Cmd", "Ctrl");
   #endif
}
} // namespace

MainComponent::MainComponent()
{
    addAndMakeVisible (toolbar);
    addAndMakeVisible (workspace);
    addAndMakeVisible (tabs);
    addAndMakeVisible (statusBar);
    tabs.setTabBarDepth (30);
    tabs.setOutline (0);
    tabs.addTab ("Generate", theme::control, &generatorPanel, false);
    tabs.addTab ("Blocks", theme::control, &blocksPanel, false);
    tabs.addTab ("Parts", theme::control, &partsPanel, false);
    tabs.setColour (juce::TabbedComponent::backgroundColourId, theme::ground);
    tabs.onTabChanged = [this] { if (isShowing()) grabKeyboardFocus(); };

    toolbar.onFile = [this] { showFileMenu(); };
    toolbar.onZoomIn = [this] { zoomBy (1.25f); };
    toolbar.onZoomOut = [this] { zoomBy (1.0f / 1.25f); };
    toolbar.onStart = [this] { returnToStart(); };
    toolbar.onEnd = [this] { skipToEnd(); };
    toolbar.onClear = [this] { clearAllDialog(); };

    // A MIDI keyboard writes in step input, and is always heard.
    audio.onMidiInput = [this] (const juce::MidiMessage& m)
    {
        if (! m.isNoteOn() || ! controller.stepInput) return;
        // Keys pressed within a moment of each other are one chord.
        static double lastTime = 0;
        const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
        const bool chord = now - lastTime < 0.06;
        lastTime = now;
        controller.writePitch (m.getNoteNumber(), chord);
        workspace.timeline.reveal (controller.caret);
    };
    audio.onPlaybackFinished = [this] { controller.stop(); };

    juce::PropertiesFile::Options prefs;
    prefs.applicationName = "Miderator";
    prefs.filenameSuffix = "settings";
    prefs.osxLibrarySubFolder = "Application Support";
    preferences.setStorageParameters (prefs);
    if (auto* p = preferences.getUserSettings())
    {
        controller.lightTheme = p->getBoolValue ("lightTheme", false);
        controller.followPlayback = p->getBoolValue ("followPlayback", true);
        controller.followStyle = p->getValue ("followStyle", "smooth") == "page" ? FollowStyle::page : FollowStyle::smooth;
        controller.varyReplaces = p->getBoolValue ("varyReplaces", false);
        controller.zoom = static_cast<float> (juce::jlimit (6.0, 400.0, p->getDoubleValue ("zoom", controller.zoom)));
        controller.rowHeight = static_cast<float> (juce::jlimit (5.0, 28.0, p->getDoubleValue ("rowHeight", controller.rowHeight)));
        const auto scale = static_cast<float> (p->getDoubleValue ("interfaceScale", 1.0));
        for (auto s : interfaceScales())
            if (std::abs (s - scale) < 0.01f) juce::Desktop::getInstance().setGlobalScaleFactor (s);
        controller.viewChanged();
    }
    readSystemSettings();
    startTimer (1500);
    juce::Desktop::getInstance().addFocusChangeListener (this);

    controller.addChangeListener (this);
    setWantsKeyboardFocus (true);
    // Clicking a control leaves the keyboard with the roll (decision 0048).
    theme::clicksKeepFocus (*this);
    const float scale = juce::Desktop::getInstance().getGlobalScaleFactor();
    setSize (juce::roundToInt (1440.0f / scale), juce::roundToInt (880.0f / scale));
   #if JUCE_MAC
    juce::MenuBarModel::setMacMainMenu (this);
   #endif
    updateTitle();
}

MainComponent::~MainComponent()
{
   #if JUCE_MAC
    juce::MenuBarModel::setMacMainMenu (nullptr);
   #endif
    juce::Desktop::getInstance().removeFocusChangeListener (this);
    controller.removeChangeListener (this);
    audio.stop();
}

void MainComponent::paint (juce::Graphics& g) { g.fillAll (theme::ground); }

void MainComponent::resized()
{
    // Nothing that matters along the bottom edge, which is often below the
    // screen: the status line is under the toolbar (decision 0048).
    auto r = getLocalBounds();
    toolbar.setBounds (r.removeFromTop (toolbar.heightFor (getWidth())));
    statusBar.setBounds (r.removeFromTop (26));
    tabs.setBounds (r.removeFromRight (juce::jlimit (280, 380, getWidth() / 3)));
    workspace.setBounds (r);
}

const std::vector<float>& MainComponent::interfaceScales()
{
    static const std::vector<float> s { 1.0f, 1.25f, 1.5f, 1.75f, 2.0f };
    return s;
}

void MainComponent::windowReady()
{
    auto* w = findParentComponentOfClass<juce::ResizableWindow>();
    if (w == nullptr) return;
    // The smallest window is about the same on the screen at every size, so
    // 200% still fits a laptop: the layout reflows rather than the window
    // outgrowing the screen.
    const float scale = juce::Desktop::getInstance().getGlobalScaleFactor();
    const int minW = std::min (980, juce::roundToInt (1100.0f / scale)), minH = std::min (600, juce::roundToInt (700.0f / scale));
    w->setResizeLimits (minW, minH, 10000, 10000);
    if (const auto* d = juce::Desktop::getInstance().getDisplays().getDisplayForRect (w->getScreenBounds()))
    {
        const auto area = d->userBounds.toNearestInt();
        auto b = w->getBounds();
        b.setSize (std::min (b.getWidth(), area.getWidth()), std::min (b.getHeight(), area.getHeight()));
        w->setBounds (b.constrainedWithin (area));
    }
}

void MainComponent::setInterfaceScale (float scale)
{
    auto& desktop = juce::Desktop::getInstance();
    const float old = desktop.getGlobalScaleFactor();
    if (std::abs (old - scale) < 0.01f) return;
    auto* w = findParentComponentOfClass<juce::ResizableWindow>();
    const auto before = w != nullptr ? w->getBounds().toFloat() * old : juce::Rectangle<float>();
    desktop.setGlobalScaleFactor (scale);
    if (auto* p = preferences.getUserSettings()) p->setValue ("interfaceScale", static_cast<double> (scale));
    if (w != nullptr)
    {
        // The same place on the screen, drawn larger or smaller inside.
        w->setResizeLimits (100, 100, 10000, 10000);
        w->setBounds ((before / scale).toNearestInt());
        windowReady();
    }
    controller.setStatus ("Interface size " + juce::String (juce::roundToInt (scale * 100.0f)) + "%");
}

void MainComponent::readSystemSettings()
{
    const bool contrast = system::increaseContrast();
    const bool motion = system::reduceMotion();
    if (contrast == theme::increasedContrast && motion == controller.reduceMotion) return;
    theme::useIncreasedContrast (contrast);
    controller.reduceMotion = motion;
    controller.viewChanged();
    repaint();
}

void MainComponent::timerCallback() { readSystemSettings(); }

void MainComponent::globalFocusChanged (juce::Component* focused)
{
    if (lastFocused != nullptr) lastFocused->repaint();
    if (focused != nullptr) focused->repaint();
    lastFocused = focused;
    // A setting reached with Tab scrolls into view.
    if (focused == nullptr) return;
    if (auto* port = focused->findParentComponentOfClass<juce::Viewport>())
        if (auto* inside = port->getViewedComponent(); inside != nullptr && inside->isParentOf (focused))
        {
            const auto b = inside->getLocalArea (focused, focused->getLocalBounds());
            const auto shown = port->getViewArea();
            if (b.getY() < shown.getY()) port->setViewPosition (shown.getX(), b.getY());
            else if (b.getBottom() > shown.getBottom()) port->setViewPosition (shown.getX(), b.getBottom() - shown.getHeight());
        }
}

void MainComponent::showError (const juce::String& title, const juce::String& message)
{
    // The Mac's own alert (decision 0048).
    juce::NativeMessageBox::showAsync (juce::MessageBoxOptions()
                                           .withIconType (juce::MessageBoxIconType::WarningIcon)
                                           .withTitle (title)
                                           .withMessage (message)
                                           .withButton ("OK")
                                           .withAssociatedComponent (this),
                                       nullptr);
}


void MainComponent::changeListenerCallback (juce::ChangeBroadcaster*)
{
    if (auto* p = preferences.getUserSettings())
    {
        if (p->getBoolValue ("lightTheme", false) != controller.lightTheme) p->setValue ("lightTheme", controller.lightTheme);
        if (p->getBoolValue ("followPlayback", true) != controller.followPlayback) p->setValue ("followPlayback", controller.followPlayback);
        if (p->getBoolValue ("varyReplaces", false) != controller.varyReplaces) p->setValue ("varyReplaces", controller.varyReplaces);
        const juce::String style = controller.followStyle == FollowStyle::page ? "page" : "smooth";
        if (p->getValue ("followStyle", "smooth") != style) p->setValue ("followStyle", style);
        if (std::abs (p->getDoubleValue ("zoom", 0.0) - controller.zoom) > 0.01) p->setValue ("zoom", controller.zoom);
        if (std::abs (p->getDoubleValue ("rowHeight", 0.0) - controller.rowHeight) > 0.01) p->setValue ("rowHeight", controller.rowHeight);
    }
    updateTitle();
    menuItemsChanged();
}

void MainComponent::updateTitle()
{
    auto* w = findParentComponentOfClass<juce::DocumentWindow>();
    if (w == nullptr) return;
   #if JUCE_MAC
    // As a Mac document window: the song's name, a dot in the close button
    // while there are unsaved changes, and the file's icon beside the name.
    w->setName (juce::String (controller.score.title));
    if (auto* peer = w->getPeer())
    {
        peer->setDocumentEditedStatus (controller.dirty);
        peer->setRepresentedFile (controller.file);
    }
   #else
    w->setName ((controller.dirty ? "* " : "") + juce::String (controller.score.title) + " - Miderator");
   #endif
}

//==============================================================================
// Keys

void MainComponent::zoomBy (float factor)
{
    workspace.timeline.zoomBy (factor, static_cast<float> (Timeline::left) + static_cast<float> (workspace.timeline.viewWidth) * 0.5f);
}

bool MainComponent::keyPressed (const juce::KeyPress& k)
{
    auto& c = controller;
    const auto mods = k.getModifiers();
    // Letters can arrive as either case depending on the platform and the
    // modifiers held; compare them as capitals.
    const int rawCode = k.getKeyCode();
    const int code = (rawCode >= 'a' && rawCode <= 'z') ? rawCode - 'a' + 'A' : rawCode;
    const auto ch = k.getTextCharacter();
    const bool cmd = mods.isCommandDown();

    if (cmd)
    {
        if (code == 'Z') { mods.isShiftDown() ? c.redo() : c.undo(); return true; }
        if (code == 'Y') { c.redo(); return true; }
        if (code == 'S') { saveDialog (mods.isShiftDown()); return true; }
        if (code == 'O') { openDialog(); return true; }
        if (code == 'N') { showNewMenu(); return true; }
        if (code == 'E') { showExportMenu(); return true; }
        if (code == 'A') { c.selectAll(); return true; }
        if (code >= '1' && code <= '3') { tabs.setCurrentTabIndex (code - '1'); return true; }
        if (code == 'C') { c.copySelection(); return true; }
        if (code == 'X') { c.cutSelection(); return true; }
        if (code == 'V') { c.paste(); return true; }
        if (code == 'D') { c.duplicateSelection(); return true; }
        if (code == '=' || code == '+') { zoomBy (1.25f); return true; }
        if (code == '-') { zoomBy (1.0f / 1.25f); return true; }
        if (code == juce::KeyPress::upKey) { c.transposeSelection (12); return true; }
        if (code == juce::KeyPress::downKey) { c.transposeSelection (-12); return true; }
        return false;
    }

    // Tab and Shift+Tab: the roll, then every control in reading order, and
    // round to the roll again (decision 0048). JUCE's own order would step out
    // to the window itself.
    if (code == juce::KeyPress::tabKey && ! cmd && ! mods.isAltDown())
    {
        auto list = juce::KeyboardFocusTraverser().getAllComponents (this);
        list.erase (std::remove_if (list.begin(), list.end(), [] (juce::Component* x) { return ! x->isShowing() || ! x->isEnabled(); }),
                    list.end());
        list.insert (list.begin(), this);
        const auto at = std::find (list.begin(), list.end(), juce::Component::getCurrentlyFocusedComponent());
        const int n = static_cast<int> (list.size());
        const int from = at == list.end() ? 0 : static_cast<int> (at - list.begin());
        list[static_cast<size_t> ((from + (mods.isShiftDown() ? n - 1 : 1)) % n)]->grabKeyboardFocus();
        return true;
    }

    // A control Tab has reached: Space presses it, as Return does, and Esc
    // hands the keyboard back to the roll (decision 0048).
    if (auto* focused = juce::Component::getCurrentlyFocusedComponent(); focused != nullptr && focused != this && isParentOf (focused))
    {
        if (code == juce::KeyPress::escapeKey) { grabKeyboardFocus(); return true; }
        if (code == juce::KeyPress::spaceKey && mods.isShiftDown() == false)
        {
            if (auto* b = dynamic_cast<juce::Button*> (focused)) { b->triggerClick(); return true; }
            if (auto* box = dynamic_cast<juce::ComboBox*> (focused)) { box->showPopup(); return true; }
        }
    }

    // Space from bar 1, Shift+Space from the caret (decision 0035).
    if (code == juce::KeyPress::spaceKey) { c.togglePlay (! mods.isShiftDown()); return true; }
    if (code == juce::KeyPress::escapeKey)
    {
        if (c.audio.isPlaying()) c.stop();
        else if (c.stepInput) c.toggleStepInput();
        else c.select ({});
        return true;
    }
    if (code == juce::KeyPress::deleteKey || code == juce::KeyPress::backspaceKey) { c.deleteSelection(); return true; }
    if (code == juce::KeyPress::upKey || code == juce::KeyPress::downKey)
    {
        const int dir = code == juce::KeyPress::upKey ? 1 : -1;
        if (mods.isAltDown())
        {
            c.caretToPart (-dir);
            workspace.arrange.revealPart (c.caretPart);
        }
        else c.transposeSelection (mods.isShiftDown() ? 12 * dir : dir);
        return true;
    }
    if (code == juce::KeyPress::leftKey || code == juce::KeyPress::rightKey)
    {
        const int dir = code == juce::KeyPress::leftKey ? -1 : 1;
        if (c.selection.empty() || c.stepInput) c.moveCaret (dir);
        else c.moveSelection (dir);
        workspace.timeline.reveal (c.caret);
        return true;
    }
    if (code == juce::KeyPress::homeKey) { returnToStart(); return true; }
    if (code == juce::KeyPress::endKey) { skipToEnd(); return true; }

    const auto lower = juce::CharacterFunctions::toLowerCase (ch);
    if (ch >= '1' && ch <= '6')
    {
        // 1 a bar, down to 6 a thirty-second.
        c.setGrid (roll::gridValues()[static_cast<size_t> (ch - '1')], c.grid.triplet);
        return true;
    }
    if (lower == 't') { c.setGrid (c.grid.base, ! c.grid.triplet); return true; }
    if (lower == 'q') { c.quantiseSelection(); return true; }
    if (lower == 'd' || lower == 'b') { c.toggleDrawTool(); return true; }
    if (lower == 'r') { c.toggleStepInput(); return true; }
    if (lower == 'f') { c.toggleFollow(); return true; }
    if (lower == 'p') { c.previewSelection(); return true; }
    if (lower == 'h' || ch == '?') { showHelp(); return true; }
    return false;
}

//==============================================================================
// Menus

juce::StringArray MainComponent::getMenuBarNames() { return { "File", "Edit", "View", "Play", "Help" }; }

juce::PopupMenu MainComponent::getMenuForIndex (int index, const juce::String&)
{
    juce::PopupMenu m;
    auto item = [&m] (int id, const juce::String& text, const juce::String& shortcut = {}, bool enabled = true)
    {
        juce::PopupMenu::Item i (text);
        i.itemID = id;
        i.isEnabled = enabled;
        if (shortcut.isNotEmpty()) i.shortcutKeyDescription = key (shortcut);
        m.addItem (i);
    };
    if (index == 0)
    {
        m.addSubMenu ("New", templateMenu());
        item (menuOpen, "Open" + dots, "Cmd+O");
        item (menuImport, "Import MIDI or MusicXML" + dots);
        m.addSeparator();
        item (menuSave, "Save", "Cmd+S");
        item (menuSaveAs, "Save As" + dots, "Shift+Cmd+S");
        m.addSeparator();
        item (menuExportMidi, "Export Song as MIDI" + dots);
        item (menuExportMidiBars, "Export Selected Bars as MIDI" + dots);
        item (menuExportAudio, "Export Song as Audio (WAV)" + dots);
        item (menuExportAudioBars, "Export Selected Bars as Audio (WAV)" + dots);
        item (menuExportXml, "Export Song as MusicXML" + dots);
        item (menuExportXmlBars, "Export Selected Bars as MusicXML" + dots);
        addScoreItems (m);
    }
    else if (index == 1)
    {
        item (menuUndo, "Undo", "Cmd+Z", controller.canUndo());
        item (menuRedo, "Redo", "Shift+Cmd+Z", controller.canRedo());
        m.addSeparator();
        item (menuCut, "Cut", "Cmd+X", ! controller.selection.empty());
        item (menuCopy, "Copy", "Cmd+C", ! controller.selection.empty());
        item (menuPaste, "Paste at the Caret", "Cmd+V");
        item (menuDelete, "Delete", "Delete", ! controller.selection.empty());
        item (menuSelectAll, "Select All", "Cmd+A");
        item (menuDuplicate, "Duplicate", "Cmd+D", ! controller.selection.empty());
        m.addSeparator();
        item (menuQuantise, "Quantise to " + juce::String (controller.grid.name()), "Q");
        m.addSeparator();
        item (menuClearAll, "Clear All Notes" + dots, {}, controller.score.lastNoteEnd() > 0);
    }
    else if (index == 2)
    {
        {
            juce::PopupMenu::Item i ("Draw Tool");
            i.itemID = menuDrawTool;
            i.isTicked = controller.drawTool;
            i.shortcutKeyDescription = "D";
            m.addItem (i);
        }
        {
            juce::PopupMenu::Item i ("Step Input from a MIDI Keyboard");
            i.itemID = menuStepInput;
            i.isTicked = controller.stepInput;
            i.shortcutKeyDescription = "R";
            m.addItem (i);
        }
        m.addSeparator();
        juce::PopupMenu grid;
        const auto& values = roll::gridValues();
        for (size_t i = 0; i < values.size(); ++i)
        {
            roll::Grid gr;
            gr.base = values[i];
            juce::PopupMenu::Item it (gr.name());
            it.itemID = menuGridBase + static_cast<int> (i);
            it.isTicked = controller.grid.base == values[i];
            it.shortcutKeyDescription = juce::String (static_cast<int> (i) + 1);
            grid.addItem (it);
        }
        grid.addSeparator();
        grid.addItem (menuTriplet, "Triplet", true, controller.grid.triplet);
        grid.addItem (menuSnap, "Snap to the Grid", true, controller.grid.snap);
        m.addSubMenu ("Grid", grid);
        m.addSeparator();
        m.addItem (menuLightTheme, "Light Theme", true, controller.lightTheme);
        item (menuZoomIn, "Zoom In", "Cmd+=");
        item (menuZoomOut, "Zoom Out", "Cmd+-");
        m.addSeparator();
        for (int t = 0; t < tabs.getNumTabs(); ++t)
            item (menuTabBase + t, "Show " + tabs.getTabNames()[t], "Cmd+" + juce::String (t + 1));
        m.addSeparator();
        juce::PopupMenu sizes;
        const float now = juce::Desktop::getInstance().getGlobalScaleFactor();
        for (size_t i = 0; i < interfaceScales().size(); ++i)
            sizes.addItem (menuScaleBase + static_cast<int> (i), juce::String (juce::roundToInt (interfaceScales()[i] * 100.0f)) + "%", true,
                           std::abs (interfaceScales()[i] - now) < 0.01f);
        m.addSubMenu ("Interface Size", sizes);
    }
    else if (index == 3)
    {
        if (controller.audio.isPlaying()) item (menuPlay, "Stop", "Space");
        else
        {
            item (menuPlay, "Play from the Start", "Space");
            item (menuPlayCaret, "Play from the Caret", "Shift+Space");
        }
        item (menuToStart, "Return to Start", "Home");
        item (menuToEnd, "Skip to End", "End");
        m.addSeparator();
        {
            juce::PopupMenu::Item i ("Follow the Playhead");
            i.itemID = menuFollow;
            i.isTicked = controller.followPlayback;
            i.shortcutKeyDescription = "F";
            m.addItem (i);
        }
        // With Reduce Motion on, the view turns pages rather than scrolls.
        m.addItem (menuFollowSmooth, "    Scroll Along with the Music", ! controller.reduceMotion,
                   controller.followPlayback && controller.followStyleNow() == FollowStyle::smooth);
        m.addItem (menuFollowPage, "    Turn a Page at a Time", true, controller.followPlayback && controller.followStyleNow() == FollowStyle::page);
        if (controller.reduceMotion) m.addItem (-1, "    (Reduce Motion is on in the Mac's settings)", false);
        m.addSeparator();
        item (menuAudioSettings, "Audio and MIDI Devices" + dots);
    }
    else
    {
        item (menuHelp, "Keyboard Shortcuts", "H");
    }
    return m;
}

void MainComponent::menuItemSelected (int id, int)
{
    auto& c = controller;
    if (id >= menuTabBase && id < menuTabBase + tabs.getNumTabs())
    {
        tabs.setCurrentTabIndex (id - menuTabBase);
        return;
    }
    if (id >= menuScaleBase && id < menuScaleBase + static_cast<int> (interfaceScales().size()))
    {
        setInterfaceScale (interfaceScales()[static_cast<size_t> (id - menuScaleBase)]);
        return;
    }
    if (id >= menuGridBase && id < menuGridBase + static_cast<int> (roll::gridValues().size()))
    {
        c.setGrid (roll::gridValues()[static_cast<size_t> (id - menuGridBase)], c.grid.triplet);
        return;
    }
    if (id >= menuMeterBase && id < menuMeterBase + static_cast<int> (meterChoices().size()))
    {
        const auto [num, den] = meterChoices()[static_cast<size_t> (id - menuMeterBase)];
        c.setMeterAt (c.score.barAt (c.caret), num, den);
        return;
    }
    if (id >= menuKeyBase)
    {
        c.setKeyAt (c.score.barAt (c.caret), (id - menuKeyBase) / 32, (id - menuKeyBase) % 32);
        return;
    }
    if (id >= menuTemplateBase)
    {
        const auto name = Controller::templates()[id - menuTemplateBase];
        checkSaved ([this, name] { controller.newScore (name); });
        return;
    }
    switch (id)
    {
        case menuOpen: openDialog(); break;
        case menuImport: importDialog(); break;
        case menuSave: saveDialog (false); break;
        case menuSaveAs: saveDialog (true); break;
        case menuExportMidi: exportDialog (ExportKind::midi, false); break;
        case menuExportMidiBars: exportDialog (ExportKind::midi, true); break;
        case menuExportAudio: exportDialog (ExportKind::audio, false); break;
        case menuExportAudioBars: exportDialog (ExportKind::audio, true); break;
        case menuExportXml: exportDialog (ExportKind::musicXml, false); break;
        case menuExportXmlBars: exportDialog (ExportKind::musicXml, true); break;
        case menuUndo: c.undo(); break;
        case menuRedo: c.redo(); break;
        case menuCut: c.cutSelection(); break;
        case menuCopy: c.copySelection(); break;
        case menuPaste: c.paste(); break;
        case menuDelete: c.deleteSelection(); break;
        case menuSelectAll: c.selectAll(); break;
        case menuDuplicate: c.duplicateSelection(); break;
        case menuQuantise: c.quantiseSelection(); break;
        case menuClearAll: clearAllDialog(); break;
        case menuDrawTool: c.toggleDrawTool(); break;
        case menuStepInput: c.toggleStepInput(); break;
        case menuSnap: c.toggleSnap(); break;
        case menuTriplet: c.setGrid (c.grid.base, ! c.grid.triplet); break;
        case menuLightTheme: c.lightTheme = ! c.lightTheme; c.viewChanged(); break;
        case menuZoomIn: zoomBy (1.25f); break;
        case menuZoomOut: zoomBy (1.0f / 1.25f); break;
        case menuPlay: c.togglePlay (true); break;
        case menuPlayCaret: c.togglePlay (false); break;
        case menuToStart: returnToStart(); break;
        case menuToEnd: skipToEnd(); break;
        case menuFollow: c.toggleFollow(); break;
        case menuFollowSmooth: c.setFollowStyle (FollowStyle::smooth); break;
        case menuFollowPage: c.setFollowStyle (FollowStyle::page); break;
        case menuAudioSettings: audioSettingsDialog(); break;
        case menuHelp: showHelp(); break;
        case menuTitle: titleDialog(); break;
        case menuTempo: tempoDialog(); break;
        case menuMeterOther: meterDialog(); break;
        case menuKeyHeard: c.useHeardKey(); break;
        case menuInsertBar: c.insertBarsAtCaret (1); break;
        case menuDeleteBars: c.deleteSelectedBars(); break;
        case menuAddBars: c.setBars (c.score.bars + 4); break;
        case menuSoundApple: audio.useBuiltInSynth (false); c.setStatus ("Sound: " + audio.synthName()); break;
        case menuSoundBuiltIn: audio.useBuiltInSynth (true); c.setStatus ("Sound: " + audio.synthName()); break;
        default: break;
    }
}

juce::PopupMenu MainComponent::templateMenu()
{
    // Grouped under headings, each with how many parts it starts with.
    juce::PopupMenu m;
    std::string group = "-";
    const auto& list = scoreTemplates();
    for (size_t i = 0; i < list.size(); ++i)
    {
        const auto& t = list[i];
        if (t.group != group)
        {
            if (t.group.empty()) m.addSeparator();
            else m.addSectionHeader (t.group);
            group = t.group;
        }
        juce::PopupMenu::Item item (t.name);
        item.itemID = menuTemplateBase + static_cast<int> (i);
        const auto n = t.instruments.size();
        if (n > 1) item.shortcutKeyDescription = juce::String (static_cast<int> (n)) + " parts";
        m.addItem (item);
    }
    return m;
}

void MainComponent::showNewMenu()
{
    auto m = templateMenu();
    m.showMenuAsync (juce::PopupMenu::Options(), [this] (int r) { if (r > 0) menuItemSelected (r, 0); });
}

void MainComponent::showFileMenu()
{
    // New, Open, Save and Export under one button, so the toolbar keeps room
    // for what is used while writing (decision 0037); then undo, the score's
    // settings (0038), the sound, the input mode and the look (0039).
    juce::PopupMenu m;
    auto item = [&m] (int id, const juce::String& text, const juce::String& shortcut = {})
    {
        juce::PopupMenu::Item i (text);
        i.itemID = id;
        if (shortcut.isNotEmpty()) i.shortcutKeyDescription = key (shortcut);
        m.addItem (i);
    };
    m.addSubMenu ("New", templateMenu());
    item (menuOpen, "Open" + dots, "Cmd+O");
    item (menuImport, "Import MIDI or MusicXML" + dots);
    m.addSeparator();
    item (menuSave, "Save", "Cmd+S");
    item (menuSaveAs, "Save As" + dots, "Shift+Cmd+S");
    m.addSeparator();
    m.addSubMenu ("Export", exportMenu());
    m.addSeparator();
    {
        juce::PopupMenu::Item i ("Undo");
        i.itemID = menuUndo;
        i.isEnabled = controller.canUndo();
        i.shortcutKeyDescription = key ("Cmd+Z");
        m.addItem (i);
        juce::PopupMenu::Item r ("Redo");
        r.itemID = menuRedo;
        r.isEnabled = controller.canRedo();
        r.shortcutKeyDescription = key ("Shift+Cmd+Z");
        m.addItem (r);
        m.addItem (menuClearAll, "Clear All Notes" + dots, controller.score.lastNoteEnd() > 0);
    }
    addScoreItems (m);
    m.addSeparator();
    {
        juce::PopupMenu::Item i ("Step Input from a MIDI Keyboard");
        i.itemID = menuStepInput;
        i.isTicked = controller.stepInput;
        i.shortcutKeyDescription = "R";
        m.addItem (i);
    }
    m.addItem (menuLightTheme, "Light Piano Roll and Tracks", true, controller.lightTheme);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&toolbar.fileAnchor()),
                     [this] (int r) { if (r > 0) menuItemSelected (r, 0); });
}

void MainComponent::addScoreItems (juce::PopupMenu& m)
{
    // What was the Song tab: the song's name, tempo, time and key
    // signatures, bars and sound (decision 0038). Signatures are set at the
    // caret's bar, as the tab's buttons did.
    const auto& s = controller.score;
    const int bar = s.barAt (controller.caret);
    m.addSeparator();
    m.addSectionHeader ("This Song");
    m.addItem (menuTitle, "Title and Composer" + dots);
    m.addItem (menuTempo, "Tempo (" + juce::String (juce::roundToInt (s.tempos.front().bpm)) + " BPM)" + dots);

    const auto& meter = s.meterAtBar (bar);
    juce::PopupMenu meters;
    bool listed = false;
    for (size_t i = 0; i < meterChoices().size(); ++i)
    {
        const auto [num, den] = meterChoices()[i];
        const bool on = num == meter.num && den == meter.den;
        listed = listed || on;
        meters.addItem (menuMeterBase + static_cast<int> (i), juce::String (num) + "/" + juce::String (den), true, on);
    }
    meters.addSeparator();
    meters.addItem (menuMeterOther, "Other" + dots, true, ! listed);
    m.addSubMenu ("Time Signature at Bar " + juce::String (bar + 1) + " (" + juce::String (meter.num) + "/" + juce::String (meter.den) + ")", meters);

    const auto& k = s.keyAtBar (bar);
    juce::PopupMenu keys;
    keys.addItem (menuKeyHeard, "Use the Key It Hears");
    keys.addSeparator();
    for (size_t r = 0; r < scaleview::roots.size(); ++r)
    {
        juce::PopupMenu sc;
        for (size_t i = 0; i < scaleview::scales.size(); ++i)
            sc.addItem (menuKeyBase + static_cast<int> (r) * 32 + static_cast<int> (i), scaleview::scales[i].name, true,
                        static_cast<int> (r) == k.root && static_cast<int> (i) == k.scale);
        keys.addSubMenu (scaleview::roots[r].name, sc, true, nullptr, static_cast<int> (r) == k.root);
    }
    juce::String keyName = "?";
    if (k.root >= 0 && k.root < static_cast<int> (scaleview::roots.size()) && k.scale >= 0 && k.scale < static_cast<int> (scaleview::scales.size()))
        keyName = juce::String (scaleview::roots[static_cast<size_t> (k.root)].name) + " " + scaleview::scales[static_cast<size_t> (k.scale)].name;
    m.addSubMenu ("Key at Bar " + juce::String (bar + 1) + " (" + keyName + ")", keys);

    juce::PopupMenu bars;
    const auto [a, b] = controller.selectedBars();
    bars.addItem (menuInsertBar, "Insert a Bar at the Caret (Before Bar " + juce::String (bar + 1) + ")");
    bars.addItem (menuDeleteBars, a == b ? "Delete Bar " + juce::String (a + 1) : "Delete Bars " + juce::String (a + 1) + "-" + juce::String (b + 1));
    bars.addItem (menuAddBars, "Add 4 Bars at the End");
    m.addSubMenu ("Bars (" + juce::String (s.bars) + " in All)", bars);

    m.addSeparator();
    juce::PopupMenu sound;
    sound.addItem (menuSoundApple, "Apple General MIDI (macOS)", true, ! audio.usingBuiltInSynth());
    sound.addItem (menuSoundBuiltIn, "Built-in Synth", true, audio.usingBuiltInSynth());
    sound.addSeparator();
    sound.addItem (menuAudioSettings, "Audio and MIDI Devices" + dots);
    m.addSubMenu ("Sound", sound);
}

namespace
{
// A small box to type into, with OK and Cancel; `done` gets the box while it
// is still there to read.
void askFor (const juce::String& title, const juce::String& message, std::function<void (juce::AlertWindow&)> fill,
             std::function<void (juce::AlertWindow&)> done)
{
    auto* w = new juce::AlertWindow (title, message, juce::MessageBoxIconType::NoIcon);
    fill (*w);
    // Cancel on the left and OK, the default, on the right, as on the Mac.
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->enterModalState (true, juce::ModalCallbackFunction::create ([w, done] (int r) { if (r == 1) done (*w); }), true);
}
} // namespace

void MainComponent::titleDialog()
{
    askFor ("Title and Composer", {}, [this] (juce::AlertWindow& w)
    {
        w.addTextEditor ("title", controller.score.title, "Title");
        w.addTextEditor ("composer", controller.score.composer, "Composer");
    },
    [this] (juce::AlertWindow& w)
    {
        const auto t = w.getTextEditorContents ("title").trim().toStdString();
        const auto who = w.getTextEditorContents ("composer").trim().toStdString();
        if (t != controller.score.title || who != controller.score.composer)
            controller.edit ("Retitled", [t, who] (Score& s) { s.title = t; s.composer = who; });
    });
}

void MainComponent::tempoDialog()
{
    askFor ("Tempo", "Beats a minute, 30 to 240", [this] (juce::AlertWindow& w)
    {
        w.addTextEditor ("bpm", juce::String (juce::roundToInt (controller.score.tempos.front().bpm)), "Tempo");
        if (auto* e = w.getTextEditor ("bpm")) e->setInputRestrictions (3, "0123456789");
    },
    [this] (juce::AlertWindow& w)
    {
        const int bpm = w.getTextEditorContents ("bpm").getIntValue();
        if (bpm < 30 || bpm > 240) { controller.setStatus ("The tempo is unchanged. Choose a tempo from 30 to 240 beats a minute."); return; }
        controller.setTempo (bpm);
    });
}

void MainComponent::meterDialog()
{
    const int bar = controller.score.barAt (controller.caret);
    askFor ("Time Signature at Bar " + juce::String (bar + 1), {}, [this, bar] (juce::AlertWindow& w)
    {
        const auto& m = controller.score.meterAtBar (bar);
        juce::StringArray nums, dens { "2", "4", "8", "16" };
        for (int n = 1; n <= 16; ++n) nums.add (juce::String (n));
        w.addComboBox ("num", nums, "Beats in a bar");
        w.addComboBox ("den", dens, "Each beat a");
        if (auto* b = w.getComboBoxComponent ("num")) b->setText (juce::String (m.num), juce::dontSendNotification);
        if (auto* b = w.getComboBoxComponent ("den")) b->setText (juce::String (m.den), juce::dontSendNotification);
    },
    [this, bar] (juce::AlertWindow& w)
    {
        const auto* n = w.getComboBoxComponent ("num");
        const auto* d = w.getComboBoxComponent ("den");
        if (n != nullptr && d != nullptr) controller.setMeterAt (bar, n->getText().getIntValue(), d->getText().getIntValue());
    });
}

void MainComponent::showExportMenu()
{
    exportMenu().showMenuAsync (juce::PopupMenu::Options(), [this] (int r) { if (r > 0) menuItemSelected (r, 0); });
}

juce::PopupMenu MainComponent::exportMenu()
{
    juce::PopupMenu m;
    const auto [a, b] = controller.selectedBars();
    const juce::String bars = a == b ? "bar " + juce::String (a + 1) : "bars " + juce::String (a + 1) + "-" + juce::String (b + 1);
    m.addItem (menuExportMidi, "Song as MIDI" + dots);
    m.addItem (menuExportMidiBars, "Selection (" + bars + ") as MIDI" + dots);
    m.addSeparator();
    m.addItem (menuExportAudio, "Song as Audio (WAV)" + dots);
    m.addItem (menuExportAudioBars, "Selection (" + bars + ") as Audio (WAV)" + dots);
    m.addSeparator();
    m.addItem (menuExportXml, "Song as MusicXML (for Dorico, MuseScore, Sibelius)" + dots);
    m.addItem (menuExportXmlBars, "Selection (" + bars + ") as MusicXML" + dots);
    return m;
}

//==============================================================================
// Files

void MainComponent::checkSaved (std::function<void()> then)
{
    if (! controller.dirty) { then(); return; }
    // The Mac's own question, in its own words (decision 0048).
    auto opts = juce::MessageBoxOptions()
                    .withIconType (juce::MessageBoxIconType::QuestionIcon)
                    .withTitle ("Do you want to save the changes you made to " + juce::String (controller.score.title) + "?")
                    .withMessage ("Your changes will be lost if you don't save them.")
                    .withButton ("Save")
                    .withButton ("Don't Save")
                    .withButton ("Cancel")
                    .withAssociatedComponent (this);
    juce::NativeMessageBox::showAsync (opts, [this, then] (int r)
    {
        if (r == 0) saveDialog (false, then);
        else if (r == 1) then();
    });
}

void MainComponent::openFile (const juce::File& f)
{
    juce::String error;
    if (! controller.load (f, error))
        showError ("Could not open " + f.getFileName(), error);
    lastFolder = f.getParentDirectory();
}

void MainComponent::openDialog()
{
    checkSaved ([this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Open a project, a MIDI file or a MusicXML file", lastFolder,
                                                      "*.miderator;*.noterator;*.mid;*.midi;*.musicxml;*.mxl;*.xml");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
        {
            const auto f = fc.getResult();
            if (f.existsAsFile()) openFile (f);
        });
    });
}

void MainComponent::importDialog()
{
    chooser = std::make_unique<juce::FileChooser> ("Import a MIDI or MusicXML file into this song, at the caret's bar", lastFolder,
                                                  "*.mid;*.midi;*.musicxml;*.mxl;*.xml");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
    {
        const auto f = fc.getResult();
        if (! f.existsAsFile()) return;
        juce::String error;
        if (! controller.importFile (f, error))
            showError ("Could not import " + f.getFileName(), error);
        lastFolder = f.getParentDirectory();
    });
}

void MainComponent::saveDialog (bool saveAs, std::function<void()> then)
{
    if (! saveAs && controller.file.existsAsFile())
    {
        juce::String error;
        if (controller.save (controller.file, error)) { if (then) then(); }
        else showError ("Could not save", error + "\n\nTry Save As to choose another folder, or check that the disk has room.");
        return;
    }
    const auto suggested = (lastFolder.isDirectory() ? lastFolder : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory))
                               .getChildFile (juce::File::createLegalFileName (controller.score.title) + ".miderator");
    chooser = std::make_unique<juce::FileChooser> ("Save the project", suggested, "*.miderator");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this, then] (const juce::FileChooser& fc)
    {
        auto f = fc.getResult();
        if (f == juce::File()) return;
        f = f.withFileExtension ("miderator");
        juce::String error;
        if (controller.save (f, error)) { lastFolder = f.getParentDirectory(); if (then) then(); }
        else showError ("Could not save", error + "\n\nTry Save As to choose another folder, or check that the disk has room.");
    });
}

void MainComponent::exportDialog (ExportKind kind, bool selectedBars)
{
    ExportRange range;
    juce::String suffix;
    if (selectedBars)
    {
        const auto [a, b] = controller.selectedBars();
        range.from = controller.score.barStart (a);
        range.to = controller.score.barStart (b + 1);
        suffix = a == b ? " bar " + juce::String (a + 1) : " bars " + juce::String (a + 1) + "-" + juce::String (b + 1);
    }
    const bool audioFile = kind == ExportKind::audio;
    const juce::String ext = audioFile ? "wav" : kind == ExportKind::musicXml ? "musicxml" : "mid";
    const auto suggested = (lastFolder.isDirectory() ? lastFolder : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory))
                               .getChildFile (juce::File::createLegalFileName (juce::String (controller.score.title) + suffix) + "." + ext);
    chooser = std::make_unique<juce::FileChooser> (audioFile ? "Export audio" : kind == ExportKind::musicXml ? "Export MusicXML" : "Export MIDI",
                                                   suggested, "*." + ext);
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this, kind, audioFile, range, ext] (const juce::FileChooser& fc)
    {
        auto f = fc.getResult();
        if (f == juce::File()) return;
        f = f.withFileExtension (ext);
        lastFolder = f.getParentDirectory();
        juce::String error;
        if (kind == ExportKind::musicXml)
        {
            if (exportMusicXml (controller.score, range, f, error)) controller.setStatus ("Exported " + f.getFileName());
            else showError ("Could not export", error + "\n\nTry another folder, or check that the disk has room.");
            return;
        }
        if (! audioFile)
        {
            if (exportMidi (controller.score, range, true, f, error)) controller.setStatus ("Exported " + f.getFileName());
            else showError ("Could not export", error + "\n\nTry another folder, or check that the disk has room.");
            return;
        }
        // Audio is rendered through a synth of its own, off the audio thread.
        // The window owns itself until it finishes.
        struct Render : public juce::ThreadWithProgressWindow
        {
            Render (const Score& s, ExportRange r, std::unique_ptr<SynthRack> sy, juce::String d, juce::File fl, Controller& c)
                : juce::ThreadWithProgressWindow ("Exporting audio" + dots, true, true),
                  score (s), range (r), synth (std::move (sy)), desc (std::move (d)), file (fl), controller (c) {}
            void run() override
            {
                ok = renderAudio (score, range, *synth, file, [this] (double p) { setProgress (p); return ! threadShouldExit(); }, error);
            }
            void threadComplete (bool cancelled) override
            {
                if (ok) controller.setStatus ("Exported " + file.getFileName() + " through " + desc);
                else if (! cancelled)
                    juce::NativeMessageBox::showAsync (juce::MessageBoxOptions()
                                                           .withIconType (juce::MessageBoxIconType::WarningIcon)
                                                           .withTitle ("Could not export audio")
                                                           .withMessage (error)
                                                           .withButton ("OK"),
                                                       nullptr);
                juce::MessageManager::callAsync ([this] { delete this; });
            }
            Score score;
            ExportRange range;
            std::unique_ptr<SynthRack> synth;
            juce::String desc;
            juce::File file;
            Controller& controller;
            bool ok = false;
            juce::String error;
        };
        // Every synth the score needs is made here, on the message thread.
        auto synth = std::make_unique<SynthRack> (audio.usingBuiltInSynth());
        synth->ensureBanks (banksFor (controller.score));
        const auto desc = synth->description();
        audio.stop();
        (new Render (controller.score, range, std::move (synth), desc, f, controller))->launchThread();
    });
}

bool MainComponent::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& f : files)
        for (const char* ext : { ".mid", ".midi", ".miderator", ".noterator", ".musicxml", ".mxl", ".xml" })
            if (f.endsWithIgnoreCase (ext)) return true;
    return false;
}

void MainComponent::filesDropped (const juce::StringArray& files, int, int)
{
    for (const auto& path : files)
    {
        const juce::File f (path);
        if (f.hasFileExtension ("miderator;noterator")) { checkSaved ([this, f] { openFile (f); }); return; }
        // A MIDI or MusicXML file dropped on a score joins it, at the caret's bar.
        juce::String error;
        if (! controller.importFile (f, error))
            showError ("Could not import " + f.getFileName(), error);
    }
}

//==============================================================================

void MainComponent::audioSettingsDialog()
{
    auto* selector = new juce::AudioDeviceSelectorComponent (audio.devices(), 0, 0, 2, 2, true, false, true, false);
    selector->setSize (520, 420);
    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned (selector);
    o.dialogTitle = "Audio and MIDI devices";
    o.dialogBackgroundColour = theme::ground;
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = true;
    o.resizable = false;
    o.launchAsync();
}

void MainComponent::clearAllDialog()
{
    if (controller.score.lastNoteEnd() <= 0) { controller.setStatus ("There are no notes to clear."); return; }
    // It asks, as removing a part with notes in it does; Undo is there too.
    const auto options = juce::MessageBoxOptions()
                             .withIconType (juce::MessageBoxIconType::QuestionIcon)
                             .withTitle ("Clear every note?")
                             .withMessage ("Every part keeps its instrument, and the tempo, key and bars stay. Undo brings the notes back.")
                             .withButton ("Clear")
                             .withButton ("Cancel")
                             .withAssociatedComponent (this);
    juce::NativeMessageBox::showAsync (options, [safe = juce::Component::SafePointer<MainComponent> (this)] (int r)
    {
        if (safe != nullptr && r == 0) safe->controller.clearAllNotes();
    });
}

void MainComponent::returnToStart()
{
    controller.returnToStart();
    workspace.timeline.scrollTo (0);
}

void MainComponent::skipToEnd()
{
    controller.skipToEnd();
    // The end near the right, so the last bars are in view.
    workspace.timeline.showAt (controller.caret, 0.8);
}

void MainComponent::showHelp()
{
    const juce::String text =
        "THE PIANO ROLL\n"
        "Click a note to choose and hear it; Shift+click for more; drag on empty space to lasso\n"
        "Drag a note to move it (Alt+drag copies it); drag its right or left end to stretch it\n"
        "Double-click empty space to draw a note; D switches to the Draw tool and back\n"
        "Drag in the Velocity lane to set how hard notes are played\n"
        "Right-click for Delete, Duplicate and Quantise\n\n"
        "THE GRID\n"
        "1 - 6  a bar, 1/2, 1/4, 1/8, 1/16, 1/32     T  triplets     Q  quantise\n\n"
        "EDITING\n"
        "Up / Down  a semitone     Shift or Cmd+Up / Down  an octave\n"
        "Left / Right  move the selection a grid step (or the caret, with nothing selected)\n"
        "Alt+Up / Down  the part above or below     Delete  remove\n"
        "Cmd+C / X / V  copy, cut, paste at the caret     Cmd+D  duplicate     Cmd+Z  undo\n"
        "Clear All (toolbar)  every note out of every part, after asking\n"
        "R  step input: a MIDI keyboard writes at the caret, a grid step at a time\n\n"
        "THE TRACKS\n"
        "Click a part's name to show it in the piano roll; double-click to select all of it\n"
        "Click a bar to choose it; drag across bars and parts to choose more\n"
        "(drag along the Chords lane for every part; Shift+click stretches the choice)\n"
        "Click the bar numbers to put the caret there\n\n"
        "LISTENING\n"
        "Space  play from bar 1, or stop     Shift+Space  play from the caret\n"
        "Home  back to the start     End  on to the end of the music\n"
        "F  Follow: the view scrolls along with the music as it plays, or stays put\n"
        "(or turns a page at a time: Play menu)\n"
        "Click a chord in the Chords lane to hear it, or the Scale lane to hear the scale\n"
        "Cmd+scroll zooms in time; Alt+scroll makes the keys taller or shorter\n\n"
        "GENERATING\n"
        "Choose some bars (or click a part's name), choose a generator, press\n"
        "Generate, click a result to hear it, and Insert to put it in.\n"
        "Generate Notes fills the bars chosen: the tune on top, the bass below,\n"
        "chords between. Suggest Notes and Vary Notes work on the notes you select.\n\n"
        "BLOCKS\n"
        "Pick a chord, arpeggio, run or interval, click a degree\n"
        "to see and hear it, and Insert to put it at the caret - the caret moves\n"
        "on, so blocks can be laid one after another.\n\n"
        "THE WINDOW\n"
        "Tab / Shift+Tab  move between the buttons and menus     Space or Return  press one\n"
        "Esc  back to the roll     Cmd+1 / 2 / 3  the Generate, Blocks and Parts tabs\n"
        "View > Interface Size  everything larger, up to 200%";
    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Keyboard Shortcuts", text);
}

} // namespace nt
