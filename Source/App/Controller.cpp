#include "Controller.h"

#include "MidiFile.h"
#include "MusicXml.h"
#include "ScoreFile.h"
#include "Spelling.h"
#include "Templates.h"

namespace nt
{

Controller::Controller (AudioEngine& audioEngine) : audio (audioEngine)
{
    newScore ("String Quartet");
}

//==============================================================================

void Controller::refresh()
{
    ensureCaretPart();
    warnings = roll::check (score);
    chords = detectChords (score, 0, score.endTick());
    keys = detectKeys (score, true);
    audio.update (score);
    sendChangeMessage();
}

void Controller::ensureCaretPart()
{
    if (score.partById (caretPart) == nullptr) caretPart = score.parts.empty() ? 0 : score.parts.front().id;
    caret = std::clamp<Tick> (caret, 0, score.endTick());
    // Drop selected ids that no longer exist.
    Selection alive;
    for (const auto& p : score.parts)
        for (const auto& n : p.notes)
            if (selection.count (n.id) != 0) alive.insert (n.id);
    selection = alive;
    // A range keeps only the parts still there, inside the score - every block of it.
    if (range.active())
    {
        auto prune = [this] (Bars& b)
        {
            std::vector<uint32_t> parts;
            for (const auto& p : score.parts)
                if (b.has (p.id)) parts.push_back (p.id);
            b.parts = parts;
            b.last = std::min (b.last, score.bars - 1);
        };
        prune (range);
        for (auto& b : range.more) prune (b);
        range.more.erase (std::remove_if (range.more.begin(), range.more.end(), [] (const Bars& b) { return ! b.active(); }),
                          range.more.end());
        if (! range.active())
        {
            auto more = range.more;
            range = {};
            if (! more.empty())
            {
                static_cast<Bars&> (range) = more.back();
                more.pop_back();
                range.more = more;
            }
        }
    }
    // Parts chosen by name keep only those still there (0050).
    chosenParts.erase (std::remove_if (chosenParts.begin(), chosenParts.end(),
                                       [this] (uint32_t id) { return score.partById (id) == nullptr; }),
                       chosenParts.end());
    if (chosenParts.size() == 1) caretPart = chosenParts.front();
    if (chosenParts.size() < 2) chosenParts.clear();
}

void Controller::pushUndo (Step step)
{
    undoStack.push_back (std::move (step));
    if (undoStack.size() > 300) undoStack.erase (undoStack.begin());
    redoStack.clear();
}

void Controller::edit (const juce::String& what, const std::function<void (Score&)>& fn)
{
    pushUndo ({ false, score });
    fn (score);
    dirty = true;
    status = what;
    refresh();
}

void Controller::generated() { pushUndo ({ true, {} }); }

void Controller::undo()
{
    if (undoStack.empty()) return;
    auto step = std::move (undoStack.back());
    undoStack.pop_back();
    if (step.results)
    {
        // A Generate undone: the ideas listed before it come back (0049).
        if (stepResults) stepResults (-1);
        redoStack.push_back (std::move (step));
        setStatus ("Undone: the ideas from before that Generate are back");
        return;
    }
    redoStack.push_back ({ false, score });
    score = std::move (step.score);
    dirty = true;
    status = "Undone";
    refresh();
}

void Controller::redo()
{
    if (redoStack.empty()) return;
    auto step = std::move (redoStack.back());
    redoStack.pop_back();
    if (step.results)
    {
        if (stepResults) stepResults (1);
        undoStack.push_back (std::move (step));
        setStatus ("Redone: the ideas from that Generate are back");
        return;
    }
    undoStack.push_back ({ false, score });
    score = std::move (step.score);
    dirty = true;
    status = "Redone";
    refresh();
}

void Controller::selectionChanged() { sendChangeMessage(); }
void Controller::viewChanged() { sendChangeMessage(); }
void Controller::setStatus (const juce::String& s) { status = s; sendChangeMessage(); }

//==============================================================================

void Controller::setCaret (uint32_t partId, Tick t)
{
    if (score.partById (partId) != nullptr) caretPart = partId;
    caret = std::clamp<Tick> (t, 0, score.endTick());
    if (const auto* p = caretPartPtr()) audio.setLiveInstrument (p->instrument);
    sendChangeMessage();
}

void Controller::choosePart (uint32_t partId)
{
    bool kept = false;
    for (const auto& b : range.blocks()) kept = kept || b.has (partId);
    if (! kept) range = {};
    if (score.partById (partId) != nullptr)
    {
        noPartChosen = false;
        chosenParts.clear();
        partAnchor = partId;
    }
    setCaret (partId, caret);
}

void Controller::clickPart (uint32_t partId, bool add, bool extend)
{
    if (score.partById (partId) == nullptr) return;
    if (! add && ! extend) { choosePart (partId); return; }
    // Choosing parts by name lets go of chosen bars; the parts chosen are
    // where the next idea goes.
    range = {};
    auto chosen = partsChosen();
    if (extend)
    {
        // From the part clicked before - the first chosen, so it takes the tune.
        const uint32_t from = score.partById (partAnchor) != nullptr ? partAnchor : (chosen.empty() ? partId : chosen.front());
        const int a = score.partIndex (from), b = score.partIndex (partId);
        chosen = { from };
        for (int i = a; i != b;)
        {
            i += a < b ? 1 : -1;
            chosen.push_back (score.parts[static_cast<size_t> (i)].id);
        }
    }
    else
    {
        const auto it = std::find (chosen.begin(), chosen.end(), partId);
        if (it != chosen.end()) chosen.erase (it);
        else chosen.push_back (partId);
        partAnchor = partId;
    }
    setChosen (chosen, partId);
    if (chosen.empty()) setStatus ("No part chosen: ideas go to every part");
    else if (chosen.size() == 1) setStatus (juce::String (score.partById (chosen.front())->name) + " chosen");
    else setStatus (partsText() + " chosen - an idea goes to them, a single line to " + juce::String (score.partById (chosen.front())->name));
}

void Controller::setChosen (const std::vector<uint32_t>& parts, uint32_t shown)
{
    chosenParts.clear();
    noPartChosen = parts.empty();
    if (parts.size() == 1) caretPart = parts.front();
    else if (parts.size() > 1)
    {
        chosenParts = parts;
        caretPart = std::find (parts.begin(), parts.end(), shown) != parts.end() ? shown : parts.back();
    }
    if (const auto* p = caretPartPtr()) audio.setLiveInstrument (p->instrument);
    sendChangeMessage();
}

std::vector<uint32_t> Controller::partsChosen() const
{
    if (! chosenParts.empty()) return chosenParts;
    if (noPartChosen || score.partById (caretPart) == nullptr) return {};
    return { caretPart };
}

bool Controller::isPartChosen (uint32_t partId) const
{
    const auto chosen = partsChosen();
    return std::find (chosen.begin(), chosen.end(), partId) != chosen.end();
}

juce::String Controller::partsText() const
{
    if (chosenParts.size() < 2) return "every part";
    if (chosenParts.size() > 4) return juce::String (static_cast<int> (chosenParts.size())) + " parts";
    juce::StringArray names;
    for (auto id : chosenParts)
        if (const auto* p = score.partById (id)) names.add (p->name);
    juce::String t;
    for (int i = 0; i < names.size(); ++i)
        t += (i == 0 ? juce::String() : i == names.size() - 1 ? juce::String (" and ") : juce::String (", ")) + names[i];
    return t;
}

void Controller::workIn (uint32_t partId)
{
    if (score.partById (partId) == nullptr) return;
    caretPart = partId;
    noPartChosen = false;
    if (std::find (chosenParts.begin(), chosenParts.end(), partId) == chosenParts.end()) chosenParts.clear();
}

void Controller::letGoOfEverything()
{
    if (audio.isPlaying() || auditioning) stop();
    stepInput = false;
    selection.clear();
    range = {};
    chosenParts.clear();
    noPartChosen = true;
    setStatus ("Nothing chosen: ideas go to every part, a single line to the top one");
}

void Controller::moveCaret (int direction)
{
    caret = std::clamp<Tick> (roll::snap (score, caret, grid) + direction * grid.step(), 0, score.endTick());
    sendChangeMessage();
}

void Controller::caretToPart (int direction)
{
    const int i = score.partIndex (caretPart);
    if (i < 0) return;
    const int j = std::clamp (i + direction, 0, static_cast<int> (score.parts.size()) - 1);
    caretPart = score.parts[static_cast<size_t> (j)].id;
    noPartChosen = false;
    chosenParts.clear();
    if (const auto* p = caretPartPtr()) audio.setLiveInstrument (p->instrument);
    sendChangeMessage();
}

uint32_t Controller::drawNoteAt (uint32_t partId, Tick at, int pitch, Tick length)
{
    if (score.partById (partId) == nullptr) return 0;
    if (length <= 0) length = grid.step();
    uint32_t id = 0;
    edit ("Drew a note", [&] (Score& s) { id = roll::drawNote (s, partId, at, length, pitch); });
    if (id == 0) return 0;
    range = {};
    selection = { id };
    workIn (partId);
    previewPitches ({ pitch }, partId, 0.5);
    sendChangeMessage();
    return id;
}

void Controller::writePitch (int pitch, bool addToChord)
{
    // Step input: a grid step at the caret, overwriting what was there, and
    // the caret moves on; keys pressed together make a chord.
    const auto partId = caretPart;
    if (score.partById (partId) == nullptr) return;
    const Tick len = grid.step();
    const Tick at = addToChord ? lastWriteStart : roll::snap (score, caret, grid);
    uint32_t id = 0;
    edit ("Wrote a note", [&] (Score& s)
    {
        if (addToChord) id = nt::addToChord (s, partId, at, len, pitch, 0);
        else id = writeNote (s, partId, at, len, pitch, 0);
    });
    if (! addToChord)
    {
        lastWriteStart = at;
        caret = std::min (at + len, score.endTick());
        selection.clear();
    }
    range = {};
    selection.insert (id);
    sendChangeMessage();
}

void Controller::setGrid (Tick base, bool triplet)
{
    grid.base = base;
    grid.triplet = triplet;
    setStatus ("Grid " + juce::String (grid.name()));
}

void Controller::toggleSnap()
{
    grid.snap = ! grid.snap;
    setStatus (grid.snap ? "Notes snap to the grid" : "Snap off: notes go exactly where they are put");
}

void Controller::toggleDrawTool()
{
    drawTool = ! drawTool;
    setStatus (drawTool ? "Draw: click the roll to draw a note, drag to make it longer, click a note to delete it"
                        : "Select: click a note to choose it, drag it to move it, drag its end to stretch it");
}

void Controller::toggleStepInput()
{
    stepInput = ! stepInput;
    if (stepInput) caret = roll::snap (score, caret, grid);
    setStatus (stepInput ? "Step input: play a MIDI keyboard to write notes at the caret, one grid step each"
                         : "Step input off");
}

//==============================================================================

void Controller::select (const Selection& s, bool preview)
{
    range = {};
    selection = s;
    if (preview) previewSelection();
    sendChangeMessage();
}

void Controller::selectAll()
{
    // Cmd+A (decision 0050): with a part chosen, every part is chosen, the
    // top one first. With none chosen (after Escape), or every part chosen
    // already, every note is selected as well.
    const bool everyPart = score.parts.size() > 1 && chosenParts.size() == score.parts.size();
    const bool notesToo = noPartChosen || everyPart || score.parts.size() < 2;
    range = {};
    selection.clear();
    std::vector<uint32_t> all;
    for (const auto& p : score.parts) all.push_back (p.id);
    if (notesToo)
        for (const auto& p : score.parts)
            for (const auto& n : p.notes) selection.insert (n.id);
    noPartChosen = false;
    if (all.size() > 1) chosenParts = all;
    status = notesToo ? juce::String ("Everything chosen: every part and all ") + juce::String (static_cast<int> (selection.size())) + " notes"
                      : juce::String ("Every part chosen - Cmd+A again selects every note too");
    sendChangeMessage();
}

void Controller::selectNext (int direction, bool extend)
{
    const auto* part = caretPartPtr();
    if (part == nullptr || part->notes.empty()) return;
    // From the last selected note in this part, or the caret.
    Tick from = caret;
    for (const auto& n : part->notes)
        if (selection.count (n.id) != 0) from = direction > 0 ? n.start : std::min (from, n.start);
    const Note* best = nullptr;
    for (const auto& n : part->notes)
    {
        if (direction > 0 ? n.start > from : n.start < from)
        {
            if (best == nullptr || (direction > 0 ? n.start < best->start : n.start > best->start)) best = &n;
        }
    }
    if (best == nullptr) return;
    range = {};
    if (! extend) selection.clear();
    // The whole chord at that moment.
    for (const auto& n : part->notes)
        if (n.start == best->start && n.voice == best->voice) selection.insert (n.id);
    caret = best->start;
    previewSelection();
    sendChangeMessage();
}

void Controller::transposeSelection (int semitones)
{
    if (selection.empty()) return;
    const auto ids = selection;
    bool ok = true;
    edit (semitones % 12 == 0 ? "Moved by an octave" : "Transposed", [&] (Score& s) { ok = transposeNotes (s, ids, semitones); });
    if (! ok) { undo(); setStatus ("That would go past the ends of the MIDI range."); return; }
    previewSelection();
}

void Controller::moveSelection (int direction)
{
    dragSelection (direction * grid.step(), 0, false);
}

void Controller::dragSelection (Tick by, int semitones, bool copy)
{
    if (selection.empty() || (by == 0 && semitones == 0 && ! copy)) return;
    const auto ids = selection;
    range = {};
    if (copy)
    {
        std::vector<uint32_t> made;
        edit ("Copied notes", [&] (Score& s) { made = roll::copyNotesBy (s, ids, by, semitones); });
        if (made.empty()) { undo(); setStatus ("That would go past the start, or the ends of the MIDI range."); return; }
        selection = Selection (made.begin(), made.end());
    }
    else
    {
        bool ok = true;
        edit (semitones != 0 && by == 0 ? "Transposed" : "Moved", [&] (Score& s) { ok = roll::moveNotes (s, ids, by, semitones); });
        if (! ok) { undo(); setStatus ("That would go past the start, or the ends of the MIDI range."); return; }
    }
    if (semitones != 0) previewSelection();
    sendChangeMessage();
}

void Controller::stretchSelection (Tick by, bool fromStart)
{
    if (selection.empty() || by == 0) return;
    const auto ids = selection;
    const Tick shortest = std::min<Tick> (grid.step(), PPQ / 8);
    edit (by > 0 ? "Lengthened" : "Shortened", [&] (Score& s)
    {
        if (fromStart) roll::resizeStarts (s, ids, by, shortest);
        else roll::resizeNotes (s, ids, by, shortest);
    });
}

void Controller::setVelocities (const std::map<uint32_t, int>& velocities)
{
    if (velocities.empty()) return;
    edit ("Changed velocity", [&] (Score& s) { roll::setVelocities (s, velocities); });
}

void Controller::quantiseSelection()
{
    // Nothing selected: the whole part in the roll, as a DAW quantises a clip.
    auto ids = selection;
    if (ids.empty())
        if (const auto* p = caretPartPtr())
            for (const auto& n : p->notes) ids.insert (n.id);
    if (ids.empty()) { setStatus ("Nothing to quantise"); return; }
    const Tick step = grid.step();
    edit ("Quantised to " + juce::String (grid.name()), [&] (Score& s) { roll::quantise (s, ids, step); });
}

void Controller::duplicateSelection()
{
    if (selection.empty()) return;
    // Straight after the selection, rounded up to the grid.
    Tick first = -1, last = 0;
    for (const auto& p : score.parts)
        for (const auto& n : p.notes)
            if (selection.count (n.id) != 0) { first = first < 0 ? n.start : std::min (first, n.start); last = std::max (last, n.end()); }
    if (first < 0) return;
    const Tick step = grid.step();
    const Tick span = std::max (step, ((last - first + step - 1) / step) * step);
    dragSelection (span, 0, true);
}

void Controller::deleteSelection()
{
    if (selection.empty()) return;
    const auto ids = selection;
    edit ("Deleted", [&] (Score& s) { deleteNotes (s, ids); });
    selection.clear();
    sendChangeMessage();
}

void Controller::copySelection()
{
    Tick earliest = 0;
    clipboard = copyNotes (score, selection, &earliest);
    clipboardFromDrums = false;
    for (const auto& p : score.parts)
        for (const auto& n : p.notes)
            if (selection.count (n.id) != 0) clipboardFromDrums = instrumentById (p.instrument).drums;
    setStatus (juce::String (static_cast<int> (clipboard.size())) + " notes copied");
}

void Controller::cutSelection()
{
    copySelection();
    deleteSelection();
}

void Controller::paste()
{
    if (clipboard.empty()) return;
    Tick span = 0;
    for (const auto& n : clipboard) span = std::max (span, n.end());
    const auto at = caret;
    const auto partId = caretPart;
    std::vector<uint32_t> ids;
    edit ("Pasted", [&] (Score& s) { ids = pasteNotes (s, partId, at, clipboard, span, true); });
    range = {};
    selection = Selection (ids.begin(), ids.end());
    caret = at + span;
    sendChangeMessage();
}

void Controller::previewSelection()
{
    // The selected notes that start first, together: a chord sounds as one.
    Tick first = -1;
    uint32_t partId = 0;
    for (const auto& p : score.parts)
        for (const auto& n : p.notes)
            if (selection.count (n.id) != 0 && (first < 0 || n.start < first)) { first = n.start; partId = p.id; }
    if (first < 0) return;
    std::vector<int> pitches;
    for (const auto& p : score.parts)
        for (const auto& n : p.notes)
            if (selection.count (n.id) != 0 && n.start == first) pitches.push_back (n.pitch);
    previewPitches (pitches, partId);
}

void Controller::selectRange (int a, int b, int fromPart, int toPart, bool keepOthers)
{
    if (score.parts.empty()) return;
    const int last = static_cast<int> (score.parts.size()) - 1;
    auto more = keepOthers ? range.more : std::vector<Bars> {};
    range = {};
    range.more = std::move (more);
    range.first = std::clamp (std::min (a, b), 0, score.bars - 1);
    range.last = std::clamp (std::max (a, b), 0, score.bars - 1);
    const int p0 = std::clamp (std::min (fromPart, toPart), 0, last);
    const int p1 = std::clamp (std::max (fromPart, toPart), 0, last);
    for (int i = p0; i <= p1; ++i) range.parts.push_back (score.parts[static_cast<size_t> (i)].id);

    selection = notesInBlocks();
    caretPart = range.parts.front();
    noPartChosen = false;
    chosenParts.clear();
    caret = score.barStart (range.first);
    sendChangeMessage();
}

void Controller::addRange (int a, int b, int fromPart, int toPart)
{
    if (! range.active()) { selectRange (a, b, fromPart, toPart); return; }
    // One bar of one part chosen already: Cmd and a click take it away.
    if (a == b && fromPart == toPart && fromPart >= 0 && fromPart < static_cast<int> (score.parts.size()))
    {
        const auto id = score.parts[static_cast<size_t> (fromPart)].id;
        auto blocks = range.blocks();
        const auto it = std::find_if (blocks.begin(), blocks.end(), [&] (const Bars& x)
                                      { return x.first == a && x.last == a && x.parts.size() == 1 && x.parts.front() == id; });
        if (it != blocks.end())
        {
            blocks.erase (it);
            range = {};
            if (! blocks.empty())
            {
                static_cast<Bars&> (range) = blocks.back();
                blocks.pop_back();
                range.more = blocks;
            }
            selection = notesInBlocks();
            sendChangeMessage();
            return;
        }
    }
    const Bars before = range;
    range.more.push_back (before);
    selectRange (a, b, fromPart, toPart, true);
}

Selection Controller::notesInBlocks() const
{
    Selection sel;
    for (const auto& b : range.blocks())
    {
        const Tick from = score.barStart (b.first), to = score.barStart (b.last + 1);
        for (const auto pid : b.parts)
            if (const auto* p = score.partById (pid))
                for (const auto& n : p->notes)
                    if (n.start >= from && n.start < to) sel.insert (n.id);
    }
    return sel;
}

juce::String Controller::rangeText() const
{
    juce::StringArray blocks;
    for (const auto& b : range.blocks())
    {
        juce::String t = b.first == b.last ? "Bar " + juce::String (b.first + 1)
                                           : "Bars " + juce::String (b.first + 1) + "-" + juce::String (b.last + 1);
        const auto* top = score.partById (b.parts.front());
        const auto* bottom = score.partById (b.parts.back());
        if (top != nullptr) t += ", " + juce::String (top->name);
        if (bottom != nullptr && bottom != top) t += " to " + juce::String (bottom->name);
        blocks.add (t);
    }
    return blocks.joinIntoString ("; ");
}

std::pair<int, int> Controller::selectedBars() const
{
    if (range.active())
    {
        int lo = range.first, hi = range.last;
        for (const auto& b : range.more) { lo = std::min (lo, b.first); hi = std::max (hi, b.last); }
        return { lo, hi };
    }
    if (selection.empty()) { const int b = score.barAt (caret); return { b, b }; }
    int lo = score.bars, hi = 0;
    for (const auto& p : score.parts)
        for (const auto& n : p.notes)
            if (selection.count (n.id) != 0)
            {
                lo = std::min (lo, score.barAt (n.start));
                hi = std::max (hi, score.barAt (std::max<Tick> (n.start, n.end() - 1)));
            }
    return { lo, std::min (hi, score.bars - 1) };
}

//==============================================================================

uint32_t Controller::addPart (const std::string& instrumentId)
{
    uint32_t id = 0;
    edit ("Added " + juce::String (instrumentById (instrumentId).name), [&] (Score& s)
    {
        Part p;
        p.id = s.newId();
        p.instrument = instrumentId;
        // Two of the same get numbered: Horn 1, Horn 2.
        int same = 0;
        for (const auto& o : s.parts) if (o.instrument == instrumentId) ++same;
        p.name = instrumentById (instrumentId).name + (same > 0 ? " " + std::to_string (same + 1) : std::string());
        id = p.id;
        s.parts.push_back (p);
    });
    caretPart = id;
    noPartChosen = false;
    chosenParts.clear();
    sendChangeMessage();
    return id;
}

void Controller::removePart (uint32_t partId)
{
    edit ("Removed a part", [&] (Score& s)
    {
        s.parts.erase (std::remove_if (s.parts.begin(), s.parts.end(), [partId] (const Part& p) { return p.id == partId; }), s.parts.end());
    });
}

void Controller::movePart (uint32_t partId, int direction)
{
    const int i = score.partIndex (partId);
    const int j = i + direction;
    if (i < 0 || j < 0 || j >= static_cast<int> (score.parts.size())) return;
    edit ("Reordered parts", [&] (Score& s) { std::swap (s.parts[static_cast<size_t> (i)], s.parts[static_cast<size_t> (j)]); });
}

void Controller::setPartInstrument (uint32_t partId, const std::string& instrumentId)
{
    edit ("Changed instrument", [&] (Score& s)
    {
        if (auto* p = s.partById (partId))
        {
            const bool namedAfterOld = p->name == instrumentById (p->instrument).name;
            p->instrument = instrumentId;
            if (namedAfterOld || p->name.empty()) p->name = instrumentById (instrumentId).name;
        }
    });
}

bool Controller::useHeardKey()
{
    for (const auto& k : keys)
        if (caret >= k.start && caret < k.end)
        {
            setKeyAt (score.barAt (k.start), k.root, k.scale);
            return true;
        }
    setStatus ("Nothing to hear yet: write or generate some music first.");
    return false;
}

void Controller::setKeyAt (int bar, int root, int scale)
{
    edit ("Changed the key", [&] (Score& s)
    {
        bool found = false;
        for (auto& k : s.keys) if (k.bar == bar) { k.root = root; k.scale = scale; found = true; }
        if (! found) s.keys.push_back ({ bar, root, scale });
        s.normalise();
    });
}

void Controller::setMeterAt (int bar, int num, int den)
{
    edit ("Changed the time signature", [&] (Score& s)
    {
        // Notes stay where they are in time: the bars are drawn again around them.
        bool found = false;
        for (auto& m : s.meters) if (m.bar == bar) { m.num = num; m.den = den; found = true; }
        if (! found) s.meters.push_back ({ bar, num, den });
        s.normalise();
    });
}

void Controller::setTempo (double bpm)
{
    edit ("Changed the tempo", [&] (Score& s) { s.tempos = { { 0, bpm } }; });
}

void Controller::setBars (int bars)
{
    edit ("Changed the length", [&] (Score& s)
    {
        s.bars = std::max (1, bars);
        s.fitBars();
    });
}

void Controller::insertBarsAtCaret (int count)
{
    const int bar = score.barAt (caret);
    edit ("Inserted bars", [&] (Score& s) { insertBars (s, bar, count); });
}

void Controller::deleteSelectedBars()
{
    const auto [a, b] = selectedBars();
    edit ("Deleted bars", [&, a = a, b = b] (Score& s) { deleteBars (s, a, b - a + 1); });
}

//==============================================================================

void Controller::togglePlay (bool fromStart)
{
    if (audio.isPlaying()) { stop(); return; }
    playFrom (fromStart ? 0 : caret);
}

void Controller::returnToStart()
{
    caret = 0;
    setStatus ("Back to bar 1");
    if (audio.isPlaying() && ! auditioning) playFrom (0);
}

Tick Controller::musicEnd() const
{
    const Tick last = score.lastNoteEnd();
    if (last <= 0) return score.endTick();
    return score.barStart (score.barAt (last - 1) + 1);
}

void Controller::skipToEnd()
{
    if (audio.isPlaying()) stop();
    caret = musicEnd();
    setStatus ("To the end of the music: bar " + juce::String (score.barAt (caret) + 1));
}

void Controller::toggleFollow()
{
    followPlayback = ! followPlayback;
    setStatus (! followPlayback ? "Follow off: the view stays where you put it while it plays"
               : followStyle == FollowStyle::smooth ? "Follow: the view scrolls along with the music as it plays"
                                                    : "Follow: the view turns a page with the music as it plays");
}

void Controller::setFollowStyle (FollowStyle style)
{
    followStyle = style;
    followPlayback = true;
    setStatus (style == FollowStyle::smooth ? "Follow: the view scrolls along with the music, the playhead a third of the way across"
                                            : "Follow: the view turns a page just before the music goes out of view");
}

void Controller::playFrom (Tick t)
{
    playheadClock.reset();
    auditioning = false;
    audio.play (score, t);
    sendChangeMessage();
}

void Controller::stop()
{
    audio.stop();
    auditioning = false;
    sendChangeMessage();
}

Tick Controller::playheadTick() const
{
    const Tick from = audio.playheadTick();
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    return score.tickAtSeconds (score.secondsAt (from) + playheadClock.update (audio.playheadSeconds(), now));
}

void Controller::previewPitches (const std::vector<int>& pitches, uint32_t partId, double seconds)
{
    const auto* p = score.partById (partId);
    audio.preview (pitches, p != nullptr ? p->instrument : std::string ("pno"), seconds);
}

//==============================================================================

juce::StringArray Controller::templates()
{
    juce::StringArray names;
    for (const auto& t : scoreTemplates()) names.add (t.name);
    return names;
}

void Controller::newScore (const juce::String& name)
{
    const auto* t = templateByName (name.toStdString());
    if (t == nullptr) t = templateByName ("Empty");
    audio.stop();
    score = scoreFromTemplate (*t);
    undoStack.clear();
    redoStack.clear();
    selection.clear();
    range = {};
    caret = 0;
    caretPart = score.parts.empty() ? 0 : score.parts.front().id;
    noPartChosen = false;
    chosenParts.clear();
    file = juce::File();
    dirty = false;
    status = "New score: " + name;
    refresh();
}

namespace
{
// The text of a compressed MusicXML (.mxl): a zip whose container.xml names
// the score inside it.
bool readMxl (const juce::File& f, std::string& text, juce::String& error)
{
    juce::ZipFile zip (f);
    juce::String path;
    if (const auto* container = zip.getEntry ("META-INF/container.xml"))
    {
        std::unique_ptr<juce::InputStream> in (zip.createStreamForEntry (*container));
        if (in != nullptr)
            if (auto xml = juce::parseXML (in->readEntireStreamAsString()))
                if (auto* rootfiles = xml->getChildByName ("rootfiles"))
                    if (auto* rf = rootfiles->getChildByName ("rootfile")) path = rf->getStringAttribute ("full-path");
    }
    for (int i = 0; path.isEmpty() && i < zip.getNumEntries(); ++i)
    {
        const auto name = zip.getEntry (i)->filename;
        if (! name.startsWith ("META-INF") && (name.endsWithIgnoreCase (".xml") || name.endsWithIgnoreCase (".musicxml"))) path = name;
    }
    const auto* entry = path.isEmpty() ? nullptr : zip.getEntry (path);
    if (entry == nullptr) { error = f.getFileName() + " has no score inside it."; return false; }
    std::unique_ptr<juce::InputStream> in (zip.createStreamForEntry (*entry));
    if (in == nullptr) { error = "Could not unpack " + f.getFileName(); return false; }
    juce::MemoryBlock mb;
    in->readIntoMemoryBlock (mb);
    text.assign (static_cast<const char*> (mb.getData()), mb.getSize());
    return true;
}
} // namespace

bool Controller::readScoreFile (const juce::File& f, Score& out, juce::String& error)
{
    if (f.hasFileExtension ("mxl;musicxml;xml"))
    {
        std::string text;
        if (f.hasFileExtension ("mxl")) { if (! readMxl (f, text, error)) return false; }
        else
        {
            juce::MemoryBlock mb;
            if (! f.loadFileAsData (mb)) { error = "Could not read " + f.getFileName(); return false; }
            text.assign (static_cast<const char*> (mb.getData()), mb.getSize());
        }
        auto r = readMusicXml (text);
        if (! r.ok) { error = r.error; return false; }
        out = std::move (r.score);
        if (out.title.empty() || out.title == "Untitled") out.title = f.getFileNameWithoutExtension().toStdString();
        return true;
    }
    juce::MemoryBlock mb;
    if (! f.loadFileAsData (mb)) { error = "Could not read " + f.getFileName(); return false; }
    if (f.hasFileExtension ("miderator;noterator"))
    {
        auto r = loadScore (mb.toString().toStdString());
        if (! r.ok) { error = r.error; return false; }
        out = std::move (r.score);
        return true;
    }
    std::vector<uint8_t> bytes (static_cast<const uint8_t*> (mb.getData()), static_cast<const uint8_t*> (mb.getData()) + mb.getSize());
    auto r = readMidiFile (bytes);
    if (! r.ok) { error = r.error; return false; }
    out = std::move (r.score);
    if (out.title == "Imported") out.title = f.getFileNameWithoutExtension().toStdString();
    return true;
}

bool Controller::load (const juce::File& f, juce::String& error)
{
    Score loaded;
    if (! readScoreFile (f, loaded, error)) return false;
    // Only a project saves back to where it came from; a MIDI or MusicXML
    // file opened here becomes a new project when it is saved.
    file = f.hasFileExtension ("miderator;noterator") ? f : juce::File();
    audio.stop();
    score = std::move (loaded);
    undoStack.clear();
    redoStack.clear();
    selection.clear();
    range = {};
    caret = 0;
    caretPart = score.parts.empty() ? 0 : score.parts.front().id;
    noPartChosen = false;
    chosenParts.clear();
    dirty = false;
    status = "Opened " + f.getFileName();
    refresh();
    return true;
}

bool Controller::importFile (const juce::File& f, juce::String& error)
{
    Score incoming;
    if (! readScoreFile (f, incoming, error)) return false;
    const Tick at = score.barStart (score.barAt (caret));
    Selection added;
    edit ("Imported " + f.getFileName(), [&] (Score& s)
    {
        for (auto& p : incoming.parts)
        {
            Part np;
            np.id = s.newId();
            np.instrument = p.instrument;
            np.name = p.name;
            s.parts.push_back (np);
            const auto ids = pasteNotes (s, np.id, at, p.notes, 0, false);
            added.insert (ids.begin(), ids.end());
        }
    });
    range = {};
    selection = added;
    sendChangeMessage();
    return true;
}

bool Controller::save (const juce::File& f, juce::String& error)
{
    const auto text = saveScore (score);
    if (! f.replaceWithText (text)) { error = "Could not write " + f.getFullPathName(); return false; }
    file = f;
    dirty = false;
    setStatus ("Saved " + f.getFileName());
    return true;
}

//==============================================================================

GeneratorContext Controller::generatorContext (bool withSelection) const
{
    auto ctx = contextFor (score, caretPart, caret, withSelection ? selection : Selection {});
    if (range.active())
        for (const auto& b : range.blocks()) ctx.rangeBars = std::max (ctx.rangeBars, b.bars());
    return ctx;
}

uint32_t Controller::lineTarget() const
{
    if (! range.active())
    {
        // The part chosen first, of several (decision 0050).
        if (! chosenParts.empty()) return chosenParts.front();
        return noPartChosen && ! score.parts.empty() ? score.parts.front().id : caretPart;
    }
    return lineTargetIn (range);
}

uint32_t Controller::lineTargetIn (const Bars& block) const
{
    if (block.has (caretPart)) return caretPart;
    return block.parts.front();
}

Tick Controller::place (Score& s, const GeneratedResult& r, bool fromSelection, const std::string& generatorId,
                        InsertReport& report) const
{
    // Every instrument gets no more notes at once than it plays (decision
    // 0036) - except a block from the toolbox, which goes in as it is.
    InsertOptions base;
    base.fitPolyphony = generatorId != "starting-blocks";
    if (range.active())
    {
        // Selected bars: the music fills them (decision 0019).
        const Tick from = s.barStart (range.first), to = s.barStart (range.last + 1);
        if (! fromSelection)
        {
            // Shared across the chosen parts - or, a single line, into one
            // (0042) - each block of bars on its own (0050).
            Tick first = from;
            for (const auto& b : range.blocks())
            {
                const Tick bFrom = s.barStart (b.first), bTo = s.barStart (b.last + 1);
                const auto parts = isSingleLine (r) ? std::vector<uint32_t> { lineTargetIn (b) } : b.parts;
                const auto one = insertIntoRange (s, r, parts, bFrom, bTo, base);
                report.newNotes.insert (one.newNotes.begin(), one.newNotes.end());
                report.newParts.insert (report.newParts.end(), one.newParts.begin(), one.newParts.end());
                report.thinnedParts.insert (report.thinnedParts.end(), one.thinnedParts.begin(), one.thinnedParts.end());
                report.unplaced.insert (report.unplaced.end(), one.unplaced.begin(), one.unplaced.end());
                first = std::min (first, bFrom);
            }
            return first;
        }
        if (generatorId == "midi-variator" && ! selection.empty())
        {
            // A variation of the bars takes their place, in the part it came from.
            uint32_t source = range.parts.front();
            for (const auto& p : s.parts)
                for (const auto& n : p.notes)
                    if (selection.count (n.id) != 0) { source = p.id; break; }
            const auto* sp = s.partById (source);
            InsertOptions o = base;
            o.contextInstrument = sp != nullptr ? sp->instrument : std::string ("pno");
            report = insertResult (s, fitToSpan (r, to - from), source, from, o);
            return from;
        }
    }
    if (! fromSelection || selection.empty())
    {
        // A block from the toolbox is small and goes where the caret is, so
        // blocks can be laid one after another (decision 0018).
        if (generatorId == "starting-blocks")
        {
            report = insertResult (s, r, caretPart, caret, base);
            return caret;
        }
        // Nothing chosen: shared across every part (decision 0041), from the
        // caret's bar so an idea always starts on a downbeat, all of it.
        // A single line goes into the caret's part alone (decision 0042).
        const Tick at = s.barStart (s.barAt (caret));
        // Several parts chosen by name: those alone (decision 0050).
        std::vector<uint32_t> all;
        if (isSingleLine (r)) all.push_back (lineTarget());
        else
            for (const auto& p : s.parts)
                if (chosenParts.empty() || isPartChosen (p.id)) all.push_back (p.id);
        report = insertIntoRange (s, r, all, at, at + std::max<Tick> (r.length, PPQ), base);
        return at;
    }
    const auto [first, last] = selectedBars();
    uint32_t source = caretPart;
    for (const auto& p : s.parts)
        for (const auto& n : p.notes)
            if (selection.count (n.id) != 0) source = p.id;
    const auto* sp = s.partById (source);
    InsertOptions o = base;
    o.contextInstrument = sp != nullptr ? sp->instrument : std::string ("pno");

    if (generatorId == "midi-variator")
    {
        // A variation follows its original, in the same part.
        const Tick at = s.barStart (last + 1);
        report = insertResult (s, r, source, at, o);
        return at;
    }
    // Anything else made from the selection goes with it, never over it:
    // chords under a tune, a tune over chords.
    const Tick at = s.barStart (first);
    report = insertResult (s, r, 0, at, o);
    return at;
}

void Controller::insertGenerated (const GeneratedResult& r, bool fromSelection, const std::string& generatorId)
{
    InsertReport report;
    Tick at = 0;
    edit ("Inserted " + juce::String (r.title), [&] (Score& s)
    {
        at = place (s, r, fromSelection, generatorId, report);
        // A Blocks chord keeps the root it was made on, for the Chords lane
        // (decision 0046) - as far as the chosen bars, if bars are chosen.
        if (range.active() && ! fromSelection)
            for (const auto& b : range.blocks())
            {
                const Tick bFrom = s.barStart (b.first);
                markChordRoot (s, r, bFrom, std::min (r.length, s.barStart (b.last + 1) - bFrom));
            }
        else markChordRoot (s, r, at, r.length);
    });
    // Say so when an instrument was given fewer notes than the idea had.
    juce::StringArray thinned;
    for (const auto id : report.thinnedParts)
        if (const auto* p = score.partById (id))
            thinned.addIfNotAlreadyThere (juce::String (p->name) + (instrumentById (p->instrument).role == 'B' ? ": bottom notes only" : ": top notes only"));
    if (! thinned.isEmpty())
        status += " - " + thinned.joinIntoString (", ");
    // And when a line had nowhere to go: no part is added for it (0041).
    juce::StringArray left;
    for (const auto& name : report.unplaced) left.addIfNotAlreadyThere (juce::String (name));
    if (! left.isEmpty())
        status += " - " + left.joinIntoString (", ") + " left out: no part for " + (left.size() == 1 ? "it" : "them");
    if (range.active() && (! fromSelection || generatorId == "midi-variator"))
    {
        // The bars stay selected, so another idea can go straight into them.
        selection = notesInBlocks();
        caret = score.barStart (range.first);
        sendChangeMessage();
        return;
    }
    range = {};
    selection = report.newNotes;
    caret = std::min (at + r.length, score.endTick());
    if (! report.newNotes.empty())
        for (const auto& p : score.parts)
            for (const auto& n : p.notes)
                if (report.newNotes.count (n.id) != 0) { caretPart = p.id; break; }
    sendChangeMessage();
}

Score Controller::auditionScore (const GeneratedResult& r, bool fromSelection, const std::string& generatorId,
                                Tick& from, Tick& to) const
{
    // Heard in place, where it would go (decision 0041), with the music
    // around it - except a block, heard on its own. Either way on a piano,
    // every note of it, whatever the parts it goes to can play (0043).
    if (range.active() || (fromSelection && ! selection.empty()) || generatorId != "starting-blocks")
    {
        Score temp = score;
        InsertReport report;
        const Tick at = place (temp, r, fromSelection, generatorId, report);
        // The parts it would go to are left silent there; the piano plays it.
        deleteNotes (temp, report.newNotes);
        for (auto id : report.newParts)
            temp.parts.erase (std::remove_if (temp.parts.begin(), temp.parts.end(),
                                              [id] (const Part& p) { return p.id == id; }), temp.parts.end());
        Tick length = std::max<Tick> (r.length, PPQ);
        if (range.active() && ! fromSelection && ! range.more.empty())
        {
            // Bars in several blocks (0050): heard in each, from the first.
            Tick end = at;
            for (const auto& b : range.blocks())
            {
                const Tick bFrom = score.barStart (b.first), bTo = score.barStart (b.last + 1);
                addAudition (temp, r, bFrom, bTo - bFrom);
                end = std::max (end, bTo);
            }
            temp.normalise();
            from = at;
            to = end;
            return temp;
        }
        if (range.active() && (! fromSelection || generatorId == "midi-variator"))
            length = score.barStart (range.last + 1) - at;
        addAudition (temp, r, at, length);
        temp.normalise();
        from = at;
        to = at + length;
        return temp;
    }
    from = 0;
    to = -1;   // to its end
    return auditionAlone (r);
}

Score Controller::auditionAlone (const GeneratedResult& r) const
{
    Score temp;
    temp.tempos = score.tempos;
    const int bar = score.barAt (caret);
    temp.meters = { score.meterAtBar (bar) };
    temp.meters.front().bar = 0;
    temp.keys = { score.keyAtBar (bar) };
    temp.keys.front().bar = 0;
    addAudition (temp, r, 0, 0);
    temp.normalise();
    return temp;
}

void Controller::auditionGenerated (const GeneratedResult& r, bool fromSelection, const std::string& generatorId)
{
    Tick from = 0, to = 0;
    const Score temp = auditionScore (r, fromSelection, generatorId, from, to);
    audio.play (temp, from, to);
    auditioning = true;
    sendChangeMessage();
}

} // namespace nt
