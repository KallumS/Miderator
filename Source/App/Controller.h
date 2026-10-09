/*
    Controller - the open score and everything that can be done to it.

    The window's pieces (the page, the toolbar, the panels) never change the
    score themselves: they ask this, and it makes the change through Edit.h,
    keeps the score from before for undo, lays the page out again, re-reads
    the chords and keys, hands the new music to playback, and tells everyone
    it changed. One place for all of that is what keeps undo, the page and
    the sound from ever disagreeing.
*/

#pragma once

#include "AudioEngine.h"
#include "Detect.h"
#include "Edit.h"
#include "Engrave.h"
#include "Generators.h"
#include "LuaEngine.h"
#include "Score.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace nt
{

struct InputState
{
    Tick base = PPQ;          // the note value chosen: a quarter
    bool dotted = false;
    bool triplet = false;
    int voice = 0;
    bool noteInput = false;   // clicks and letters write notes

    Tick length() const
    {
        Tick t = base;
        if (dotted) t = t * 3 / 2;
        if (triplet) t = t * 2 / 3;
        return std::max<Tick> (1, t);
    }
};

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
    uint32_t caretPart = 0;
    Tick caret = 0;
    InputState input;
    bool transposedScore = false;
    bool lightPage = true;        // black on white unless the user asks for dark (decision 0020)
    float zoom = 9.0f;            // pixels per staff space

    engrave::Layout layout;
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
    void viewChanged();           // zoom, page colour, transposition: a relayout, no undo
    void setStatus (const juce::String& s);

    //==========================================================================
    // Writing
    void setCaret (uint32_t partId, Tick t);
    void moveCaret (int direction);
    void caretToPart (int direction);
    void typeLetter (int letter, bool addToChord);   // 0..6 is C..B
    void typeRest();
    void writePitch (int pitch, bool addToChord);    // from a click or a MIDI key
    void writeAt (uint32_t partId, Tick at, int pitch, bool addToChord);
    void setDuration (Tick base);
    void toggleDot();
    void toggleTriplet();
    void toggleNoteInput();
    void setVoice (int voice);

    //==========================================================================
    // The selection
    void select (const Selection& s, bool preview = false);
    void selectAll();
    void selectNext (int direction, bool extend);
    void transposeSelection (int semitones);
    void moveSelection (int direction);
    void lengthenSelection (int direction);
    void deleteSelection();
    void copySelection();
    void cutSelection();
    void paste();
    void toggleVoiceOfSelection();
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
    int lastPitch = 67;
    uint32_t lastPart = 0;
    Tick lastWriteStart = 0;
    Tick lastWriteLength = PPQ;
    double lastMidiTime = 0;
    Tick midiChordAt = -1;

    void refresh();               // relayout, re-detect, re-send to playback
    // Puts a result into `s` where it belongs; returns where it starts.
    Tick place (Score& s, const GeneratedResult& r, bool fromSelection, const std::string& generatorId, InsertReport& report) const;
    void ensureCaretPart();
};

} // namespace nt
