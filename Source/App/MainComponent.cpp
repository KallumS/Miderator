#include "MainComponent.h"

#include "Exporter.h"
#include "Templates.h"

namespace nt
{

namespace
{
enum MenuIds
{
    menuNew = 1, menuOpen, menuImport, menuSave, menuSaveAs, menuExportMidi, menuExportMidiBars, menuExportAudio, menuExportAudioBars,
    menuExportXml, menuExportXmlBars,
    menuUndo = 100, menuRedo, menuCut, menuCopy, menuPaste, menuDelete, menuSelectAll, menuVoice,
    menuPlay = 200, menuNoteInput, menuLightPage, menuTransposed, menuZoomIn, menuZoomOut, menuAudioSettings, menuHelp,
    menuTemplateBase = 1000
};

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
    addAndMakeVisible (view);
    addAndMakeVisible (tabs);
    addAndMakeVisible (statusBar);
    tabs.setTabBarDepth (30);
    tabs.setOutline (0);
    tabs.addTab ("Generate", theme::control, &generatorPanel, false);
    tabs.addTab ("Blocks", theme::control, &blocksPanel, false);
    tabs.addTab ("Parts", theme::control, &partsPanel, false);
    tabs.addTab ("Score", theme::control, &scorePanel, false);
    tabs.setColour (juce::TabbedComponent::backgroundColourId, theme::ground);

    toolbar.onNew = [this] { showNewMenu(); };
    toolbar.onOpen = [this] { openDialog(); };
    toolbar.onSave = [this] { saveDialog (false); };
    toolbar.onExport = [this] { showExportMenu(); };
    toolbar.onSettings = [this] { showSettingsMenu(); };
    scorePanel.onAudioSettings = [this] { audioSettingsDialog(); };

    // A MIDI keyboard writes in note input, and is always heard.
    audio.onMidiInput = [this] (const juce::MidiMessage& m)
    {
        if (! m.isNoteOn() || ! controller.input.noteInput) return;
        // Keys pressed within a moment of each other are one chord.
        static double lastTime = 0;
        const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
        const bool chord = now - lastTime < 0.06;
        lastTime = now;
        controller.writePitch (m.getNoteNumber(), chord);
    };
    audio.onPlaybackFinished = [this] { controller.stop(); };

    juce::PropertiesFile::Options prefs;
    prefs.applicationName = "Noterator";
    prefs.filenameSuffix = "settings";
    prefs.osxLibrarySubFolder = "Application Support";
    preferences.setStorageParameters (prefs);
    if (auto* p = preferences.getUserSettings())
    {
        controller.lightPage = ! p->getBoolValue ("darkPage", false);
        controller.zoom = static_cast<float> (juce::jlimit (5.0, 24.0, p->getDoubleValue ("zoom", controller.zoom)));
        controller.viewChanged();
    }

    controller.addChangeListener (this);
    setWantsKeyboardFocus (true);
    setSize (1440, 880);
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
    controller.removeChangeListener (this);
    audio.stop();
}

void MainComponent::paint (juce::Graphics& g) { g.fillAll (theme::ground); }

void MainComponent::resized()
{
    auto r = getLocalBounds();
    toolbar.setBounds (r.removeFromTop (46));
    statusBar.setBounds (r.removeFromBottom (26));
    tabs.setBounds (r.removeFromRight (std::min (380, getWidth() / 3)));
    view.setBounds (r);
}

void MainComponent::changeListenerCallback (juce::ChangeBroadcaster*)
{
    if (auto* p = preferences.getUserSettings())
    {
        if (p->getBoolValue ("darkPage", false) == controller.lightPage) p->setValue ("darkPage", ! controller.lightPage);
        if (std::abs (p->getDoubleValue ("zoom", 0.0) - controller.zoom) > 0.01) p->setValue ("zoom", controller.zoom);
    }
    updateTitle();
    menuItemsChanged();
}

void MainComponent::updateTitle()
{
    if (auto* w = findParentComponentOfClass<juce::DocumentWindow>())
        w->setName ((controller.dirty ? "* " : "") + juce::String (controller.score.title) + " - Noterator");
}

//==============================================================================
// Keys

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
        if (code == 'C') { c.copySelection(); return true; }
        if (code == 'X') { c.cutSelection(); return true; }
        if (code == 'V') { c.paste(); return true; }
        if (code == '=' || code == '+') { view.zoomBy (1.15f); return true; }
        if (code == '-') { view.zoomBy (1.0f / 1.15f); return true; }
        if (code == juce::KeyPress::upKey) { c.transposeSelection (12); return true; }
        if (code == juce::KeyPress::downKey) { c.transposeSelection (-12); return true; }
        return false;
    }

    if (code == juce::KeyPress::spaceKey) { c.togglePlay(); return true; }
    if (code == juce::KeyPress::escapeKey)
    {
        if (c.audio.isPlaying()) c.stop();
        else if (c.input.noteInput) c.toggleNoteInput();
        else c.select ({});
        return true;
    }
    if (code == juce::KeyPress::deleteKey || code == juce::KeyPress::backspaceKey) { c.deleteSelection(); return true; }
    if (code == juce::KeyPress::upKey)
    {
        if (mods.isAltDown()) c.caretToPart (-1);
        else c.transposeSelection (1);
        return true;
    }
    if (code == juce::KeyPress::downKey)
    {
        if (mods.isAltDown()) c.caretToPart (1);
        else c.transposeSelection (-1);
        return true;
    }
    if (code == juce::KeyPress::leftKey || code == juce::KeyPress::rightKey)
    {
        const int dir = code == juce::KeyPress::leftKey ? -1 : 1;
        if (mods.isAltDown()) c.moveSelection (dir);
        else if (c.input.noteInput || c.selection.empty()) c.moveCaret (dir);
        else c.selectNext (dir, mods.isShiftDown());
        view.scrollToTick (c.caret);
        return true;
    }
    if (code == juce::KeyPress::returnKey)
    {
        c.toggleNoteInput();
        return true;
    }

    const auto lower = juce::CharacterFunctions::toLowerCase (ch);
    if (lower >= 'a' && lower <= 'g')
    {
        static const int letterOf[7] = { 5, 6, 0, 1, 2, 3, 4 };   // a..g -> A..G as C-based letters
        if (! c.input.noteInput) c.toggleNoteInput();
        c.typeLetter (letterOf[lower - 'a'], mods.isShiftDown());
        view.scrollToTick (c.caret);
        return true;
    }
    if (ch >= '1' && ch <= '7')
    {
        static const Tick values[7] = { PPQ / 16, PPQ / 8, PPQ / 4, PPQ / 2, PPQ, 2 * PPQ, 4 * PPQ };
        c.setDuration (values[ch - '1']);
        return true;
    }
    if (ch == '0') { c.typeRest(); view.scrollToTick (c.caret); return true; }
    if (ch == '.') { c.toggleDot(); return true; }
    if (lower == 't') { c.toggleTriplet(); return true; }
    if (lower == 'n') { c.toggleNoteInput(); return true; }
    if (lower == 'v') { c.toggleVoiceOfSelection(); return true; }
    if (lower == 'p') { c.previewSelection(); return true; }
    if (ch == '[' || ch == ']') { c.lengthenSelection (ch == ']' ? 1 : -1); return true; }
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
        item (menuOpen, "Open...", "Cmd+O");
        item (menuImport, "Import MIDI or MusicXML into this score...");
        m.addSeparator();
        item (menuSave, "Save", "Cmd+S");
        item (menuSaveAs, "Save As...", "Shift+Cmd+S");
        m.addSeparator();
        item (menuExportMidi, "Export Score as MIDI...");
        item (menuExportMidiBars, "Export Selected Bars as MIDI...");
        item (menuExportAudio, "Export Score as Audio (WAV)...");
        item (menuExportAudioBars, "Export Selected Bars as Audio (WAV)...");
        item (menuExportXml, "Export Score as MusicXML...");
        item (menuExportXmlBars, "Export Selected Bars as MusicXML...");
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
        m.addSeparator();
        item (menuVoice, "Swap Voice of Selection", "V", ! controller.selection.empty());
    }
    else if (index == 2)
    {
        m.addItem (menuNoteInput, "Note Input", true, controller.input.noteInput);
        m.addItem (menuLightPage, "Dark Page", true, ! controller.lightPage);
        m.addItem (menuTransposed, "Transposed Score", true, controller.transposedScore);
        m.addSeparator();
        item (menuZoomIn, "Zoom In", "Cmd+=");
        item (menuZoomOut, "Zoom Out", "Cmd+-");
    }
    else if (index == 3)
    {
        item (menuPlay, controller.audio.isPlaying() ? "Stop" : "Play", "Space");
        m.addSeparator();
        item (menuAudioSettings, "Audio and MIDI Devices...");
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
        case menuVoice: c.toggleVoiceOfSelection(); break;
        case menuNoteInput: c.toggleNoteInput(); break;
        case menuLightPage: c.lightPage = ! c.lightPage; c.viewChanged(); break;
        case menuTransposed: c.transposedScore = ! c.transposedScore; c.viewChanged(); break;
        case menuZoomIn: view.zoomBy (1.15f); break;
        case menuZoomOut: view.zoomBy (1.0f / 1.15f); break;
        case menuPlay: c.togglePlay(); break;
        case menuAudioSettings: audioSettingsDialog(); break;
        case menuHelp: showHelp(); break;
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

void MainComponent::showExportMenu()
{
    juce::PopupMenu m;
    const auto [a, b] = controller.selectedBars();
    const juce::String bars = a == b ? "bar " + juce::String (a + 1) : "bars " + juce::String (a + 1) + "-" + juce::String (b + 1);
    m.addItem (menuExportMidi, "Score as MIDI...");
    m.addItem (menuExportMidiBars, "Selection (" + bars + ") as MIDI...");
    m.addSeparator();
    m.addItem (menuExportAudio, "Score as audio (WAV)...");
    m.addItem (menuExportAudioBars, "Selection (" + bars + ") as audio (WAV)...");
    m.addSeparator();
    m.addItem (menuExportXml, "Score as MusicXML (for Dorico, MuseScore, Sibelius)...");
    m.addItem (menuExportXmlBars, "Selection (" + bars + ") as MusicXML...");
    m.showMenuAsync (juce::PopupMenu::Options(), [this] (int r) { if (r > 0) menuItemSelected (r, 0); });
}

void MainComponent::showSettingsMenu()
{
    juce::PopupMenu m;
    m.addSectionHeader ("Playing through: " + audio.synthName());
    m.addItem (1, "Apple General MIDI (macOS)", true, ! audio.usingBuiltInSynth());
    m.addItem (2, "Built-in synth", true, audio.usingBuiltInSynth());
    m.addSeparator();
    m.addItem (3, "Audio and MIDI devices...");
    m.showMenuAsync (juce::PopupMenu::Options(), [this] (int r)
    {
        if (r == 1 || r == 2) { audio.useBuiltInSynth (r == 2); controller.setStatus ("Sound: " + audio.synthName()); }
        if (r == 3) audioSettingsDialog();
    });
}

//==============================================================================
// Files

void MainComponent::checkSaved (std::function<void()> then)
{
    if (! controller.dirty) { then(); return; }
    auto opts = juce::MessageBoxOptions()
                    .withIconType (juce::MessageBoxIconType::QuestionIcon)
                    .withTitle ("Save the changes to " + juce::String (controller.score.title) + "?")
                    .withMessage ("They will be lost if you don't.")
                    .withButton ("Save")
                    .withButton ("Don't Save")
                    .withButton ("Cancel")
                    .withAssociatedComponent (this);
    juce::AlertWindow::showAsync (opts, [this, then] (int r)
    {
        if (r == 1) saveDialog (false, then);
        else if (r == 2) then();
    });
}

void MainComponent::openFile (const juce::File& f)
{
    juce::String error;
    if (! controller.load (f, error))
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Could not open " + f.getFileName(), error);
    lastFolder = f.getParentDirectory();
}

void MainComponent::openDialog()
{
    checkSaved ([this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Open a score, a MIDI file or a MusicXML file", lastFolder,
                                                      "*.noterator;*.mid;*.midi;*.musicxml;*.mxl;*.xml");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
        {
            const auto f = fc.getResult();
            if (f.existsAsFile()) openFile (f);
        });
    });
}

void MainComponent::importDialog()
{
    chooser = std::make_unique<juce::FileChooser> ("Import a MIDI or MusicXML file into this score, at the caret's bar", lastFolder,
                                                  "*.mid;*.midi;*.musicxml;*.mxl;*.xml");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
    {
        const auto f = fc.getResult();
        if (! f.existsAsFile()) return;
        juce::String error;
        if (! controller.importFile (f, error))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Could not import " + f.getFileName(), error);
        lastFolder = f.getParentDirectory();
    });
}

void MainComponent::saveDialog (bool saveAs, std::function<void()> then)
{
    if (! saveAs && controller.file.existsAsFile())
    {
        juce::String error;
        if (controller.save (controller.file, error)) { if (then) then(); }
        else juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Could not save", error);
        return;
    }
    const auto suggested = (lastFolder.isDirectory() ? lastFolder : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory))
                               .getChildFile (juce::File::createLegalFileName (controller.score.title) + ".noterator");
    chooser = std::make_unique<juce::FileChooser> ("Save the score", suggested, "*.noterator");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this, then] (const juce::FileChooser& fc)
    {
        auto f = fc.getResult();
        if (f == juce::File()) return;
        f = f.withFileExtension ("noterator");
        juce::String error;
        if (controller.save (f, error)) { lastFolder = f.getParentDirectory(); if (then) then(); }
        else juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Could not save", error);
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
            else juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Could not export", error);
            return;
        }
        if (! audioFile)
        {
            if (exportMidi (controller.score, range, true, f, error)) controller.setStatus ("Exported " + f.getFileName());
            else juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Could not export", error);
            return;
        }
        // Audio is rendered through a synth of its own, off the audio thread.
        // The window owns itself until it finishes.
        struct Render : public juce::ThreadWithProgressWindow
        {
            Render (const Score& s, ExportRange r, std::unique_ptr<SynthRack> sy, juce::String d, juce::File fl, Controller& c)
                : juce::ThreadWithProgressWindow ("Exporting audio...", true, true),
                  score (s), range (r), synth (std::move (sy)), desc (std::move (d)), file (fl), controller (c) {}
            void run() override
            {
                ok = renderAudio (score, range, *synth, file, [this] (double p) { setProgress (p); return ! threadShouldExit(); }, error);
            }
            void threadComplete (bool cancelled) override
            {
                if (ok) controller.setStatus ("Exported " + file.getFileName() + " through " + desc);
                else if (! cancelled)
                    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Could not export audio", error);
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
        for (const char* ext : { ".mid", ".midi", ".noterator", ".musicxml", ".mxl", ".xml" })
            if (f.endsWithIgnoreCase (ext)) return true;
    return false;
}

void MainComponent::filesDropped (const juce::StringArray& files, int, int)
{
    for (const auto& path : files)
    {
        const juce::File f (path);
        if (f.hasFileExtension ("noterator")) { checkSaved ([this, f] { openFile (f); }); return; }
        // A MIDI or MusicXML file dropped on a score joins it, at the caret's bar.
        juce::String error;
        if (! controller.importFile (f, error))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Could not import " + f.getFileName(), error);
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

void MainComponent::showHelp()
{
    const juce::String text =
        "WRITING\n"
        "N or Return  note input on and off\n"
        "A - G  write that note at the caret (Shift adds it to the chord)\n"
        "0  a rest     1 - 7  note values (5 is a quarter)     .  dot     T  triplet\n"
        "Click a line or space in note input to write the note the key gives it.\n"
        "A MIDI keyboard writes in note input, and always plays.\n\n"
        "EDITING\n"
        "Click a note to select and hear it; double-click for its whole chord\n"
        "Click an empty bar to choose it; drag across bars and parts to choose more\n"
        "(drag along the Chords lane for every part; Shift+click stretches the choice)\n"
        "Shift+drag picks out notes; drag a note up or down to move it\n"
        "Up / Down  a semitone     Cmd+Up / Down  an octave\n"
        "Left / Right  the next note (Shift extends)     Alt+Left / Right  move the selection\n"
        "[ and ]  halve or double     V  swap voice     Delete  remove\n"
        "Cmd+C / X / V  copy, cut, paste at the caret     Cmd+Z  undo\n\n"
        "LISTENING\n"
        "Space  play from the selection or the caret, or stop\n"
        "Click a chord in the Chords lane to hear it, or the Scale lane to hear the scale\n\n"
        "GENERATING\n"
        "Choose some bars (or put the caret in a part), choose a generator, press\n"
        "Generate, click a result to hear it, and Insert to put it in.\n"
        "Generate Notes fills the bars chosen: the tune on top, the bass below,\n"
        "chords between. Suggest Notes and Vary Notes work on the music you select.\n\n"
        "BLOCKS\n"
        "Pick a chord, arpeggio, run or interval, click a degree\n"
        "to see and hear it, and Insert to put it at the caret - the caret moves\n"
        "on, so blocks can be laid one after another.";
    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Noterator - keys", text);
}

} // namespace nt
