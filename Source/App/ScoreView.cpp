#include "ScoreView.h"

#include "Spelling.h"

namespace nt
{

using namespace engrave;

ScoreView::ScoreView (Controller& c) : controller (c)
{
    setWantsKeyboardFocus (false);
    addAndMakeVisible (hbar);
    addAndMakeVisible (vbar);
    hbar.addListener (this);
    vbar.addListener (this);
    hbar.setAutoHide (false);
    vbar.setAutoHide (false);
    controller.addChangeListener (this);
    startTimerHz (30);
}

ScoreView::~ScoreView()
{
    controller.removeChangeListener (this);
}

//==============================================================================
// Geometry

juce::Point<float> ScoreView::origin() const
{
    return { static_cast<float> (gutter - scrollX), static_cast<float> (lanesHeight + 4.0f * space() - scrollY) };
}

float ScoreView::headerWidthPx() const
{
    if (controller.layout.measures.empty()) return 0;
    const auto& m = controller.layout.measures.front();
    return (4.4f + static_cast<float> (m.key.count) * 1.0f + 0.8f) * space();
}

juce::Rectangle<int> ScoreView::musicArea() const
{
    return getLocalBounds().withTrimmedLeft (gutter).withTrimmedTop (lanesHeight).withTrimmedRight (12).withTrimmedBottom (12);
}

juce::Point<double> ScoreView::toLayout (juce::Point<float> p) const
{
    const auto o = origin();
    return { (p.x - o.x) / space(), (p.y - o.y) / space() };
}

int ScoreView::positionAt (int staff, double layoutY) const
{
    const auto& st = controller.layout.staves[static_cast<size_t> (staff)];
    return static_cast<int> (std::lround ((st.top + 4.0 - layoutY) * 2.0));
}

Tick ScoreView::snapTick (Tick t) const
{
    const auto& s = controller.score;
    const int bar = s.barAt (t);
    const Tick start = s.barStart (bar);
    const Tick grid = std::max<Tick> (1, controller.input.length());
    // A little bias forward: a click just short of a beat means that beat.
    Tick snapped = start + ((t - start + grid * 35 / 100) / grid) * grid;
    if (snapped >= start + s.barLength (bar)) snapped = start + ((s.barLength (bar) - 1) / grid) * grid;
    return std::max<Tick> (0, snapped);
}

int ScoreView::staffPart (int staffIndex) const
{
    return controller.layout.staves[static_cast<size_t> (staffIndex)].part;
}

int ScoreView::barAtX (float x) const
{
    const auto& lay = controller.layout;
    if (lay.measures.empty()) return 0;
    const int m = lay.measureAtX (toLayout ({ std::max (x, static_cast<float> (gutter)), 0.0f }).x);
    return lay.measures[static_cast<size_t> (std::clamp (m, 0, static_cast<int> (lay.measures.size()) - 1))].bar;
}

int ScoreView::partAtY (float y) const
{
    const auto& lay = controller.layout;
    if (lay.staves.empty()) return 0;
    return staffPart (lay.staffAtY (toLayout ({ 0.0f, y }).y));
}

void ScoreView::resized()
{
    hbar.setBounds (gutter, getHeight() - 12, getWidth() - gutter - 12, 12);
    vbar.setBounds (getWidth() - 12, lanesHeight, 12, getHeight() - lanesHeight - 12);
    updateScrollbars();
}

void ScoreView::updateScrollbars()
{
    const auto& lay = controller.layout;
    const double contentW = (lay.width + 8.0) * space();
    const double contentH = (lay.height + 8.0) * space();
    const double viewW = musicArea().getWidth(), viewH = musicArea().getHeight();
    scrollX = juce::jlimit (0.0, std::max (0.0, contentW - viewW), scrollX);
    scrollY = juce::jlimit (0.0, std::max (0.0, contentH - viewH), scrollY);
    hbar.setRangeLimits (0, std::max (contentW, viewW), juce::dontSendNotification);
    hbar.setCurrentRange (scrollX, viewW, juce::dontSendNotification);
    vbar.setRangeLimits (0, std::max (contentH, viewH), juce::dontSendNotification);
    vbar.setCurrentRange (scrollY, viewH, juce::dontSendNotification);
}

void ScoreView::scrollBarMoved (juce::ScrollBar* bar, double start)
{
    if (bar == &hbar) scrollX = start;
    else scrollY = start;
    repaint();
}

void ScoreView::scrollToTick (Tick t)
{
    const double x = controller.layout.xForTick (t) * space();
    const double w = musicArea().getWidth();
    if (x < scrollX + 20 || x > scrollX + w - 60) scrollX = std::max (0.0, x - w * 0.15);
    updateScrollbars();
    repaint();
}

void ScoreView::zoomBy (float factor)
{
    controller.zoom = juce::jlimit (5.0f, 24.0f, controller.zoom * factor);
    updateScrollbars();
    repaint();
}

void ScoreView::changeListenerCallback (juce::ChangeBroadcaster*)
{
    updateScrollbars();
    repaint();
}

void ScoreView::timerCallback()
{
    if (controller.audio.isPlaying() && ! controller.auditioning)
    {
        const Tick t = controller.playheadTick();
        if (t != lastPlayhead)
        {
            lastPlayhead = t;
            // Keep the playhead on the page, a page at a time.
            const double x = controller.layout.xForTick (t) * space();
            const double w = musicArea().getWidth();
            if (x > scrollX + w - 40 || x < scrollX) { scrollX = std::max (0.0, x - 40); updateScrollbars(); }
            lastSounding = controller.audio.soundingNotes();
            repaint();
        }
    }
    else if (lastPlayhead >= 0)
    {
        lastPlayhead = -1;
        lastSounding.clear();
        repaint();
    }
}

//==============================================================================
// Painting

void ScoreView::paint (juce::Graphics& g)
{
    const auto page = controller.lightPage ? theme::lightPage() : theme::darkPage();
    g.fillAll (page.paper);

    RenderStyle style;
    style.space = space();
    style.page = page;
    style.selected = std::set<uint32_t> (controller.selection.begin(), controller.selection.end());
    style.sounding = std::set<uint32_t> (lastSounding.begin(), lastSounding.end());

    const auto area = musicArea();
    {
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (area.withTop (lanesHeight).withRight (getWidth() - 12));
        ScoreRenderer::draw (g, controller.layout, controller.score, style, origin(), area.toFloat());
        paintRange (g);
        paintCaret (g);
        paintGhost (g);

        if (controller.audio.isPlaying() && ! controller.auditioning && lastPlayhead >= 0)
        {
            const float x = origin().x + static_cast<float> (controller.layout.xForTick (lastPlayhead)) * space();
            g.setColour (page.accent);
            g.fillRect (x - 1.0f, static_cast<float> (lanesHeight), 2.0f, static_cast<float> (getHeight() - lanesHeight - 12));
        }
        if (! rubberBand.isEmpty())
        {
            g.setColour (page.accent.withAlpha (0.12f));
            g.fillRect (rubberBand);
            g.setColour (page.accent.withAlpha (0.7f));
            g.drawRect (rubberBand, 1);
        }
    }
    paintGutter (g);
    paintLanes (g);
}

void ScoreView::paintCaret (juce::Graphics& g)
{
    const auto& lay = controller.layout;
    const auto page = controller.lightPage ? theme::lightPage() : theme::darkPage();
    const int pi = controller.score.partIndex (controller.caretPart);
    if (pi < 0) return;
    double top = 1e9, bottom = -1e9;
    for (const auto& st : lay.staves)
        if (st.part == pi) { top = std::min (top, st.top); bottom = std::max (bottom, st.top + 4.0); }
    if (top > bottom) return;
    const auto o = origin();
    const float x = o.x + static_cast<float> (lay.xForTick (controller.caret)) * space() - 0.6f * space();
    const float y1 = o.y + static_cast<float> (top - 1.0) * space(), y2 = o.y + static_cast<float> (bottom + 1.0) * space();
    const bool input = controller.input.noteInput;
    g.setColour (input ? page.accent : page.dim.withAlpha (0.8f));
    g.fillRect (x, y1, input ? 2.5f : 1.5f, y2 - y1);
    // The part the caret is in, marked down the side.
    g.fillRect (static_cast<float> (gutter) + 1.0f, y1, 3.0f, y2 - y1);
}

void ScoreView::paintRange (juce::Graphics& g)
{
    const auto& range = controller.range;
    const auto& lay = controller.layout;
    if (! range.active() || lay.measures.empty()) return;
    const auto page = controller.lightPage ? theme::lightPage() : theme::darkPage();
    double top = 1e9, bottom = -1e9;
    for (const auto& st : lay.staves)
    {
        const auto& p = controller.score.parts[static_cast<size_t> (st.part)];
        if (std::find (range.parts.begin(), range.parts.end(), p.id) == range.parts.end()) continue;
        top = std::min (top, st.top);
        bottom = std::max (bottom, st.top + 4.0);
    }
    if (top > bottom) return;
    const auto& m0 = lay.measures[static_cast<size_t> (std::clamp (range.first, 0, static_cast<int> (lay.measures.size()) - 1))];
    const auto& m1 = lay.measures[static_cast<size_t> (std::clamp (range.last, 0, static_cast<int> (lay.measures.size()) - 1))];
    const auto o = origin();
    const juce::Rectangle<float> r (o.x + static_cast<float> (m0.x) * space(), o.y + static_cast<float> (top - 1.5) * space(),
                                    static_cast<float> (m1.x + m1.width - m0.x) * space(), static_cast<float> (bottom - top + 3.0) * space());
    g.setColour (page.accent.withAlpha (controller.lightPage ? 0.16f : 0.10f));
    g.fillRect (r);
    g.setColour (page.accent.withAlpha (0.9f));
    g.drawRect (r, 1.5f);
}

void ScoreView::paintGhost (juce::Graphics& g)
{
    if (! controller.input.noteInput || hover.x < gutter || hover.y < lanesHeight) return;
    const auto& lay = controller.layout;
    if (lay.staves.empty()) return;
    const auto lp = toLayout (hover);
    const int staff = lay.staffAtY (lp.y);
    const auto& st = lay.staves[static_cast<size_t> (staff)];
    const int pos = positionAt (staff, lp.y);
    if (pos < -10 || pos > 18) return;
    const Tick t = snapTick (lay.tickForX (lp.x));
    const auto o = origin();
    const float x = o.x + static_cast<float> (lay.xForTick (t)) * space();
    const float y = o.y + static_cast<float> (st.top + 4.0 - pos * 0.5) * space();
    const auto page = controller.lightPage ? theme::lightPage() : theme::darkPage();
    g.setColour (page.accent.withAlpha (0.55f));
    // Ledger lines for the ghost as well, so a note above the staff can be aimed.
    for (int p = -2; p >= pos; p -= 2)
        g.fillRect (x - 0.4f * space(), o.y + static_cast<float> (st.top + 4.0 - p * 0.5) * space() - 1.0f, 2.0f * space(), 1.5f);
    for (int p = 10; p <= pos; p += 2)
        g.fillRect (x - 0.4f * space(), o.y + static_cast<float> (st.top + 4.0 - p * 0.5) * space() - 1.0f, 2.0f * space(), 1.5f);
    Glyphs::get().draw (g, controller.input.base >= 4 * PPQ ? smufl::noteheadWhole
                           : controller.input.base >= 2 * PPQ ? smufl::noteheadHalf : smufl::noteheadBlack, x, y, space());
}

void ScoreView::paintGutter (juce::Graphics& g)
{
    const auto page = controller.lightPage ? theme::lightPage() : theme::darkPage();
    const auto& lay = controller.layout;
    const auto o = origin();
    juce::Graphics::ScopedSaveState s (g);
    g.reduceClipRegion (0, lanesHeight, getWidth(), getHeight() - lanesHeight - 12);

    RenderStyle style;
    style.space = space();
    style.page = page;

    // Once the start has scrolled away, the clef and key stay put beside the
    // names, the way a galley view keeps them.
    if (scrollX > 1.0 && ! lay.measures.empty())
    {
        const float w = headerWidthPx();
        g.setColour (page.paper);
        g.fillRect (static_cast<float> (gutter), static_cast<float> (lanesHeight), w, static_cast<float> (getHeight()));
        const int mi = lay.measureAtX ((scrollX) / space());
        const auto& m = lay.measures[static_cast<size_t> (std::max (0, mi))];
        for (const auto& st : lay.staves)
        {
            const float top = o.y + static_cast<float> (st.top) * space();
            g.setColour (page.ink.withAlpha (0.85f));
            for (int l = 0; l < 5; ++l) g.fillRect (static_cast<float> (gutter), top + static_cast<float> (l) * space() - 0.5f, w, 1.0f);
            ScoreRenderer::drawStaffStart (g, st, m, style, static_cast<float> (gutter), top, false);
        }
        g.setColour (page.dim.withAlpha (0.5f));
        g.fillRect (static_cast<float> (gutter) + w - 1.0f, static_cast<float> (lanesHeight), 1.0f, static_cast<float> (getHeight()));
    }

    g.setColour (page.paper);
    g.fillRect (0, lanesHeight, gutter, getHeight());
    ScoreRenderer::drawNames (g, lay, controller.score, style, static_cast<float> (gutter) - 10.0f, o.y, false);
    // The caret's part, marked.
    const int pi = controller.score.partIndex (controller.caretPart);
    for (const auto& st : lay.staves)
        if (st.part == pi && st.staffInPart == 0)
        {
            g.setColour (controller.input.noteInput ? page.accent : page.dim);
            g.fillRect (static_cast<float> (gutter) - 5.0f, o.y + static_cast<float> (st.top) * space(), 3.0f,
                        static_cast<float> (st.staffCount * 4 + (st.staffCount - 1) * 7) * space());
        }
}

void ScoreView::paintLanes (juce::Graphics& g)
{
    const auto& lay = controller.layout;
    g.setColour (theme::sunken);
    g.fillRect (0, 0, getWidth(), lanesHeight);
    g.setColour (theme::rule);
    g.fillRect (0, 22, getWidth(), 1);
    g.fillRect (0, lanesHeight - 1, getWidth(), 1);

    g.setColour (theme::textDim);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("Scale", 8, 0, gutter - 16, 22, juce::Justification::centredLeft);
    g.drawText ("Chords", 8, 22, gutter - 16, lanesHeight - 22, juce::Justification::centredLeft);

    juce::Graphics::ScopedSaveState s (g);
    g.reduceClipRegion (gutter, 0, getWidth() - gutter, lanesHeight);
    const float ox = origin().x;
    auto xOf = [&] (Tick t) { return ox + static_cast<float> (lay.xForTick (t)) * space(); };

    g.setFont (juce::FontOptions (12.5f));
    for (const auto& k : controller.keys)
    {
        const float x1 = xOf (k.start), x2 = xOf (k.end);
        g.setColour (theme::frameActive);
        g.fillRoundedRectangle (x1 + 2.0f, 3.0f, std::max (4.0f, x2 - x1 - 4.0f), 16.0f, 3.0f);
        // The label stays in view while any of its span is.
        const float lx = std::max (x1 + 8.0f, static_cast<float> (gutter) + 6.0f);
        g.setColour (theme::text);
        g.drawText (k.label, juce::Rectangle<float> (lx, 3.0f, std::max (20.0f, x2 - lx - 4.0f), 16.0f), juce::Justification::centredLeft, true);
    }

    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    for (const auto& c : controller.chords)
    {
        if (c.name.empty()) continue;
        const float x1 = xOf (c.start), x2 = xOf (c.end);
        bool lit = false;
        if (controller.audio.isPlaying() && lastPlayhead >= c.start && lastPlayhead < c.end) lit = true;
        g.setColour (lit ? theme::accent : theme::control);
        g.fillRect (x1 + 1.0f, static_cast<float> (lanesHeight - 6), std::max (2.0f, x2 - x1 - 2.0f), 2.0f);
        const float lx = std::max (x1 + 3.0f, static_cast<float> (gutter) + 4.0f);
        if (lx > x2 - 8.0f) continue;
        g.setColour (lit ? theme::accent : theme::text);
        g.drawText (c.name, juce::Rectangle<float> (lx, 23.0f, std::max (30.0f, x2 - lx - 1.0f), static_cast<float> (lanesHeight - 30)),
                    juce::Justification::centredLeft, true);
    }
}

//==============================================================================
// Mouse

void ScoreView::mouseDown (const juce::MouseEvent& e)
{
    // The window takes the keys, so letters and arrows work after a click here.
    if (auto* p = getParentComponent()) p->grabKeyboardFocus();
    auto& lay = controller.layout;
    const auto p = e.position;
    dragStart = p;
    dragging = draggingNotes = false;
    selectingBars = allParts = false;
    rubberBand = {};

    // The lanes: a chord plays the harmony there, a scale plays itself.
    if (p.y < lanesHeight)
    {
        if (p.x < gutter) return;
        const Tick t = lay.tickForX (toLayout (p).x);
        // A drag along the lanes chooses bars in every part.
        anchorBar = barAtX (p.x);
        anchorPart = 0;
        allParts = true;
        if (p.y < 22)
        {
            for (const auto& k : controller.keys)
                if (t >= k.start && t < k.end)
                {
                    const auto& sc = scaleview::scales[static_cast<size_t> (k.scale)];
                    const int rootPc = scaleview::roots[static_cast<size_t> (k.root)].pitchClass();
                    std::vector<int> notes;
                    for (int iv : sc.intervals) notes.push_back (60 + rootPc + iv);
                    notes.push_back (72 + rootPc);
                    controller.audio.previewSequence (notes, "pno", 0.16);
                    controller.setStatus (juce::String (k.label) + ": " + juce::String (static_cast<int> (sc.intervals.size())) + " notes");
                }
            return;
        }
        for (const auto& c : controller.chords)
            if (t >= c.start && t < c.end && ! c.pitches.empty())
            {
                controller.audio.preview (c.pitches, "pno", 1.2, 90);
                controller.select (notesInRange (controller.score, c.start, c.end));
                controller.setStatus (juce::String (c.name) + " - bar " + juce::String (controller.score.barAt (c.start) + 1));
                return;
            }
        return;
    }

    // The names: a click puts the caret in that part.
    if (p.x < gutter)
    {
        if (lay.staves.empty()) return;
        const int staff = lay.staffAtY (toLayout (p).y);
        controller.setCaret (controller.score.parts[static_cast<size_t> (staffPart (staff))].id, controller.caret);
        return;
    }

    if (lay.staves.empty()) return;
    const auto lp = toLayout (p);
    int staffIndex = -1, elementIndex = -1;
    const Head* head = lay.headAt (lp.x, lp.y, &staffIndex, &elementIndex);

    if (controller.input.noteInput && ! (head != nullptr && e.mods.isCommandDown()))
    {
        const int staff = lay.staffAtY (lp.y);
        const auto& st = lay.staves[static_cast<size_t> (staff)];
        const auto& part = controller.score.parts[static_cast<size_t> (st.part)];
        const auto& inst = instrumentById (part.instrument);
        const int pos = positionAt (staff, lp.y);
        const Tick t = snapTick (lay.tickForX (lp.x));
        int pitch;
        if (inst.drums)
        {
            // The kit piece written nearest that line or space.
            static const std::pair<int, int> kit[] = { { -1, 44 }, { 1, 36 }, { 2, 41 }, { 5, 38 }, { 6, 45 }, { 7, 48 }, { 8, 51 }, { 9, 42 }, { 10, 49 } };
            pitch = 38;
            int best = 100;
            for (const auto& [kp, kpitch] : kit) if (std::abs (kp - pos) < best) { best = std::abs (kp - pos); pitch = kpitch; }
        }
        else
        {
            const auto& k = controller.score.keyAtBar (controller.score.barAt (t));
            pitch = pitchAtPosition (pos, st.clef, keyContext (k.root, k.scale), inst, controller.transposedScore);
        }
        controller.writeAt (part.id, t, pitch, e.mods.isShiftDown());
        return;
    }

    if (head != nullptr)
    {
        auto sel = controller.selection;
        const auto& el = lay.staves[static_cast<size_t> (staffIndex)].elements[static_cast<size_t> (elementIndex)];
        if (e.mods.isCommandDown()) { if (sel.count (head->noteId) != 0) sel.erase (head->noteId); else sel.insert (head->noteId); }
        else if (e.mods.isShiftDown()) sel.insert (head->noteId);
        else if (sel.count (head->noteId) == 0) sel = { head->noteId };
        const auto& part = controller.score.parts[static_cast<size_t> (lay.staves[static_cast<size_t> (staffIndex)].part)];
        controller.caretPart = part.id;
        controller.caret = el.at;
        controller.select (sel);
        controller.previewPitches ({ head->pitch }, part.id, 0.6);
        draggingNotes = true;
        dragPitchFrom = head->pitch;
        dragSemitones = 0;
        return;
    }

    // Empty paper: that bar is chosen and the caret goes where the click
    // was; a drag chooses more bars, and more parts (decision 0019). Shift
    // and a click stretches the bars chosen; Shift and a drag picks notes.
    const int bar = barAtX (p.x), partIndex = partAtY (p.y);
    const Tick at = snapTick (lay.tickForX (lp.x));
    if (e.mods.isShiftDown())
    {
        if (controller.range.active())
        {
            controller.selectRange (anchorBar, bar, anchorPart, partIndex);
            controller.setStatus (controller.rangeText() + " chosen");
        }
        return;
    }
    anchorBar = bar;
    anchorPart = partIndex;
    allParts = false;
    selectingBars = true;
    controller.selectRange (bar, bar, partIndex, partIndex);
    controller.caret = at;
    controller.setStatus (controller.rangeText() + " chosen - drag to choose more, Generate fills them");
}

void ScoreView::mouseDrag (const juce::MouseEvent& e)
{
    if (e.mouseWasClicked()) return;
    if (draggingNotes)
    {
        // Up and down by staff steps, measured from where the drag began.
        const int steps = static_cast<int> (std::lround ((dragStart.y - e.position.y) / (space() * 0.5f)));
        static constexpr int diatonic[7] = { 2, 2, 1, 2, 2, 2, 1 };
        int semis = 0;
        for (int i = 0; i < std::abs (steps); ++i) semis += diatonic[i % 7];
        semis = steps < 0 ? -semis : semis;
        if (semis != dragSemitones)
        {
            dragSemitones = semis;
            controller.setStatus ("Moving by " + juce::String (semis) + " semitones - let go to place");
        }
        return;
    }
    if (dragStart.x < gutter || controller.input.noteInput) return;
    if (selectingBars || (allParts && dragStart.y < lanesHeight))
    {
        const int bar = barAtX (e.position.x);
        const int last = static_cast<int> (controller.score.parts.size()) - 1;
        const int part = allParts ? last : partAtY (e.position.y);
        const auto& r = controller.range;
        const int first = std::min (anchorBar, bar), lastBar = std::max (anchorBar, bar);
        const auto topId = controller.score.parts.empty() ? 0u : controller.score.parts[static_cast<size_t> (std::min (anchorPart, part))].id;
        const auto bottomId = controller.score.parts.empty() ? 0u : controller.score.parts[static_cast<size_t> (std::max (anchorPart, part))].id;
        if (! r.active() || r.first != first || r.last != lastBar || r.parts.front() != topId || r.parts.back() != bottomId)
        {
            controller.selectRange (anchorBar, bar, anchorPart, part);
            controller.setStatus (controller.rangeText() + " chosen");
        }
        selectingBars = true;
        return;
    }
    if (dragStart.y < lanesHeight) return;
    dragging = true;
    rubberBand = juce::Rectangle<float> (dragStart, e.position).toNearestInt();
    repaint();
}

void ScoreView::mouseUp (const juce::MouseEvent&)
{
    if (draggingNotes && dragSemitones != 0) controller.transposeSelection (dragSemitones);
    draggingNotes = false;
    dragSemitones = 0;
    if (selectingBars && controller.range.active())
        controller.setStatus (controller.rangeText() + " chosen - Generate fills them; Esc lets go");
    selectingBars = allParts = false;
    if (dragging && ! rubberBand.isEmpty())
    {
        const auto& lay = controller.layout;
        const auto a = toLayout (rubberBand.getTopLeft().toFloat()), b = toLayout (rubberBand.getBottomRight().toFloat());
        Selection sel = controller.selection;
        uint32_t firstPart = 0;
        for (const auto& st : lay.staves)
            for (const auto& el : st.elements)
                for (const auto& h : el.heads)
                {
                    const double hx = h.x + 0.6, hy = st.top + 4.0 - h.pos * 0.5;
                    if (hx >= a.x && hx <= b.x && hy >= a.y && hy <= b.y && ! el.rest)
                    {
                        sel.insert (h.noteId);
                        if (firstPart == 0) firstPart = controller.score.parts[static_cast<size_t> (st.part)].id;
                    }
                }
        if (firstPart != 0) controller.caretPart = firstPart;
        controller.select (sel);
        controller.setStatus (juce::String (static_cast<int> (sel.size())) + " notes selected");
    }
    dragging = false;
    rubberBand = {};
    repaint();
}

void ScoreView::mouseDoubleClick (const juce::MouseEvent& e)
{
    const auto& lay = controller.layout;
    if (e.position.x < gutter && e.position.y > lanesHeight && ! lay.staves.empty())
    {
        // A part's name: everything it plays.
        const int staff = lay.staffAtY (toLayout (e.position).y);
        const auto& part = controller.score.parts[static_cast<size_t> (staffPart (staff))];
        Selection sel;
        for (const auto& n : part.notes) sel.insert (n.id);
        controller.select (sel);
        return;
    }
    const auto lp = toLayout (e.position);
    int staffIndex = -1, elementIndex = -1;
    if (lay.headAt (lp.x, lp.y, &staffIndex, &elementIndex) == nullptr) return;
    // A chord: all of it, heard together.
    const auto& el = lay.staves[static_cast<size_t> (staffIndex)].elements[static_cast<size_t> (elementIndex)];
    Selection sel;
    std::vector<int> pitches;
    for (const auto& h : el.heads) { sel.insert (h.noteId); pitches.push_back (h.pitch); }
    controller.select (sel);
    controller.previewPitches (pitches, controller.score.parts[static_cast<size_t> (lay.staves[static_cast<size_t> (staffIndex)].part)].id, 1.0);
}

void ScoreView::mouseMove (const juce::MouseEvent& e)
{
    hover = e.position;
    setTooltip (tooltipAt (e.position));
    if (controller.input.noteInput) repaint();
}

void ScoreView::mouseExit (const juce::MouseEvent&)
{
    hover = { -1, -1 };
    repaint();
}

void ScoreView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (e.mods.isCommandDown())
    {
        zoomBy (w.deltaY > 0 ? 1.1f : 1.0f / 1.1f);
        return;
    }
    const double amount = 220.0;
    if (e.mods.isShiftDown()) scrollX -= w.deltaY * amount;
    else
    {
        scrollX -= w.deltaX * amount;
        scrollY -= w.deltaY * amount;
    }
    updateScrollbars();
    repaint();
}

juce::String ScoreView::tooltipAt (juce::Point<float> p) const
{
    const auto& lay = controller.layout;
    if (p.y < lanesHeight)
    {
        if (p.y < 22) return "The scale the music is in, read from the notes. Click to hear it.";
        return "The chords, read from every part together. Click one to hear it.";
    }
    if (p.x < gutter || lay.staves.empty()) return {};
    const auto lp = toLayout (p);
    int staffIndex = -1, elementIndex = -1;
    const Head* h = lay.headAt (lp.x, lp.y, &staffIndex, &elementIndex);
    if (h == nullptr) return {};
    const auto& st = lay.staves[static_cast<size_t> (staffIndex)];
    const auto& el = st.elements[static_cast<size_t> (elementIndex)];
    const auto& part = controller.score.parts[static_cast<size_t> (st.part)];
    const auto& inst = instrumentById (part.instrument);
    const auto& k = controller.score.keyAtBar (controller.score.barAt (el.at));
    const auto ctx = keyContext (k.root, k.scale);
    juce::StringArray lines;
    lines.add (juce::String (pitchName (h->pitch, ctx)) + (h->written != h->pitch ? " (written " + juce::String (pitchName (h->written, ctx)) + ")" : juce::String())
               + "  -  " + juce::String (part.name) + ", bar " + juce::String (el.measure + 1));
    if (h->outOfRange)
        lines.add ("Out of the " + juce::String (inst.name) + "'s range (" + juce::String (pitchName (inst.low, ctx)) + " to "
                   + juce::String (pitchName (inst.high, ctx)) + ").");
    else if (h->outsideSweet)
        lines.add ("Playable, but outside where the " + juce::String (inst.name) + " sounds most like itself ("
                   + juce::String (pitchName (inst.sweetLow, ctx)) + " to " + juce::String (pitchName (inst.sweetHigh, ctx)) + ").");
    if (el.tooManyNotes)
        lines.add (inst.monophonic() ? "A chord: the " + juce::String (inst.name) + " plays one note at a time."
                                     : "More notes at once than the " + juce::String (inst.name) + " plays.");
    if (el.tooFast) lines.add ("Faster than the " + juce::String (inst.name) + " plays cleanly at this tempo.");
    if (el.tooWide) lines.add ("A wider leap than the " + juce::String (inst.name) + " takes comfortably in passing.");
    return lines.joinIntoString ("\n");
}

} // namespace nt
