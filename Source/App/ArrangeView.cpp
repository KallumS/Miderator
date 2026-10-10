#include "ArrangeView.h"

#include "ScaleModel.h"

namespace nt
{

ArrangeView::ArrangeView (Controller& c, Timeline& t) : controller (c), timeline (t)
{
    addAndMakeVisible (vbar);
    vbar.addListener (this);
    vbar.setAutoHide (false);
    controller.addChangeListener (this);
    timeline.addChangeListener (this);
}

ArrangeView::~ArrangeView()
{
    controller.removeChangeListener (this);
    timeline.removeChangeListener (this);
}

//==============================================================================
// Geometry

juce::Rectangle<int> ArrangeView::tracksArea() const
{
    return getLocalBounds().withTrimmedTop (lanesHeight).withTrimmedRight (Timeline::right);
}

float ArrangeView::trackTop (int index) const
{
    return static_cast<float> (lanesHeight + index * trackHeight - scrollY);
}

int ArrangeView::trackAt (float y) const
{
    const int n = static_cast<int> (controller.score.parts.size());
    if (n == 0) return 0;
    const int i = static_cast<int> (std::floor ((static_cast<double> (y) - lanesHeight + scrollY) / trackHeight));
    return std::clamp (i, 0, n - 1);
}

int ArrangeView::barAt (float x) const
{
    const auto& s = controller.score;
    return std::clamp (s.barAt (std::max<Tick> (0, timeline.tickAt (std::max (x, static_cast<float> (Timeline::left))))), 0, std::max (0, s.bars - 1));
}

juce::Rectangle<float> ArrangeView::muteBox (int index) const
{
    return { static_cast<float> (Timeline::left) - 50.0f, trackTop (index) + static_cast<float> (trackHeight) - 21.0f, 20.0f, 16.0f };
}

juce::Rectangle<float> ArrangeView::soloBox (int index) const
{
    return muteBox (index).translated (23.0f, 0.0f);
}

void ArrangeView::resized()
{
    vbar.setBounds (getWidth() - Timeline::right, lanesHeight, Timeline::right, getHeight() - lanesHeight);
    updateScrollbar();
}

void ArrangeView::updateScrollbar()
{
    const double content = static_cast<double> (controller.score.parts.size()) * trackHeight + 8.0;
    const double view = tracksArea().getHeight();
    scrollY = juce::jlimit (0.0, std::max (0.0, content - view), scrollY);
    vbar.setRangeLimits (0, std::max (content, view), juce::dontSendNotification);
    vbar.setCurrentRange (scrollY, view, juce::dontSendNotification);
}

void ArrangeView::scrollBarMoved (juce::ScrollBar*, double start)
{
    scrollY = start;
    repaint();
}

void ArrangeView::revealPart (uint32_t partId)
{
    const int i = controller.score.partIndex (partId);
    if (i < 0) return;
    const double top = static_cast<double> (i) * trackHeight, view = tracksArea().getHeight();
    if (top < scrollY) scrollY = top;
    else if (top + trackHeight > scrollY + view) scrollY = top + trackHeight - view;
    updateScrollbar();
    repaint();
}

void ArrangeView::changeListenerCallback (juce::ChangeBroadcaster*)
{
    updateScrollbar();
    repaint();
}

void ArrangeView::showPlayhead (Tick t, const std::vector<uint32_t>& sounding)
{
    if (t == lastPlayhead && sounding == lastSounding) return;
    lastPlayhead = t;
    lastSounding = sounding;
    repaint();
}

//==============================================================================
// Painting

void ArrangeView::paint (juce::Graphics& g)
{
    const auto c = theme::rollColours (controller.lightTheme);
    const auto& s = controller.score;
    const auto area = tracksArea();
    g.fillAll (c.trackRowAlt);

    const std::set<uint32_t> selected (controller.selection.begin(), controller.selection.end());
    const std::set<uint32_t> sounding (lastSounding.begin(), lastSounding.end());
    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (area.withTrimmedLeft (Timeline::left));
        const auto timeArea = area.withTrimmedLeft (Timeline::left);
        for (size_t i = 0; i < s.parts.size(); ++i)
        {
            const float top = trackTop (static_cast<int> (i));
            if (top > static_cast<float> (area.getBottom()) || top + trackHeight < static_cast<float> (area.getY())) continue;
            g.setColour (i % 2 == 0 ? c.trackRow : c.trackRowAlt);
            g.fillRect (static_cast<float> (timeArea.getX()), top, static_cast<float> (timeArea.getWidth()), static_cast<float> (trackHeight));
        }
        paint::grid (g, timeArea, timeline, s, controller.grid, c, false);
        // Each track's notes, small.
        paint::Mini mini;
        mini.selected = &selected;
        mini.sounding = &sounding;
        mini.warnings = &controller.warnings;
        for (size_t i = 0; i < s.parts.size(); ++i)
        {
            const float top = trackTop (static_cast<int> (i));
            if (top > static_cast<float> (area.getBottom()) || top + trackHeight < static_cast<float> (area.getY())) continue;
            const juce::Rectangle<float> lane (static_cast<float> (timeArea.getX()), top + 5.0f, static_cast<float> (timeArea.getWidth()), static_cast<float> (trackHeight) - 10.0f);
            paint::miniNotes (g, lane, s.parts[i].notes, [this] (Tick t) { return timeline.xOf (t); }, c, mini);
            g.setColour (c.barLine.withAlpha (0.35f));
            g.fillRect (static_cast<float> (timeArea.getX()), top + static_cast<float> (trackHeight) - 1.0f, static_cast<float> (timeArea.getWidth()), 1.0f);
        }

        // The chosen bars: a tint with an outline (decision 0019).
        const auto& r = controller.range;
        if (r.active())
        {
            const int top = s.partIndex (r.parts.front()), bottom = s.partIndex (r.parts.back());
            if (top >= 0 && bottom >= 0)
            {
                const float x1 = timeline.xOf (s.barStart (r.first)), x2 = timeline.xOf (s.barStart (r.last + 1));
                const juce::Rectangle<float> box (x1, trackTop (top) + 1.0f, x2 - x1, static_cast<float> ((bottom - top + 1) * trackHeight) - 2.0f);
                g.setColour (c.selected.withAlpha (controller.lightTheme ? 0.16f : 0.10f));
                g.fillRect (box);
                g.setColour (c.selected.withAlpha (0.9f));
                g.drawRect (box, 1.5f);
            }
        }

        // The caret and the playhead, across every track.
        g.setColour (controller.stepInput ? c.selected : c.page.dim);
        g.fillRect (timeline.xOf (controller.caret), static_cast<float> (area.getY()), controller.stepInput ? 2.0f : 1.0f, static_cast<float> (area.getHeight()));
        if (lastPlayhead >= 0)
        {
            g.setColour (c.sounding);
            g.fillRect (timeline.xOf (lastPlayhead) - 1.0f, static_cast<float> (area.getY()), 2.0f, static_cast<float> (area.getHeight()));
        }
    }
    paintHeaders (g, c);
    paintLanes (g, c);
}

void ArrangeView::paintHeaders (juce::Graphics& g, const theme::RollColours& c)
{
    const auto& s = controller.score;
    const auto area = tracksArea();
    juce::Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (area.withWidth (Timeline::left));
    g.setColour (theme::ground);
    g.fillRect (area.withWidth (Timeline::left));
    for (size_t i = 0; i < s.parts.size(); ++i)
    {
        const auto& p = s.parts[i];
        const int index = static_cast<int> (i);
        const float top = trackTop (index);
        if (top > static_cast<float> (area.getBottom()) || top + trackHeight < static_cast<float> (area.getY())) continue;
        const bool current = p.id == controller.caretPart && ! controller.noPartChosen;
        const juce::Rectangle<float> box (2.0f, top + 1.0f, static_cast<float> (Timeline::left) - 4.0f, static_cast<float> (trackHeight) - 2.0f);
        g.setColour (current ? c.headerActive : c.header);
        g.fillRoundedRectangle (box, 3.0f);
        if (current)
        {
            g.setColour (theme::accent);
            g.fillRect (box.getX(), box.getY() + 3.0f, 3.0f, box.getHeight() - 6.0f);
        }
        const auto& inst = instrumentById (p.instrument);
        g.setColour (c.headerText);
        g.setFont (juce::FontOptions (13.5f, juce::Font::bold));
        g.drawText (p.name, juce::Rectangle<float> (box.getX() + 10.0f, top + 4.0f, box.getWidth() - 14.0f, 18.0f), juce::Justification::centredLeft, true);
        g.setColour (c.headerText.withAlpha (0.6f));
        g.setFont (juce::FontOptions (11.5f));
        g.drawText (p.name == inst.name ? juce::String (inst.family) : juce::String (inst.name),
                    juce::Rectangle<float> (box.getX() + 10.0f, top + 22.0f, box.getWidth() - 66.0f, 16.0f), juce::Justification::centredLeft, true);
        // Mute and solo.
        for (int k = 0; k < 2; ++k)
        {
            const auto b = k == 0 ? muteBox (index) : soloBox (index);
            const bool on = k == 0 ? p.mute : p.solo;
            g.setColour (on ? theme::accent : theme::control);
            g.fillRoundedRectangle (b, 2.5f);
            g.setColour (theme::ink);
            g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
            g.drawText (k == 0 ? "M" : "S", b, juce::Justification::centred, false);
        }
    }
}

void ArrangeView::paintLanes (juce::Graphics& g, const theme::RollColours& c)
{
    juce::ignoreUnused (c);
    const auto& s = controller.score;
    g.setColour (theme::sunken);
    g.fillRect (0, 0, getWidth(), lanesHeight);

    const auto rulerArea = juce::Rectangle<int> (Timeline::left, 0, getWidth() - Timeline::left - Timeline::right, rulerHeight);
    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (rulerArea);
        paint::ruler (g, rulerArea, timeline, s, controller.caret, controller.stepInput);
    }
    g.setColour (theme::rule);
    g.fillRect (0, rulerHeight + scaleHeight, getWidth(), 1);
    g.fillRect (0, lanesHeight - 1, getWidth(), 1);
    g.fillRect (0, rulerHeight - 1, Timeline::left, 1);

    g.setColour (theme::textDim);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("Bars", 10, 0, Timeline::left - 16, rulerHeight, juce::Justification::centredLeft);
    g.drawText ("Scale", 10, rulerHeight, Timeline::left - 16, scaleHeight, juce::Justification::centredLeft);
    g.drawText ("Chords", 10, rulerHeight + scaleHeight, Timeline::left - 16, chordHeight, juce::Justification::centredLeft);

    juce::Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (Timeline::left, rulerHeight, getWidth() - Timeline::left - Timeline::right, lanesHeight - rulerHeight);
    const float left = static_cast<float> (Timeline::left);

    g.setFont (juce::FontOptions (12.5f));
    for (const auto& k : controller.keys)
    {
        const float x1 = timeline.xOf (k.start), x2 = timeline.xOf (k.end);
        g.setColour (theme::frameActive);
        g.fillRoundedRectangle (x1 + 2.0f, static_cast<float> (rulerHeight) + 2.0f, std::max (4.0f, x2 - x1 - 4.0f), static_cast<float> (scaleHeight) - 4.0f, 3.0f);
        // The label stays in view while any of its span is.
        const float lx = std::max (x1 + 8.0f, left + 6.0f);
        g.setColour (theme::text);
        g.drawText (k.label, juce::Rectangle<float> (lx, static_cast<float> (rulerHeight) + 2.0f, std::max (20.0f, x2 - lx - 4.0f), static_cast<float> (scaleHeight) - 4.0f),
                    juce::Justification::centredLeft, true);
    }

    g.setFont (juce::FontOptions (14.5f, juce::Font::bold));
    const float laneTop = static_cast<float> (rulerHeight + scaleHeight);
    for (const auto& ch : controller.chords)
    {
        if (ch.name.empty()) continue;
        const float x1 = timeline.xOf (ch.start), x2 = timeline.xOf (ch.end);
        const bool lit = controller.audio.isPlaying() && lastPlayhead >= ch.start && lastPlayhead < ch.end;
        g.setColour (lit ? theme::accent : theme::control);
        g.fillRect (x1 + 1.0f, static_cast<float> (lanesHeight) - 5.0f, std::max (2.0f, x2 - x1 - 2.0f), 2.0f);
        const float lx = std::max (x1 + 3.0f, left + 4.0f);
        if (lx > x2 - 8.0f) continue;
        g.setColour (lit ? theme::accent : theme::text);
        g.drawText (ch.name, juce::Rectangle<float> (lx, laneTop + 1.0f, std::max (30.0f, x2 - lx - 1.0f), static_cast<float> (chordHeight) - 7.0f),
                    juce::Justification::centredLeft, true);
    }
}

//==============================================================================
// Mouse

void ArrangeView::mouseDown (const juce::MouseEvent& e)
{
    // The window takes the keys, so Space and the arrows work after a click here.
    focusWindow (*this);
    const auto p = e.position;
    auto& s = controller.score;
    selectingBars = allParts = settingCaret = false;

    // The ruler: the caret goes there.
    if (p.y < rulerHeight)
    {
        if (p.x < Timeline::left) return;
        settingCaret = true;
        controller.setCaret (controller.caretPart, roll::snap (s, timeline.tickAt (p.x), controller.grid));
        return;
    }

    // The lanes: a chord plays the harmony there, a scale plays itself.
    if (p.y < lanesHeight)
    {
        if (p.x < Timeline::left) return;
        const Tick t = timeline.tickAt (p.x);
        // A drag along the lanes chooses bars in every part.
        anchorBar = barAt (p.x);
        anchorPart = 0;
        allParts = true;
        if (p.y < rulerHeight + scaleHeight)
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
                controller.select (notesInRange (s, c.start, c.end));
                controller.setStatus (juce::String (c.name) + " - bar " + juce::String (s.barAt (c.start) + 1));
                return;
            }
        return;
    }

    if (s.parts.empty()) return;
    const int index = trackAt (p.y);
    const float rowTop = trackTop (index);
    if (p.y > rowTop + trackHeight) return;   // below the last track
    const auto partId = s.parts[static_cast<size_t> (index)].id;

    // The names: mute, solo, or show that part in the roll.
    if (p.x < Timeline::left)
    {
        if (muteBox (index).contains (p) || soloBox (index).contains (p))
        {
            const bool mute = muteBox (index).contains (p);
            controller.edit (mute ? "Muted a part" : "Soloed a part", [partId, mute] (Score& sc)
            {
                if (auto* part = sc.partById (partId)) { if (mute) part->mute = ! part->mute; else part->solo = ! part->solo; }
            });
            return;
        }
        controller.choosePart (partId);
        controller.setStatus (juce::String (s.parts[static_cast<size_t> (index)].name) + " in the piano roll");
        return;
    }

    // In a track: that bar is chosen and the part goes in the roll; a drag
    // chooses more bars, and more parts (decision 0019). Shift and a click
    // stretches the bars chosen.
    const int bar = barAt (p.x);
    if (e.mods.isShiftDown() && controller.range.active())
    {
        controller.selectRange (anchorBar, bar, anchorPart, index);
        controller.setStatus (controller.rangeText() + " chosen");
        return;
    }
    anchorBar = bar;
    anchorPart = index;
    selectingBars = true;
    controller.selectRange (bar, bar, index, index);
    controller.caret = roll::snapDown (s, timeline.tickAt (p.x), controller.grid);
    controller.setStatus (controller.rangeText() + " chosen - drag to choose more, Generate fills them");
}

void ArrangeView::mouseDrag (const juce::MouseEvent& e)
{
    if (e.mouseWasClicked()) return;
    if (settingCaret)
    {
        controller.setCaret (controller.caretPart, roll::snap (controller.score, timeline.tickAt (e.position.x), controller.grid));
        return;
    }
    if (! selectingBars && ! (allParts && e.mouseDownPosition.y < lanesHeight)) return;
    const int bar = barAt (e.position.x);
    const int last = static_cast<int> (controller.score.parts.size()) - 1;
    if (last < 0) return;
    const int part = allParts ? last : trackAt (e.position.y);
    const auto& r = controller.range;
    const int first = std::min (anchorBar, bar), lastBar = std::max (anchorBar, bar);
    const auto topId = controller.score.parts[static_cast<size_t> (std::min (anchorPart, part))].id;
    const auto bottomId = controller.score.parts[static_cast<size_t> (std::max (anchorPart, part))].id;
    if (! r.active() || r.first != first || r.last != lastBar || r.parts.front() != topId || r.parts.back() != bottomId)
    {
        controller.selectRange (anchorBar, bar, anchorPart, part);
        controller.setStatus (controller.rangeText() + " chosen");
    }
    selectingBars = true;
}

void ArrangeView::mouseUp (const juce::MouseEvent&)
{
    if (selectingBars && controller.range.active())
        controller.setStatus (controller.rangeText() + " chosen - Generate fills them; Esc lets go");
    selectingBars = allParts = settingCaret = false;
}

void ArrangeView::mouseDoubleClick (const juce::MouseEvent& e)
{
    const auto& s = controller.score;
    if (e.position.y < lanesHeight || s.parts.empty()) return;
    const int index = trackAt (e.position.y);
    if (e.position.x >= Timeline::left || muteBox (index).contains (e.position) || soloBox (index).contains (e.position)) return;
    // A part's name: everything it plays.
    const auto& part = s.parts[static_cast<size_t> (index)];
    Selection sel;
    for (const auto& n : part.notes) sel.insert (n.id);
    controller.select (sel);
    controller.setStatus ("All of " + juce::String (part.name) + ": " + juce::String (static_cast<int> (sel.size())) + " notes");
}

void ArrangeView::mouseMove (const juce::MouseEvent& e)
{
    setTooltip (tooltipAt (e.position));
}

void ArrangeView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (e.mods.isCommandDown())
    {
        timeline.zoomBy (w.deltaY > 0 ? 1.12f : 1.0f / 1.12f, std::max (e.position.x, static_cast<float> (Timeline::left)));
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

juce::String ArrangeView::tooltipAt (juce::Point<float> p) const
{
    if (p.y < rulerHeight) return p.x >= Timeline::left ? "Click to put the caret here: playback starts from it" : juce::String();
    if (p.y < rulerHeight + scaleHeight) return "The scale the music is in, read from the notes. Click to hear it.";
    if (p.y < lanesHeight) return "The chords, read from every part together. Click one to hear it; drag along here to choose bars in every part.";
    const auto& s = controller.score;
    if (s.parts.empty()) return {};
    const int index = trackAt (p.y);
    if (p.x < Timeline::left)
    {
        if (muteBox (index).contains (p)) return "Mute";
        if (soloBox (index).contains (p)) return "Solo";
        return "Click to show this part in the piano roll; double-click to select all of it";
    }
    return "Click a bar to choose it, drag to choose more bars and parts: Generate fills them";
}

} // namespace nt
