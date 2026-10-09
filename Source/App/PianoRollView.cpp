#include "PianoRollView.h"

#include "Spelling.h"

namespace nt
{

PianoRollView::PianoRollView (Controller& c, Timeline& t) : controller (c), timeline (t)
{
    addAndMakeVisible (vbar);
    vbar.addListener (this);
    vbar.setAutoHide (false);
    controller.addChangeListener (this);
    timeline.addChangeListener (this);
    startTimerHz (30);
}

PianoRollView::~PianoRollView()
{
    controller.removeChangeListener (this);
    timeline.removeChangeListener (this);
}

//==============================================================================
// Geometry

bool PianoRollView::drums() const
{
    const auto* p = part();
    return p != nullptr && instrumentById (p->instrument).drums;
}

juce::Rectangle<int> PianoRollView::gridArea() const
{
    return { Timeline::left, rulerHeight, std::max (0, getWidth() - Timeline::left - Timeline::right),
             std::max (0, getHeight() - rulerHeight - velocityHeight - velocityGap) };
}

juce::Rectangle<int> PianoRollView::velocityArea() const
{
    return { Timeline::left, getHeight() - velocityHeight, std::max (0, getWidth() - Timeline::left - Timeline::right), velocityHeight };
}

float PianoRollView::yOf (int pitch) const
{
    return static_cast<float> (gridArea().getY() + (127 - pitch) * static_cast<double> (rowHeight()) - scrollY);
}

int PianoRollView::pitchAt (float y) const
{
    const int row = static_cast<int> (std::floor ((static_cast<double> (y) - gridArea().getY() + scrollY) / rowHeight()));
    return std::clamp (127 - row, 0, 127);
}

juce::Rectangle<float> PianoRollView::noteRect (Tick start, Tick end, int pitch) const
{
    const float x1 = timeline.xOf (start), x2 = timeline.xOf (end);
    return { x1, yOf (pitch) + 1.0f, std::max (3.0f, x2 - x1 - 1.0f), std::max (2.0f, rowHeight() - 2.0f) };
}

const Note* PianoRollView::noteAt (juce::Point<float> p, int* zone) const
{
    const auto* pt = part();
    if (pt == nullptr) return nullptr;
    const Note* found = nullptr;
    // Short notes are hard to hit: every note can be caught a few pixels
    // either side, and at least eight pixels tall. The last drawn is on top.
    auto grab = [this] (const Note& n)
    {
        auto r = noteRect (n.start, n.end(), n.pitch).expanded (3.0f, 0.0f);
        return r.getHeight() < 8.0f ? r.withSizeKeepingCentre (r.getWidth(), 8.0f) : r;
    };
    for (const auto& n : pt->notes)
        if (grab (n).contains (p)) found = &n;
    if (found != nullptr && zone != nullptr)
    {
        // The last quarter of a note (two to seven pixels) and just past it
        // stretch its end; the first four pixels of a long one, its start.
        const auto r = noteRect (found->start, found->end(), found->pitch);
        const float edge = std::clamp (r.getWidth() * 0.25f, 2.0f, 7.0f);
        *zone = p.x >= r.getRight() - edge ? 1 : (r.getWidth() >= 18.0f && p.x < r.getX() + 4.0f) ? -1 : 0;
    }
    return found;
}

const Note* PianoRollView::stemAt (float x) const
{
    const auto* pt = part();
    if (pt == nullptr) return nullptr;
    const Note* best = nullptr;
    float bestDistance = 5.0f;
    for (const auto& n : pt->notes)
    {
        const float d = std::abs (timeline.xOf (n.start) - x);
        // Among stems in the same place, a selected one first.
        if (d < bestDistance || (best != nullptr && std::abs (d - bestDistance) < 0.01f && controller.selection.count (n.id) != 0))
        {
            best = &n;
            bestDistance = d;
        }
    }
    return best;
}

Tick PianoRollView::minLength() const
{
    return std::min<Tick> (controller.grid.step(), PPQ / 8);
}

void PianoRollView::resized()
{
    const auto grid = gridArea();
    vbar.setBounds (getWidth() - Timeline::right, grid.getY(), Timeline::right, grid.getHeight());
    updateScrollbar();
}

void PianoRollView::updateScrollbar()
{
    const double content = 128.0 * rowHeight();
    const double view = gridArea().getHeight();
    scrollY = juce::jlimit (0.0, std::max (0.0, content - view), scrollY);
    vbar.setRangeLimits (0, std::max (content, view), juce::dontSendNotification);
    vbar.setCurrentRange (scrollY, view, juce::dontSendNotification);
}

void PianoRollView::scrollBarMoved (juce::ScrollBar*, double start)
{
    scrollY = start;
    repaint();
}

void PianoRollView::centreOnPart()
{
    const auto* p = part();
    if (p == nullptr) return;
    const auto& inst = instrumentById (p->instrument);
    int lo = 127, hi = 0;
    for (const auto& n : p->notes) { lo = std::min (lo, n.pitch); hi = std::max (hi, n.pitch); }
    if (p->notes.empty()) { lo = inst.drums ? 35 : inst.sweetLow; hi = inst.drums ? 59 : inst.sweetHigh; }
    const double mid = 127.0 - (lo + hi) * 0.5 + 0.5;
    scrollY = mid * rowHeight() - gridArea().getHeight() * 0.5;
    updateScrollbar();
    repaint();
}

void PianoRollView::changeListenerCallback (juce::ChangeBroadcaster*)
{
    if (controller.caretPart != shownPart)
    {
        shownPart = controller.caretPart;
        centreOnPart();
    }
    updateScrollbar();
    repaint();
}

void PianoRollView::timerCallback()
{
    if (controller.audio.isPlaying() && ! controller.auditioning)
    {
        const Tick t = controller.playheadTick();
        if (t != lastPlayhead)
        {
            lastPlayhead = t;
            // Keep the playhead in view, a page at a time (decision 0033).
            if (controller.followPlayback) timeline.follow (t);
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

void PianoRollView::paint (juce::Graphics& g)
{
    const auto c = theme::rollColours (controller.lightTheme);
    g.fillAll (theme::ground);
    const auto grid = gridArea();
    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (grid);
        paintRows (g, c);
        paint::grid (g, grid, timeline, controller.score, controller.grid, c, true);

        // The chosen bars, where they cover this part.
        const auto& r = controller.range;
        const auto& s = controller.score;
        if (r.active() && std::find (r.parts.begin(), r.parts.end(), controller.caretPart) != r.parts.end())
        {
            const float x1 = timeline.xOf (s.barStart (r.first)), x2 = timeline.xOf (s.barStart (r.last + 1));
            g.setColour (c.selected.withAlpha (controller.lightTheme ? 0.10f : 0.06f));
            g.fillRect (x1, static_cast<float> (grid.getY()), x2 - x1, static_cast<float> (grid.getHeight()));
            g.setColour (c.selected.withAlpha (0.6f));
            g.fillRect (x1, static_cast<float> (grid.getY()), 1.5f, static_cast<float> (grid.getHeight()));
            g.fillRect (x2 - 1.5f, static_cast<float> (grid.getY()), 1.5f, static_cast<float> (grid.getHeight()));
        }

        paintNotes (g, c);

        // The caret and the playhead.
        g.setColour (controller.stepInput ? c.selected : c.page.dim);
        g.fillRect (timeline.xOf (controller.caret), static_cast<float> (grid.getY()), controller.stepInput ? 2.0f : 1.0f, static_cast<float> (grid.getHeight()));
        if (lastPlayhead >= 0)
        {
            g.setColour (c.sounding);
            g.fillRect (timeline.xOf (lastPlayhead) - 1.0f, static_cast<float> (grid.getY()), 2.0f, static_cast<float> (grid.getHeight()));
        }
        if (mode == Mode::lasso && ! lasso.isEmpty())
        {
            g.setColour (c.selected.withAlpha (0.12f));
            g.fillRect (lasso);
            g.setColour (c.selected.withAlpha (0.8f));
            g.drawRect (lasso, 1.0f);
        }
    }
    paintKeys (g, c);
    paintVelocities (g, c);

    // The ruler, with the part's name over the keys.
    const juce::Rectangle<int> ruler (Timeline::left, 0, grid.getWidth(), rulerHeight);
    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (ruler);
        paint::ruler (g, ruler, timeline, controller.score, controller.caret, controller.stepInput);
    }
    g.setColour (theme::sunken);
    g.fillRect (0, 0, Timeline::left, rulerHeight);
    g.setColour (theme::rule);
    g.fillRect (0, rulerHeight - 1, Timeline::left, 1);
    if (const auto* p = part())
    {
        g.setColour (theme::accent);
        g.fillRect (0, 3, 3, rulerHeight - 6);
        g.setColour (theme::text);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText (p->name, juce::Rectangle<int> (10, 0, Timeline::left - 14, rulerHeight - 1), juce::Justification::centredLeft, true);
    }
    else
    {
        g.setColour (theme::textDim);
        g.setFont (juce::FontOptions (12.5f));
        g.drawText ("No part", juce::Rectangle<int> (10, 0, Timeline::left - 14, rulerHeight - 1), juce::Justification::centredLeft, true);
    }
}

void PianoRollView::paintRows (juce::Graphics& g, const theme::RollColours& c)
{
    const auto grid = gridArea();
    const int top = pitchAt (static_cast<float> (grid.getY())), bottom = pitchAt (static_cast<float> (grid.getBottom()));
    const bool kit = drums();
    for (int p = bottom; p <= top; ++p)
    {
        const bool dark = kit ? roll::drumName (p).empty() : roll::isBlackKey (p);
        g.setColour (dark ? c.blackRow : c.whiteRow);
        g.fillRect (static_cast<float> (grid.getX()), yOf (p), static_cast<float> (grid.getWidth()), rowHeight());
        // Octaves marked between B and C.
        if (! kit && p % 12 == 0)
        {
            g.setColour (c.beatLine);
            g.fillRect (static_cast<float> (grid.getX()), yOf (p) + rowHeight() - 1.0f, static_cast<float> (grid.getWidth()), 1.0f);
        }
    }
}

void PianoRollView::paintNotes (juce::Graphics& g, const theme::RollColours& c)
{
    const auto& s = controller.score;
    const auto* pt = part();
    const auto grid = gridArea().toFloat();
    const bool kit = drums();

    // The other parts, faintly, so a line can be written against them.
    if (! kit)
    {
        g.setColour (c.ghost);
        for (const auto& other : s.parts)
        {
            if (pt != nullptr && other.id == pt->id) continue;
            if (instrumentById (other.instrument).drums) continue;
            for (const auto& n : other.notes)
            {
                const auto r = noteRect (n.start, n.end(), n.pitch);
                if (r.getRight() < grid.getX() || r.getX() > grid.getRight() || r.getBottom() < grid.getY() || r.getY() > grid.getBottom()) continue;
                g.fillRect (r);
            }
        }
    }
    if (pt == nullptr) return;

    const std::set<uint32_t> sounding (lastSounding.begin(), lastSounding.end());
    const bool labels = rowHeight() >= 10.0f && ! kit;
    g.setFont (juce::FontOptions (std::min (11.0f, rowHeight() - 1.0f)));
    const auto ctx = keyContext (0, 0);
    auto drawOne = [&] (const Note& n, Tick start, Tick end, int pitch, bool selected, float alpha)
    {
        const auto r = noteRect (start, end, pitch);
        if (r.getRight() < grid.getX() || r.getX() > grid.getRight() || r.getBottom() < grid.getY() || r.getY() > grid.getBottom()) return;
        const auto w = controller.warnings.find (n.id);
        const roll::Warning warn = w != controller.warnings.end() ? w->second : roll::Warning {};
        juce::Colour fill = warn.outOfRange ? c.page.warn : warn.outsideSweet ? c.faint : c.note;
        // Louder notes are drawn stronger.
        fill = fill.withMultipliedAlpha (0.55f + 0.45f * static_cast<float> (n.velocity) / 127.0f);
        if (selected) fill = c.selected;
        if (sounding.count (n.id) != 0) fill = c.sounding;
        g.setColour (fill.withMultipliedAlpha (alpha));
        g.fillRoundedRectangle (r, 2.0f);
        g.setColour (c.noteEdge.withMultipliedAlpha (alpha * 0.8f));
        g.drawRoundedRectangle (r, 2.0f, 1.0f);
        if (warn.flagged())
        {
            // A red mark on what the instrument would struggle with.
            juce::Path mark;
            const float m = std::min (7.0f, r.getHeight());
            mark.addTriangle (r.getX(), r.getY(), r.getX() + m, r.getY(), r.getX(), r.getY() + m);
            g.setColour (c.page.warn);
            g.fillPath (mark);
        }
        if (labels && r.getWidth() > 30.0f)
        {
            g.setColour ((selected || sounding.count (n.id) != 0 || ! controller.lightTheme) ? theme::ink : juce::Colours::white);
            g.drawText (pitchName (pitch, ctx), r.reduced (4.0f, 0.0f), juce::Justification::centredLeft, false);
        }
    };

    for (const auto& n : pt->notes)
    {
        const bool selected = controller.selection.count (n.id) != 0;
        Tick start = n.start, end = n.end();
        int pitch = n.pitch;
        if (selected && dragged)
        {
            if (mode == Mode::move)
            {
                if (dragCopy) drawOne (n, start, end, pitch, false, 0.5f);
                start += dragBy;
                end += dragBy;
                pitch = std::clamp (pitch + dragSemitones, 0, 127);
            }
            else if (mode == Mode::stretchEnd) end = std::max (start + minLength(), end + dragBy);
            else if (mode == Mode::stretchStart) start = std::clamp<Tick> (start + dragBy, 0, end - minLength());
        }
        drawOne (n, start, end, pitch, selected, 1.0f);
    }

    // The note being drawn.
    if (mode == Mode::draw)
    {
        const auto r = noteRect (drawStart, drawStart + drawLength, drawPitch);
        g.setColour (c.selected.withAlpha (0.8f));
        g.fillRoundedRectangle (r, 2.0f);
    }
}

void PianoRollView::paintKeys (juce::Graphics& g, const theme::RollColours& c)
{
    const auto grid = gridArea();
    juce::Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (0, grid.getY(), Timeline::left, grid.getHeight());
    g.setColour (theme::ground);
    g.fillRect (0, grid.getY(), Timeline::left, grid.getHeight());

    const float keysX = static_cast<float> (Timeline::left - keysWidth);
    const float rh = rowHeight();
    const int top = pitchAt (static_cast<float> (grid.getY())), bottom = pitchAt (static_cast<float> (grid.getBottom()));
    const bool kit = drums();
    const auto* pt = part();

    // What is sounding in this part, lit on the keys.
    std::set<int> lit;
    if (pt != nullptr && ! lastSounding.empty())
    {
        const std::set<uint32_t> sounding (lastSounding.begin(), lastSounding.end());
        for (const auto& n : pt->notes)
            if (sounding.count (n.id) != 0) lit.insert (n.pitch);
    }
    if (mode == Mode::keys && keyPitch >= 0) lit.insert (keyPitch);
    const int hoverPitch = hover.x >= 0 && hover.x < Timeline::left && hover.y >= grid.getY() && hover.y < grid.getBottom() ? pitchAt (hover.y) : -1;

    for (int p = bottom; p <= top; ++p)
    {
        const float y = yOf (p);
        if (kit)
        {
            const auto name = roll::drumName (p);
            g.setColour (lit.count (p) != 0 ? theme::accent : name.empty() ? c.blackKey : c.whiteKey);
            g.fillRect (keysX, y, static_cast<float> (keysWidth), rh - 1.0f);
            if (! name.empty() && rh >= 9.0f)
            {
                g.setColour (p == hoverPitch ? theme::accent : theme::text);
                g.setFont (juce::FontOptions (std::min (12.0f, rh)));
                g.drawText (name, juce::Rectangle<float> (8.0f, y, keysX - 12.0f, rh), juce::Justification::centredRight, true);
            }
            continue;
        }
        const bool black = roll::isBlackKey (p);
        // A black key's row is white beyond its end, as on a keyboard.
        if (black)
        {
            g.setColour (c.whiteKey);
            g.fillRect (keysX, y, static_cast<float> (keysWidth), rh);
        }
        g.setColour (lit.count (p) != 0 ? theme::accent : black ? c.blackKey : c.whiteKey);
        g.fillRect (keysX, y, black ? keysWidth * 0.62f : static_cast<float> (keysWidth), rh);
        if (! black)
        {
            g.setColour (c.keyText.withAlpha (0.25f));
            g.fillRect (keysX, y + rh - 0.5f, static_cast<float> (keysWidth), 0.5f);
        }
        if (p % 12 == 0 || p == hoverPitch)
        {
            g.setColour (p == hoverPitch ? theme::accent : theme::textDim);
            g.setFont (juce::FontOptions (std::min (11.5f, std::max (9.0f, rh))));
            g.drawText (roll::keyName (p), juce::Rectangle<float> (keysX - 40.0f, y + rh * 0.5f - 7.0f, 36.0f, 14.0f), juce::Justification::centredRight, false);
        }
    }

    // Where the instrument plays, and where it sounds best (decision 0006):
    // a bracket from its lowest to its highest note, thick where it sounds best.
    if (pt != nullptr && ! kit)
    {
        const auto& inst = instrumentById (pt->instrument);
        const float x = 16.0f;
        const float yHigh = yOf (inst.high), yLow = yOf (inst.low) + rh;
        const float sHigh = yOf (inst.sweetHigh), sLow = yOf (inst.sweetLow) + rh;
        g.setColour (theme::textDim);
        g.fillRect (x + 2.0f, yHigh, 2.0f, yLow - yHigh);
        g.fillRect (x, yHigh, 8.0f, 2.0f);
        g.fillRect (x, yLow - 2.0f, 8.0f, 2.0f);
        g.setColour (theme::control);
        g.fillRoundedRectangle (x, sHigh, 6.0f, sLow - sHigh, 2.0f);
        g.setColour (theme::textDim);
        g.setFont (juce::FontOptions (11.0f));
        auto label = [&] (const juce::String& text, float y)
        {
            if (y < static_cast<float> (grid.getY()) || y > static_cast<float> (grid.getBottom()) - 12.0f) return;
            g.drawText (text, juce::Rectangle<float> (x + 12.0f, y, keysX - x - 54.0f, 13.0f), juce::Justification::centredLeft, true);
        };
        label ("highest", yHigh);
        label ("lowest", yLow - 13.0f);
        g.setColour (theme::text);
        label ("at its best", juce::jlimit (static_cast<float> (grid.getY()) + 16.0f, static_cast<float> (grid.getBottom()) - 30.0f, (sHigh + sLow) * 0.5f - 6.0f));
    }
}

void PianoRollView::paintVelocities (juce::Graphics& g, const theme::RollColours& c)
{
    const auto area = velocityArea();
    g.setColour (theme::sunken);
    g.fillRect (0, area.getY() - velocityGap, getWidth(), velocityGap);
    g.setColour (theme::rule);
    g.fillRect (0, area.getY() - velocityGap + 2, getWidth(), 1);
    g.setColour (theme::ground);
    g.fillRect (0, area.getY(), Timeline::left, area.getHeight());
    g.setColour (theme::textDim);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("Velocity", juce::Rectangle<int> (10, area.getY() + 4, Timeline::left - 20, 16), juce::Justification::centredLeft);
    g.setFont (juce::FontOptions (10.5f));
    g.drawText ("127", juce::Rectangle<int> (Timeline::left - 40, area.getY() + 1, 34, 12), juce::Justification::centredRight);
    g.drawText ("1", juce::Rectangle<int> (Timeline::left - 40, area.getBottom() - 13, 34, 12), juce::Justification::centredRight);

    juce::Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (area);
    g.setColour (c.whiteRow);
    g.fillRect (area);
    paint::grid (g, area, timeline, controller.score, controller.grid, c, false);
    g.setColour (c.beatLine);
    g.fillRect (static_cast<float> (area.getX()), static_cast<float> (area.getY()) + area.getHeight() * 0.5f, static_cast<float> (area.getWidth()), 1.0f);

    const auto* pt = part();
    if (pt == nullptr) return;
    const float h = static_cast<float> (area.getHeight()) - 6.0f;
    const float bottom = static_cast<float> (area.getBottom()) - 1.0f;
    for (const auto& n : pt->notes)
    {
        const float x = timeline.xOf (n.start);
        if (x < static_cast<float> (area.getX()) - 8.0f || x > static_cast<float> (area.getRight()) + 8.0f) continue;
        const auto it = velocityEdits.find (n.id);
        const int v = it != velocityEdits.end() ? it->second : n.velocity;
        const float y = bottom - h * static_cast<float> (v) / 127.0f;
        const bool selected = controller.selection.count (n.id) != 0;
        g.setColour (selected ? c.selected : c.note);
        g.fillRect (x, y, 2.0f, bottom - y);
        g.fillRoundedRectangle (x - 1.5f, y - 1.5f, 7.0f, 4.0f, 1.5f);
    }
}

//==============================================================================
// Mouse

void PianoRollView::mouseDown (const juce::MouseEvent& e)
{
    focusWindow (*this);
    const auto p = e.position;
    downAt = p;
    mode = Mode::none;
    dragged = false;
    dragBy = 0;
    dragSemitones = 0;
    const auto* pt = part();
    const auto& s = controller.score;
    const auto grid = gridArea();

    if (p.y < rulerHeight)
    {
        if (p.x < Timeline::left) return;
        mode = Mode::caret;
        controller.setCaret (controller.caretPart, roll::snap (s, timeline.tickAt (p.x), controller.grid));
        return;
    }
    if (pt == nullptr) return;

    if (velocityArea().contains (p.toInt()))
    {
        mode = Mode::velocity;
        velocityEdits.clear();
        velocityFrom.clear();
        velocityRelative = false;
        lastVelocityX = p.x;
        // A selected note's stem moves every selected note's velocity by as
        // much; anywhere else, the drag paints velocities across the notes.
        if (const auto* n = stemAt (p.x))
        {
            if (controller.selection.count (n->id) != 0 && controller.selection.size() > 1)
            {
                velocityRelative = true;
                for (const auto& m : pt->notes)
                    if (controller.selection.count (m.id) != 0) velocityFrom[m.id] = m.velocity;
            }
        }
        velocityDrag (p);
        return;
    }

    if (p.x < Timeline::left)
    {
        if (p.y >= grid.getY() && p.y < grid.getBottom())
        {
            mode = Mode::keys;
            keyPitch = pitchAt (p.y);
            controller.previewPitches ({ keyPitch }, pt->id, 0.6);
            repaint();
        }
        return;
    }
    if (! grid.contains (p.toInt())) return;

    int zone = 0;
    const Note* hit = noteAt (p, &zone);
    if (e.mods.isPopupMenu())
    {
        if (hit != nullptr && controller.selection.count (hit->id) == 0) controller.select ({ hit->id });
        showMenu (hit);
        return;
    }

    if (controller.drawTool)
    {
        if (hit != nullptr)
        {
            // Draw mode: a click on a note deletes it.
            const auto id = hit->id;
            controller.select ({ id });
            controller.deleteSelection();
            return;
        }
        mode = Mode::draw;
        drawPitch = pitchAt (p.y);
        drawStart = roll::snapDown (s, timeline.tickAt (p.x), controller.grid);
        drawLength = controller.grid.snap ? controller.grid.step() : std::max<Tick> (minLength(), controller.grid.step());
        controller.previewPitches ({ drawPitch }, pt->id, 0.4);
        repaint();
        return;
    }

    if (hit != nullptr)
    {
        auto sel = controller.selection;
        if (e.mods.isShiftDown() || e.mods.isCommandDown())
        {
            if (sel.count (hit->id) != 0) sel.erase (hit->id);
            else sel.insert (hit->id);
        }
        else if (sel.count (hit->id) == 0) sel = { hit->id };
        dragNote = hit->id;
        dragStart = hit->start;
        dragEnd = hit->end();
        dragPitch = hit->pitch;
        controller.caret = hit->start;
        controller.select (sel);
        controller.previewPitches ({ dragPitch }, pt->id, 0.5);
        if (sel.count (dragNote) == 0) return;   // Shift-clicked off
        mode = zone > 0 ? Mode::stretchEnd : zone < 0 ? Mode::stretchStart : Mode::move;
        dragCopy = e.mods.isAltDown() && mode == Mode::move;
        return;
    }

    mode = Mode::lasso;
    lasso = {};
}

void PianoRollView::mouseDrag (const juce::MouseEvent& e)
{
    const auto p = e.position;
    const auto& s = controller.score;
    const auto* pt = part();
    if (mode != Mode::velocity && e.getDistanceFromDragStart() < 3 && ! dragged) return;
    const Tick raw = timeline.tickAt (p.x) - timeline.tickAt (downAt.x);
    switch (mode)
    {
        case Mode::caret:
            controller.setCaret (controller.caretPart, roll::snap (s, timeline.tickAt (p.x), controller.grid));
            return;
        case Mode::keys:
        {
            const int k = pitchAt (p.y);
            if (k != keyPitch && pt != nullptr)
            {
                keyPitch = k;
                controller.previewPitches ({ k }, pt->id, 0.4);
                repaint();
            }
            return;
        }
        case Mode::move:
        {
            dragged = true;
            // The clicked note lands on the grid; the others keep their places around it.
            const Tick by = roll::snap (s, std::max<Tick> (0, dragStart + raw), controller.grid) - dragStart;
            const int semis = static_cast<int> (std::lround ((downAt.y - p.y) / rowHeight()));
            if (semis != dragSemitones && pt != nullptr) controller.previewPitches ({ std::clamp (dragPitch + semis, 0, 127) }, pt->id, 0.3);
            dragBy = by;
            dragSemitones = semis;
            const int bar = s.barAt (dragStart + by);
            const Tick inBar = dragStart + by - s.barStart (bar);
            controller.setStatus (juce::String (dragCopy ? "Copying" : "Moving") + " to bar " + juce::String (bar + 1) + " beat "
                                  + juce::String (1.0 + static_cast<double> (inBar) / static_cast<double> (s.meterAtBar (bar).beatTicks()), 2)
                                  + (semis != 0 ? ", " + juce::String (semis > 0 ? "+" : "") + juce::String (semis) + " semitones" : juce::String()));
            repaint();
            return;
        }
        case Mode::stretchEnd:
            dragged = true;
            dragBy = roll::snap (s, dragEnd + raw, controller.grid) - dragEnd;
            repaint();
            return;
        case Mode::stretchStart:
            dragged = true;
            dragBy = roll::snap (s, dragStart + raw, controller.grid) - dragStart;
            repaint();
            return;
        case Mode::lasso:
            dragged = true;
            lasso = juce::Rectangle<float> (downAt, p).getIntersection (gridArea().toFloat());
            repaint();
            return;
        case Mode::draw:
        {
            dragged = true;
            const Tick to = roll::snap (s, timeline.tickAt (p.x), controller.grid);
            drawLength = std::max (controller.grid.snap ? controller.grid.step() : minLength(), to - drawStart);
            repaint();
            return;
        }
        case Mode::velocity:
            velocityDrag (p);
            return;
        case Mode::none:
            return;
    }
}

void PianoRollView::velocityDrag (juce::Point<float> p)
{
    const auto* pt = part();
    if (pt == nullptr) return;
    const auto area = velocityArea().toFloat();
    const float h = area.getHeight() - 6.0f;
    const int v = std::clamp (static_cast<int> (std::lround ((area.getBottom() - 1.0f - p.y) / h * 127.0f)), 1, 127);
    if (velocityRelative)
    {
        const int delta = static_cast<int> (std::lround ((downAt.y - p.y) / h * 127.0f));
        for (const auto& [id, from] : velocityFrom) velocityEdits[id] = std::clamp (from + delta, 1, 127);
        controller.setStatus ("Velocity " + juce::String (delta >= 0 ? "+" : "") + juce::String (delta));
    }
    else
    {
        // Every stem the pointer has passed since the last move takes the new height.
        const float a = std::min (lastVelocityX, p.x) - 4.0f, b = std::max (lastVelocityX, p.x) + 4.0f;
        for (const auto& n : pt->notes)
        {
            const float x = timeline.xOf (n.start);
            if (x >= a && x <= b) velocityEdits[n.id] = v;
        }
        controller.setStatus ("Velocity " + juce::String (v));
    }
    lastVelocityX = p.x;
    repaint();
}

void PianoRollView::mouseUp (const juce::MouseEvent& e)
{
    const auto m = mode;
    mode = Mode::none;
    const auto* pt = part();
    switch (m)
    {
        case Mode::move:
            if (dragged && (dragBy != 0 || dragSemitones != 0)) controller.dragSelection (dragBy, dragSemitones, dragCopy);
            break;
        case Mode::stretchEnd:
        case Mode::stretchStart:
            if (dragged && dragBy != 0) controller.stretchSelection (dragBy, m == Mode::stretchStart);
            break;
        case Mode::lasso:
            if (! dragged)
            {
                // A click on empty space: nothing selected, and the caret there.
                controller.select ({});
                controller.setCaret (controller.caretPart, roll::snap (controller.score, timeline.tickAt (e.position.x), controller.grid));
            }
            else if (pt != nullptr)
            {
                Selection sel = e.mods.isShiftDown() ? controller.selection : Selection {};
                for (const auto& n : pt->notes)
                    if (noteRect (n.start, n.end(), n.pitch).intersects (lasso)) sel.insert (n.id);
                controller.select (sel);
                controller.setStatus (juce::String (static_cast<int> (sel.size())) + " notes selected");
            }
            break;
        case Mode::draw:
            if (pt != nullptr) controller.drawNoteAt (pt->id, drawStart, drawPitch, drawLength);
            break;
        case Mode::velocity:
            if (! velocityEdits.empty()) controller.setVelocities (velocityEdits);
            velocityEdits.clear();
            velocityFrom.clear();
            break;
        case Mode::caret:
        case Mode::keys:
        case Mode::none:
            break;
    }
    keyPitch = -1;
    dragged = false;
    dragBy = 0;
    dragSemitones = 0;
    dragCopy = false;
    lasso = {};
    repaint();
}

void PianoRollView::mouseDoubleClick (const juce::MouseEvent& e)
{
    const auto* pt = part();
    if (pt == nullptr || ! gridArea().contains (e.position.toInt()) || controller.drawTool) return;
    if (const auto* hit = noteAt (e.position))
    {
        // A chord: all of it, heard together.
        Selection sel;
        std::vector<int> pitches;
        for (const auto& n : pt->notes)
            if (n.start == hit->start) { sel.insert (n.id); pitches.push_back (n.pitch); }
        controller.select (sel);
        controller.previewPitches (pitches, pt->id, 1.0);
        return;
    }
    // Empty space: a note, one grid step long.
    controller.drawNoteAt (pt->id, roll::snapDown (controller.score, timeline.tickAt (e.position.x), controller.grid), pitchAt (e.position.y));
}

void PianoRollView::mouseMove (const juce::MouseEvent& e)
{
    hover = e.position;
    setTooltip (tooltipAt (e.position));
    int zone = 0;
    const bool inGrid = gridArea().contains (e.position.toInt());
    const Note* n = inGrid ? noteAt (e.position, &zone) : nullptr;
    if (controller.drawTool && inGrid) setMouseCursor (n != nullptr ? juce::MouseCursor::NormalCursor : juce::MouseCursor::CrosshairCursor);
    else if (n != nullptr && zone != 0) setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
    else if (n != nullptr) setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    else setMouseCursor (juce::MouseCursor::NormalCursor);
    if (e.position.x < Timeline::left) repaint (0, 0, Timeline::left, getHeight());
}

void PianoRollView::mouseExit (const juce::MouseEvent&)
{
    hover = { -1, -1 };
    repaint (0, 0, Timeline::left, getHeight());
}

void PianoRollView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (e.mods.isCommandDown())
    {
        timeline.zoomBy (w.deltaY > 0 ? 1.12f : 1.0f / 1.12f, std::max (e.position.x, static_cast<float> (Timeline::left)));
        return;
    }
    if (e.mods.isAltDown())
    {
        // Taller or shorter keys, keeping the key under the pointer where it is.
        const int anchor = pitchAt (e.position.y);
        const float y = e.position.y;
        controller.rowHeight = juce::jlimit (5.0f, 28.0f, controller.rowHeight * (w.deltaY > 0 ? 1.1f : 1.0f / 1.1f));
        scrollY = (127 - anchor) * static_cast<double> (rowHeight()) - (y - gridArea().getY()) + rowHeight() * 0.5;
        updateScrollbar();
        controller.viewChanged();
        return;
    }
    const double amount = 220.0;
    if (e.mods.isShiftDown() || std::abs (w.deltaX) > std::abs (w.deltaY))
    {
        timeline.scrollTo (timeline.scrollX - (e.mods.isShiftDown() ? w.deltaY : w.deltaX) * amount);
        return;
    }
    scrollY -= w.deltaY * amount;
    updateScrollbar();
    repaint();
}

void PianoRollView::showMenu (const Note* note)
{
    enum { del = 1, dup, quant, all, draw };
    juce::PopupMenu m;
    const bool any = ! controller.selection.empty();
    m.addItem (del, "Delete", any);
    m.addItem (dup, "Duplicate (Cmd+D)", any);
    m.addItem (quant, any ? "Quantise selected notes to " + juce::String (controller.grid.name()) + " (Q)"
                          : "Quantise the part to " + juce::String (controller.grid.name()) + " (Q)");
    m.addSeparator();
    m.addItem (all, "Select every note in this part");
    m.addItem (draw, "Draw tool (D)", true, controller.drawTool);
    juce::ignoreUnused (note);
    m.showMenuAsync (juce::PopupMenu::Options(), [this] (int r)
    {
        if (r == del) controller.deleteSelection();
        if (r == dup) controller.duplicateSelection();
        if (r == quant) controller.quantiseSelection();
        if (r == draw) controller.toggleDrawTool();
        if (r == all)
            if (const auto* p = part())
            {
                Selection sel;
                for (const auto& n : p->notes) sel.insert (n.id);
                controller.select (sel);
            }
    });
}

juce::String PianoRollView::tooltipAt (juce::Point<float> p) const
{
    if (p.y < rulerHeight) return p.x >= Timeline::left ? "Click to put the caret here: playback starts from it" : juce::String();
    if (velocityArea().contains (p.toInt()))
        return "How hard each note is played. Drag across the stems to set them; drag a selected note's stem to change all the selected together.";
    const auto* pt = part();
    if (pt == nullptr) return {};
    if (p.x < Timeline::left)
        return drums() ? "Click a drum to hear it" : "Click a key to hear it. The bar beside the keys shows the instrument's range, brighter where it sounds best.";
    const Note* n = noteAt (p);
    if (n == nullptr) return controller.drawTool ? "Click to draw a note; drag to make it longer" : "Double-click to draw a note; drag to select several";
    const auto& s = controller.score;
    const auto& inst = instrumentById (pt->instrument);
    const auto ctx = keyContext (s.keyAtBar (s.barAt (n->start)).root, s.keyAtBar (s.barAt (n->start)).scale);
    juce::StringArray lines;
    const juce::String name = inst.drums && ! roll::drumName (n->pitch).empty() ? juce::String (roll::drumName (n->pitch)) : juce::String (pitchName (n->pitch, ctx));
    lines.add (name + "  -  " + juce::String (pt->name) + ", bar " + juce::String (s.barAt (n->start) + 1) + ", velocity " + juce::String (n->velocity));
    const auto it = controller.warnings.find (n->id);
    if (it != controller.warnings.end())
    {
        const auto& w = it->second;
        if (w.outOfRange)
            lines.add ("Out of the " + juce::String (inst.name) + "'s range (" + juce::String (pitchName (inst.low, ctx)) + " to "
                       + juce::String (pitchName (inst.high, ctx)) + ").");
        else if (w.outsideSweet)
            lines.add ("Playable, but outside where the " + juce::String (inst.name) + " sounds most like itself ("
                       + juce::String (pitchName (inst.sweetLow, ctx)) + " to " + juce::String (pitchName (inst.sweetHigh, ctx)) + ").");
        if (w.tooManyNotes)
            lines.add (inst.monophonic() ? "A chord: the " + juce::String (inst.name) + " plays one note at a time."
                                         : "More notes at once than the " + juce::String (inst.name) + " plays.");
        if (w.tooFast) lines.add ("Faster than the " + juce::String (inst.name) + " plays cleanly at this tempo.");
        if (w.tooWide) lines.add ("A wider leap than the " + juce::String (inst.name) + " takes comfortably in passing.");
    }
    return lines.joinIntoString ("\n");
}

} // namespace nt
