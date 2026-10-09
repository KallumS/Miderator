/*
    Controller - the open score and everything that can be done to it.

    The window's pieces (the tracks, the piano roll, the toolbar, the panels)
    never change the score themselves: they ask this, and it makes the change
    through Edit.h or Roll.h, keeps the score from before for undo, re-reads
    the chords, the keys and what each instrument cannot play, hands the new
    music to playback, and tells everyone it changed. One place for all of
    that is what keeps undo, the screen and the sound from ever disagreeing.
*/

#pragma once

#include "AudioEngine.h"
#include "Detect.h"
#include "Edit.h"
#include "Generators.h"
#include "LuaEngine.h"
#include "Roll.h"
#include "Score.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <map>

namespace nt
{

// Whole bars across some parts, chosen by dragging over the page: where a
// generator's music goes (decision 0019).
struct BarRange
{
    int first = -1, last = -1;
    std::vector<uint32_t> parts;   // top to bottom
    bool active() const { return first >= 0 && last >= first && ! parts.empty(); }
    int bars() const { return active() ? last - first + 1 : 0; }
};

class Controller : public juce::ChangeBroadcaster
{
public:
    explicit Controller (AudioEngine& audioEngine);

    //==========================================================================
    // State, read by everything that draws
    Score score;
    Selection selection;
    BarRange range;               // set: the selection is everything in these bars
    uint32_t caretPart = 0;       // the part the piano roll shows and writes into
    Tick caret = 0;               // where playback starts and pasting and step input go
    roll::Grid grid;              // what notes snap to
    bool drawTool = false;        // a click on the roll draws a note (decision 0028)
    bool stepInput = false;       // a MIDI keyboard writes at the caret
    bool lightTheme = false;      // dark unless the user asks for light (decision 0031)
    bool followPlayback = true;   // the view turns a page with the playhead (decision 0033)
    float zoom = 32.0f;           // pixels per quarter note, across the tracks and the roll
    float rowHeight = 12.0f;      // pixels per key in the piano roll

    roll::Warnings warnings;      // what each instrument cannot play, note by note
    std::vector<ChordSpan> chords;
    std::vector<KeySpan> keys;
    juce::File file;
    bool dirty = false;
    bool auditioning = false;     // playing a generated result, not the score
    juce::String status;          // the last thing worth telling the user

    AudioEngine& audio;
    LuaEngine lua;

    //==========================================================================
    // Changes
    void edit (const juce::String& what, const std::function<void (Score&)>& fn);
    bool canUndo() const { return ! undoStack.empty(); }
    bool canRedo() const { return ! redoStack.empty(); }
    void undo();
    void redo();
    void selectionChanged();
    void viewChanged();           // zoom, theme, grid: a repaint, no undo
    void setStatus (const juce::String& s);

    //==========================================================================
    // Writing
    void setCaret (uint32_t partId, Tick t);
    void moveCaret (int direction);                  // a grid step either way
    void caretToPart (int direction);
    // A note drawn on the roll: `length` 0 means one grid step.
    uint32_t drawNoteAt (uint32_t partId, Tick at, int pitch, Tick length = 0);
    void writePitch (int pitch, bool addToChord);    // step input, from a MIDI key
    void setGrid (Tick base, bool triplet);
    void toggleSnap();
    void toggleDrawTool();
    void toggleStepInput();

    //==========================================================================
    // The selection
    void select (const Selection& s, bool preview = false);
    void selectAll();
    void selectNext (int direction, bool extend);
    void transposeSelection (int semitones);
    void moveSelection (int direction);              // a grid step either way
    // A drag on the roll: by `by` ticks and `semitones`; copies with `copy`.
    void dragSelection (Tick by, int semitones, bool copy);
    // A drag on a note's end (or its start, `fromStart`).
    void stretchSelection (Tick by, bool fromStart);
    void setVelocities (const std::map<uint32_t, int>& velocities);
    void quantiseSelection();
    void duplicateSelection();                       // a copy straight after it
    void deleteSelection();
    void copySelection();
    void cutSelection();
    void paste();
    void previewSelection();
    // Bars `a` to `b` across the parts at indices `fromPart` to `toPart`,
    // in either order; the notes in them become the selection.
    void selectRange (int a, int b, int fromPart, int toPart);
    // "Bars 2-5, Violin I to Cello", for whatever shows the range.
    juce::String rangeText() const;
    // The bars the range or the selection covers, or the caret's bar.
    std::pair<int, int> selectedBars() const;

    //==========================================================================
    // Parts and score
    uint32_t addPart (const std::string& instrumentId);
    void removePart (uint32_t partId);
    void movePart (uint32_t partId, int direction);
    void setPartInstrument (uint32_t partId, const std::string& instrumentId);
    void setKeyAt (int bar, int root, int scale);
    void setMeterAt (int bar, int num, int den);
    void setTempo (double bpm);
    void setBars (int bars);
    void insertBarsAtCaret (int count);
    void deleteSelectedBars();

    //==========================================================================
    // Sound
    void togglePlay();
    void toggleFollow();
    void playFrom (Tick t);
    void stop();
    void previewPitches (const std::vector<int>& pitches, uint32_t partId, double seconds = 0.9);
    Tick playheadTick() const;

    //==========================================================================
    // Files
    void newScore (const juce::String& templateName);
    bool load (const juce::File& f, juce::String& error);
    // Adds a MIDI or MusicXML file's parts to this score, at the caret's bar.
    bool importFile (const juce::File& f, juce::String& error);
    // A project, MIDI file or MusicXML file (.musicxml, .xml, .mxl) as a score.
    static bool readScoreFile (const juce::File& f, Score& out, juce::String& error);
    bool save (const juce::File& f, juce::String& error);
    static juce::StringArray templates();

    // The part a generator writes into and the context it is asked in.
    GeneratorContext generatorContext (bool withSelection) const;
    // A result from a generator that works on the selection goes beside or
    // after it (decision 0011); any other goes into the caret's part, or
    // fills the selected bars when there are some (decision 0019).
    void insertGenerated (const GeneratedResult& r, bool fromSelection, const std::string& generatorId);
    void auditionGenerated (const GeneratedResult& r, bool fromSelection, const std::string& generatorId);

    const Part* caretPartPtr() const { return score.partById (caretPart); }
    int keyRootAt (Tick t) const { return score.keyAtBar (score.barAt (t)).root; }

private:
    std::vector<Score> undoStack, redoStack;
    std::vector<Note> clipboard;
    bool clipboardFromDrums = false;
    Tick lastWriteStart = 0;
    Tick lastWriteLength = PPQ;
    double lastMidiTime = 0;
    Tick midiChordAt = -1;

    void refresh();               // re-detect, re-check, re-send to playback
    // Puts a result into `s` where it belongs; returns where it starts.
    Tick place (Score& s, const GeneratedResult& r, bool fromSelection, const std::string& generatorId, InsertReport& report) const;
    void ensureCaretPart();
};

} // namespace nt
