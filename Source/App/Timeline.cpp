#include "Timeline.h"

#include "Follow.h"

namespace nt
{

double Timeline::contentWidth() const
{
    const auto& s = controller.score;
    return static_cast<double> (s.barStart (s.bars + 4)) * pixelsPerTick();
}

void Timeline::scrollTo (double x)
{
    const double limit = std::max (0.0, contentWidth() - viewWidth);
    x = juce::jlimit (0.0, limit, x);
    if (std::abs (x - scrollX) < 0.01) return;
    scrollX = x;
    sendChangeMessage();
}

void Timeline::zoomBy (float factor, float anchorX)
{
    // The time under the pointer stays under it.
    const Tick anchor = tickAt (anchorX);
    controller.zoom = juce::jlimit (6.0f, 400.0f, controller.zoom * factor);
    scrollX = std::max (0.0, static_cast<double> (anchor) * pixelsPerTick() - (anchorX - left));
    controller.viewChanged();
    scrollTo (scrollX);
    sendChangeMessage();
}

void Timeline::reveal (Tick t)
{
    const double x = static_cast<double> (t) * pixelsPerTick();
    if (x < scrollX + 10 || x > scrollX + viewWidth - 40) scrollTo (x - viewWidth * 0.15);
}

void Timeline::showAt (Tick t, double fraction)
{
    scrollTo (static_cast<double> (t) * pixelsPerTick() - viewWidth * fraction);
}

void Timeline::follow (Tick t)
{
    // Whole pixels, so the notes stay crisp as they move.
    scrollTo (std::round (followScroll (static_cast<double> (t) * pixelsPerTick(), scrollX, viewWidth, controller.followStyle)));
}

//==============================================================================

namespace paint
{

void grid (juce::Graphics& g, juce::Rectangle<int> area, const Timeline& tl, const Score& score,
           const roll::Grid& gr, const theme::RollColours& c, bool steps)
{
    const float top = static_cast<float> (area.getY()), h = static_cast<float> (area.getHeight());
    const float x0 = static_cast<float> (area.getX()), x1 = static_cast<float> (area.getRight());
    const int firstBar = std::max (0, score.barAt (std::max<Tick> (0, tl.tickAt (x0))));
    const double ppt = tl.pixelsPerTick();
    for (int bar = firstBar;; ++bar)
    {
        const Tick start = score.barStart (bar);
        const float bx = tl.xOf (start);
        if (bx > x1) break;
        const auto& m = score.meterAtBar (bar);
        const Tick beat = m.beatTicks(), length = m.barTicks();
        // Grid steps, where they are at least six pixels apart.
        if (steps && gr.snap && static_cast<double> (gr.step()) * ppt >= 6.0 && gr.step() < beat)
        {
            g.setColour (c.stepLine);
            for (Tick t = gr.step(); t < length; t += gr.step())
                if (t % beat != 0) g.fillRect (tl.xOf (start + t), top, 1.0f, h);
        }
        // Beats, where a beat is at least four pixels.
        if (static_cast<double> (beat) * ppt >= 4.0)
        {
            g.setColour (c.beatLine);
            for (Tick t = beat; t < length; t += beat) g.fillRect (tl.xOf (start + t), top, 1.0f, h);
        }
        g.setColour (c.barLine);
        g.fillRect (bx, top, 1.0f, h);
        if (bar > score.bars + 64) break;
    }
    // The end of the score.
    const float ex = tl.xOf (score.endTick());
    if (ex >= x0 && ex <= x1)
    {
        g.setColour (c.barLine);
        g.fillRect (ex - 1.0f, top, 3.0f, h);
    }
}

void ruler (juce::Graphics& g, juce::Rectangle<int> area, const Timeline& tl, const Score& score, Tick caret, bool caretLit)
{
    g.setColour (theme::sunken);
    g.fillRect (area);
    g.setColour (theme::rule);
    g.fillRect (area.getX(), area.getBottom() - 1, area.getWidth(), 1);
    const float x1 = static_cast<float> (area.getRight());
    const float bottom = static_cast<float> (area.getBottom());
    // Bar numbers spaced so they never collide: every bar, or every 2, 4, 8...
    const double barPx = static_cast<double> (score.barLength (0)) * tl.pixelsPerTick();
    int every = 1;
    while (barPx * every < 34.0) every *= 2;
    g.setFont (juce::FontOptions (11.5f));
    const int firstBar = std::max (0, score.barAt (std::max<Tick> (0, tl.tickAt (static_cast<float> (area.getX())))));
    for (int bar = firstBar;; ++bar)
    {
        const Tick start = score.barStart (bar);
        const float bx = tl.xOf (start);
        if (bx > x1 || bar > score.bars + 64) break;
        const bool numbered = bar % every == 0;
        g.setColour (bar < score.bars ? theme::textDim : theme::rule);
        g.fillRect (bx, numbered ? static_cast<float> (area.getY()) + 3.0f : bottom - 7.0f, 1.0f, numbered ? bottom - static_cast<float> (area.getY()) - 3.0f : 7.0f);
        if (numbered)
        {
            g.setColour (bar < score.bars ? theme::text : theme::textDim);
            g.drawText (juce::String (bar + 1), juce::Rectangle<float> (bx + 4.0f, static_cast<float> (area.getY()), 40.0f, bottom - static_cast<float> (area.getY()) - 2.0f),
                        juce::Justification::centredLeft, false);
        }
        // Beat ticks, where there is room.
        const auto& m = score.meterAtBar (bar);
        if (static_cast<double> (m.beatTicks()) * tl.pixelsPerTick() >= 10.0)
        {
            g.setColour (theme::rule);
            for (Tick t = m.beatTicks(); t < m.barTicks(); t += m.beatTicks()) g.fillRect (tl.xOf (start + t), bottom - 4.0f, 1.0f, 4.0f);
        }
    }
    // The caret: a small flag where playback starts and pasting goes.
    const float cx = tl.xOf (caret);
    if (cx >= static_cast<float> (area.getX()) - 6.0f && cx <= x1)
    {
        juce::Path flag;
        flag.addTriangle (cx - 5.0f, static_cast<float> (area.getY()) + 3.0f, cx + 5.0f, static_cast<float> (area.getY()) + 3.0f, cx, bottom - 3.0f);
        g.setColour (caretLit ? theme::accent : theme::control);
        g.fillPath (flag);
    }
}

void miniNotes (juce::Graphics& g, juce::Rectangle<float> area, const std::vector<Note>& notes,
                const std::function<float (Tick)>& xOf, const theme::RollColours& c, const Mini& opts)
{
    if (notes.empty()) return;
    int lo = 127, hi = 0;
    for (const auto& n : notes) { lo = std::min (lo, n.pitch); hi = std::max (hi, n.pitch); }
    // At least an octave tall, centred on the music.
    if (hi - lo < 12) { const int mid = (lo + hi) / 2; lo = mid - 6; hi = lo + 12; }
    const float rowH = area.getHeight() / static_cast<float> (hi - lo + 1);
    const float h = std::clamp (rowH, 2.0f, std::max (2.0f, opts.tallest));
    const float x0 = area.getX(), x1 = area.getRight();
    for (const auto& n : notes)
    {
        float a = xOf (n.start), b = xOf (n.end());
        if (b < x0 || a > x1) continue;
        a = std::max (a, x0);
        b = std::min (b, x1);
        const float y = area.getBottom() - static_cast<float> (n.pitch - lo + 1) * rowH + (rowH - h) * 0.5f;
        juce::Colour col = c.note;
        if (opts.warnings != nullptr)
        {
            const auto it = opts.warnings->find (n.id);
            if (it != opts.warnings->end() && it->second.outOfRange) col = c.page.warn;
        }
        if (opts.selected != nullptr && opts.selected->count (n.id) != 0) col = c.selected;
        if (opts.sounding != nullptr && opts.sounding->count (n.id) != 0) col = c.sounding;
        g.setColour (col);
        g.fillRect (a, y, std::max (1.5f, b - a - 1.0f), h);
    }
}

} // namespace paint

} // namespace nt
