#include "ScoreRenderer.h"

#include "BinaryData.h"

#include <cmath>

namespace nt
{

using namespace engrave;

//==============================================================================

Glyphs& Glyphs::get()
{
    static Glyphs instance;
    return instance;
}

Glyphs::Glyphs()
{
    typeface = juce::Typeface::createSystemTypefaceFor (BinaryData::Bravura_otf, BinaryData::Bravura_otfSize);
    // A black notehead is exactly one staff space tall (Bravura's metadata:
    // -0.5 to 0.5), which calibrates the font against the staff whatever the
    // platform's idea of a font's height is.
    const auto probe = makePath (smufl::noteheadBlack, 100.0f).getBounds();
    unitsPerSpace = probe.getHeight() > 0 ? probe.getHeight() : 25.0f;
}

juce::Path Glyphs::makePath (juce::juce_wchar cp, float height)
{
    juce::Path p;
    if (typeface == nullptr) return p;
    juce::Font font (juce::FontOptions (typeface).withHeight (height));
    juce::GlyphArrangement ga;
    ga.addLineOfText (font, juce::String::charToString (cp), 0.0f, 0.0f);
    ga.createPath (p);
    return p;
}

const juce::Path& Glyphs::path (juce::juce_wchar cp)
{
    auto it = cache.find (cp);
    if (it != cache.end()) return it->second;
    auto p = makePath (cp, 100.0f);
    p.applyTransform (juce::AffineTransform::scale (1.0f / unitsPerSpace));
    return cache.emplace (cp, std::move (p)).first->second;
}

void Glyphs::draw (juce::Graphics& g, juce::juce_wchar cp, float x, float y, float space)
{
    g.fillPath (path (cp), juce::AffineTransform::scale (space).translated (x, y));
}

//==============================================================================

namespace
{
constexpr int sharpPositions[7] { 8, 5, 9, 6, 3, 7, 4 };
constexpr int flatPositions[7]  { 4, 7, 3, 6, 2, 5, 1 };

int clefOffset (Clef c)
{
    switch (c)
    {
        case Clef::bass: return -2;
        case Clef::alto: return -1;
        case Clef::tenor: return 1;
        case Clef::treble: case Clef::treble8vb: case Clef::percussion: return 0;
    }
    return 0;
}

juce::juce_wchar clefGlyph (Clef c)
{
    switch (c)
    {
        case Clef::bass: return smufl::fClef;
        case Clef::alto: case Clef::tenor: return smufl::cClef;
        case Clef::treble8vb: return smufl::gClef8vb;
        case Clef::percussion: return smufl::percussionClef;
        case Clef::treble: return smufl::gClef;
    }
    return smufl::gClef;
}

// The line the clef's origin sits on, in spaces down from the top line.
float clefLine (Clef c)
{
    switch (c)
    {
        case Clef::bass: return 1.0f;
        case Clef::alto: case Clef::percussion: return 2.0f;
        case Clef::tenor: return 1.0f;
        case Clef::treble: case Clef::treble8vb: return 3.0f;
    }
    return 3.0f;
}

float headY (int pos) { return 4.0f - static_cast<float> (pos) * 0.5f; }

juce::juce_wchar restGlyph (int den)
{
    switch (den)
    {
        case 1: return smufl::restWhole;
        case 2: return smufl::restHalf;
        case 4: return smufl::restQuarter;
        case 8: return smufl::rest8th;
        case 16: return smufl::rest8th + 1;
        case 32: return smufl::rest8th + 2;
        default: return smufl::rest8th + 3;
    }
}

juce::juce_wchar headGlyph (int den, bool cross)
{
    if (cross) return den == 1 ? smufl::noteheadXWhole : den == 2 ? smufl::noteheadXHalf : smufl::noteheadXBlack;
    return den == 1 ? smufl::noteheadWhole : den == 2 ? smufl::noteheadHalf : smufl::noteheadBlack;
}

juce::juce_wchar accidentalGlyph (int acc)
{
    switch (acc)
    {
        case -2: return smufl::accidentalDoubleFlat;
        case -1: return smufl::accidentalFlat;
        case 1: return smufl::accidentalSharp;
        case 2: return smufl::accidentalDoubleSharp;
        default: return smufl::accidentalNatural;
    }
}

void drawNumber (juce::Graphics& g, int value, float centreX, float y, float space, juce::juce_wchar zero)
{
    auto& gl = Glyphs::get();
    const auto digits = juce::String (value);
    float width = 0;
    for (auto c : digits) width += gl.bounds (zero + static_cast<juce::juce_wchar> (c - '0')).getRight() + 0.05f;
    float x = centreX - width * space * 0.5f;
    for (auto c : digits)
    {
        const auto cp = zero + static_cast<juce::juce_wchar> (c - '0');
        gl.draw (g, cp, x, y, space);
        x += (gl.bounds (cp).getRight() + 0.05f) * space;
    }
}

void drawKey (juce::Graphics& g, const KeySignature& key, Clef clef, float x, float top, float space)
{
    if (clef == Clef::percussion) return;
    const int off = clefOffset (clef);
    for (int i = 0; i < key.count; ++i)
    {
        int pos = (key.flats ? flatPositions : sharpPositions)[i] + off;
        if (clef == Clef::tenor && ! key.flats && pos > 8) pos -= 7;
        Glyphs::get().draw (g, key.flats ? smufl::accidentalFlat : smufl::accidentalSharp,
                            x + static_cast<float> (i) * space, top + headY (pos) * space, space);
    }
}

void drawTime (juce::Graphics& g, const Meter& m, float x, float top, float space)
{
    const float centre = x + 0.9f * space;
    drawNumber (g, m.num, centre, top + 1.0f * space, space, smufl::timeSig0);
    drawNumber (g, m.den, centre, top + 3.0f * space, space, smufl::timeSig0);
}

float keyWidth (const KeySignature& k) { return static_cast<float> (k.count) * 1.0f + (k.count > 0 ? 0.6f : 0.0f); }

void drawTie (juce::Graphics& g, float x1, float x2, float y, bool above, float space)
{
    const float dir = above ? -1.0f : 1.0f;
    const float y0 = y + dir * 0.45f * space;
    const float bulge = std::min (0.9f, 0.25f + (x2 - x1) / space * 0.04f) * space;
    juce::Path p;
    p.startNewSubPath (x1, y0);
    p.quadraticTo ((x1 + x2) * 0.5f, y0 + dir * bulge, x2, y0);
    p.quadraticTo ((x1 + x2) * 0.5f, y0 + dir * (bulge - 0.18f * space), x1, y0);
    p.closeSubPath();
    g.fillPath (p);
}
} // namespace

void ScoreRenderer::drawStaffStart (juce::Graphics& g, const Staff& staff, const Measure& m, const RenderStyle& style,
                                    float x, float y, bool withTime)
{
    const float sp = style.space;
    g.setColour (style.page.ink);
    Glyphs::get().draw (g, clefGlyph (staff.clef), x + 0.8f * sp, y + clefLine (staff.clef) * sp, sp);
    drawKey (g, m.key, staff.clef, x + 4.4f * sp, y, sp);
    if (withTime) drawTime (g, m.meter, x + (4.4f + keyWidth (m.key)) * sp, y, sp);
}

void ScoreRenderer::draw (juce::Graphics& g, const Layout& lay, const Score&, const RenderStyle& style,
                          juce::Point<float> origin, juce::Rectangle<float> clip)
{
    const float sp = style.space;
    const auto& page = style.page;
    auto& gl = Glyphs::get();
    auto X = [&] (double x) { return origin.x + static_cast<float> (x) * sp; };
    const float clipL = clip.getX() - 6 * sp, clipR = clip.getRight() + 6 * sp;
    auto visible = [&] (double x) { const float px = X (x); return px >= clipL && px <= clipR; };

    // Staff lines.
    for (const auto& st : lay.staves)
    {
        const float top = origin.y + static_cast<float> (st.top) * sp;
        if (top > clip.getBottom() + 8 * sp || top + 4 * sp < clip.getY() - 8 * sp) continue;
        g.setColour (page.ink.withAlpha (0.85f));
        const float x1 = std::max (X (0), clip.getX()), x2 = std::min (X (lay.width), clip.getRight());
        const int lines = st.clef == Clef::percussion ? 5 : 5;
        for (int l = 0; l < lines; ++l)
            g.fillRect (x1, top + static_cast<float> (l) * sp - staffLineThickness * sp * 0.5f, x2 - x1,
                        std::max (1.0f, static_cast<float> (staffLineThickness) * sp));
    }

    // Bar lines, through each part's staves; a double bar at the end.
    for (size_t i = 0; i < lay.staves.size(); ++i)
    {
        const auto& st = lay.staves[i];
        if (st.staffInPart != 0) continue;
        const auto& bottomStaff = lay.staves[i + static_cast<size_t> (st.staffCount) - 1];
        const float y1 = origin.y + static_cast<float> (st.top) * sp;
        const float y2 = origin.y + static_cast<float> (bottomStaff.top + 4.0) * sp;
        g.setColour (page.ink);
        const float thin = std::max (1.0f, 0.16f * sp);
        g.fillRect (X (0) - thin * 0.5f, y1, thin, y2 - y1);
        for (size_t m = 1; m < lay.measures.size(); ++m)
            if (visible (lay.measures[m].x)) g.fillRect (X (lay.measures[m].x) - thin * 0.5f, y1, thin, y2 - y1);
        const float end = X (lay.width);
        g.fillRect (end - 0.5f * sp, y1, 0.5f * sp, y2 - y1);
        g.fillRect (end - 1.4f * sp, y1, thin, y2 - y1);

        // A brace for an instrument on two staves.
        if (st.staffCount > 1)
        {
            juce::Path brace = gl.path (smufl::brace);
            const auto b = brace.getBounds();
            if (b.getHeight() > 0)
            {
                const float h = y2 - y1;
                brace.applyTransform (juce::AffineTransform::translation (-b.getX(), -b.getY())
                                          .scaled (sp * 1.0f / std::max (0.01f, b.getWidth()) * 1.2f, h / b.getHeight())
                                          .translated (X (0) - 1.6f * sp, y1));
                g.fillPath (brace);
            }
        }
    }

    // Bar numbers.
    if (style.showBarNumbers && ! lay.staves.empty())
    {
        g.setColour (page.dim);
        g.setFont (juce::FontOptions (std::max (9.0f, sp * 1.3f)));
        const float y = origin.y + static_cast<float> (lay.staves.front().top) * sp - 3.2f * sp;
        for (const auto& m : lay.measures)
            if (visible (m.x))
                g.drawText (juce::String (m.bar + 1), juce::Rectangle<float> (X (m.x) + 0.2f * sp, y, 6 * sp, 1.6f * sp),
                            juce::Justification::centredLeft);
    }

    // What each measure shows at its start.
    for (const auto& st : lay.staves)
    {
        const float top = origin.y + static_cast<float> (st.top) * sp;
        for (const auto& m : lay.measures)
        {
            if (! visible (m.x)) continue;
            g.setColour (page.ink);
            if (m.bar == 0)
            {
                drawStaffStart (g, st, m, style, X (0), top, true);
                continue;
            }
            float x = static_cast<float> (m.x) + 0.8f;
            if (m.showKey)
            {
                drawKey (g, m.key, st.clef, X (x), top, sp);
                x += std::max (keyWidth (m.key), 1.0f) + 0.8f;
            }
            if (m.showTime) drawTime (g, m.meter, X (x), top, sp);
        }
    }

    // The music.
    for (const auto& st : lay.staves)
    {
        const float top = origin.y + static_cast<float> (st.top) * sp;
        if (top > clip.getBottom() + 10 * sp || top + 4 * sp < clip.getY() - 10 * sp) continue;
        auto Y = [&] (double y) { return top + static_cast<float> (y) * sp; };

        for (const auto& el : st.elements)
        {
            if (! visible (el.x)) continue;
            if (el.rest)
            {
                g.setColour (page.ink);
                gl.draw (g, restGlyph (el.den), X (el.x), Y (el.restY), sp);
                for (int d = 0; d < el.dots; ++d)
                    gl.draw (g, smufl::augmentationDot, X (el.x + gl.bounds (restGlyph (el.den)).getRight() + 0.3 + d * 0.5),
                             Y (el.restY - 0.5), sp);
                continue;
            }

            bool anySelected = false, anySounding = false;
            for (const auto& h : el.heads)
            {
                anySelected = anySelected || style.selected.count (h.noteId) != 0;
                anySounding = anySounding || style.sounding.count (h.noteId) != 0;
            }
            const auto stemColour = (anySelected || anySounding) ? page.accent : page.ink;
            const double w = el.den == 1 ? wholeHeadWidth : headWidth;

            for (const auto& h : el.heads)
            {
                juce::Colour c = page.ink;
                if (style.showWarnings && h.outOfRange) c = page.warn;
                else if (style.showWarnings && h.outsideSweet) c = page.dim;
                if (style.selected.count (h.noteId) != 0 || style.sounding.count (h.noteId) != 0) c = page.accent;

                // Ledger lines, a little wider than the head.
                g.setColour (page.ink);
                const float lx = X (h.x - ledgerExtension), lw = static_cast<float> (w + 2 * ledgerExtension) * sp;
                const float lt = std::max (1.0f, 0.16f * sp);
                for (int p = -2; p >= h.pos; p -= 2) g.fillRect (lx, Y (headY (p)) - lt * 0.5f, lw, lt);
                for (int p = 10; p <= h.pos; p += 2) g.fillRect (lx, Y (headY (p)) - lt * 0.5f, lw, lt);

                g.setColour (c);
                gl.draw (g, headGlyph (el.den, h.drumCross), X (h.x), Y (headY (h.pos)), sp);
                if (h.showAccidental)
                    gl.draw (g, accidentalGlyph (h.accidental), X (el.x + h.accidentalX), Y (headY (h.pos)), sp);
                // Dots sit in a space: a head on a line puts its dot in the space above.
                for (int d = 0; d < el.dots; ++d)
                {
                    double right = el.x + w;
                    for (const auto& o : el.heads) right = std::max (right, o.x + w);
                    const int dotPos = (h.pos % 2 == 0) ? (el.voice == 1 ? h.pos - 1 : h.pos + 1) : h.pos;
                    gl.draw (g, smufl::augmentationDot, X (right + 0.25 + d * 0.5), Y (headY (dotPos)), sp);
                }
            }

            g.setColour (stemColour);
            if (el.stem != Stem::none)
            {
                const float sx = X (el.stemX) - static_cast<float> (stemThickness) * sp * 0.5f;
                g.fillRect (sx, Y (el.stemTop), std::max (1.0f, static_cast<float> (stemThickness) * sp), Y (el.stemBottom) - Y (el.stemTop));
                if (el.beam < 0 && el.beamCount > 0)
                {
                    const auto n = static_cast<juce::juce_wchar> (std::min (el.beamCount, 4) - 1);
                    if (el.stem == Stem::up) gl.draw (g, smufl::flag8thUp + 2 * n, sx, Y (el.stemTop), sp);
                    else gl.draw (g, smufl::flag8thDown + 2 * n, sx, Y (el.stemBottom), sp);
                }
            }

            if (style.showWarnings && (el.tooManyNotes || el.tooFast || el.tooWide))
            {
                g.setColour (page.warn);
                const float cx = X (el.x + w * 0.5), cy = Y (-1.6);
                juce::Path tri;
                tri.addTriangle (cx - 0.45f * sp, cy, cx + 0.45f * sp, cy, cx, cy + 0.7f * sp);
                g.fillPath (tri);
            }
        }

        // Beams: the primary on the stems' ends, each further level a beam
        // and a gap nearer the heads.
        for (const auto& b : st.beams)
        {
            if (! visible (b.x1) && ! visible (b.x2)) continue;
            bool lit = false;
            for (int i : b.elements)
                for (const auto& h : st.elements[static_cast<size_t> (i)].heads)
                    lit = lit || style.selected.count (h.noteId) != 0 || style.sounding.count (h.noteId) != 0;
            g.setColour (lit ? page.accent : page.ink);
            const double dir = b.stem == Stem::up ? 1.0 : -1.0;
            auto slab = [&] (double xa, double xb, double offset)
            {
                const double t = stemThickness * 0.5;
                const double ya = b.yAt (xa) + offset, yb = b.yAt (xb) + offset;
                juce::Path p;
                p.startNewSubPath (X (xa - t), Y (ya));
                p.lineTo (X (xb + t), Y (yb));
                p.lineTo (X (xb + t), Y (yb + dir * beamThickness));
                p.lineTo (X (xa - t), Y (ya + dir * beamThickness));
                p.closeSubPath();
                g.fillPath (p);
            };
            slab (b.x1, b.x2, 0);
            for (const auto& s : b.segments) slab (s.xa, s.xb, dir * s.level * (beamThickness + beamSpacing));
        }

        g.setColour (page.ink);
        for (const auto& t : st.ties)
            if (visible (t.x1) || visible (t.x2))
                drawTie (g, X (t.x1), X (t.x2), Y (t.y), t.above, sp);

        for (const auto& t : st.tuplets)
        {
            if (! visible (t.x1)) continue;
            const float cx = (X (t.x1) + X (t.x2)) * 0.5f;
            const float y = Y (t.y);
            if (t.bracket)
            {
                const float lt = std::max (1.0f, 0.16f * sp);
                const float hook = (t.above ? 0.6f : -0.6f) * sp;
                g.fillRect (X (t.x1), y, cx - 0.9f * sp - X (t.x1), lt);
                g.fillRect (cx + 0.9f * sp, y, X (t.x2) - cx - 0.9f * sp, lt);
                g.fillRect (X (t.x1), std::min (y, y + hook), lt, std::abs (hook));
                g.fillRect (X (t.x2) - lt, std::min (y, y + hook), lt, std::abs (hook));
            }
            drawNumber (g, t.number, cx, y + 0.75f * sp, sp * 0.8f, smufl::tuplet0);
        }
    }
}

void ScoreRenderer::drawNames (juce::Graphics& g, const Layout& lay, const Score& score, const RenderStyle& style,
                               float right, float originY, bool shortNames)
{
    const float sp = style.space;
    g.setFont (juce::FontOptions (std::max (11.0f, sp * 1.6f)));
    for (size_t i = 0; i < lay.staves.size(); ++i)
    {
        const auto& st = lay.staves[i];
        if (st.staffInPart != 0) continue;
        const auto& part = score.parts[static_cast<size_t> (st.part)];
        const auto& inst = instrumentById (part.instrument);
        const auto& last = lay.staves[i + static_cast<size_t> (st.staffCount) - 1];
        const float y1 = originY + static_cast<float> (st.top) * sp;
        const float y2 = originY + static_cast<float> (last.top + 4.0) * sp;
        // A name too long for the margin is shortened the way a printed score
        // shortens it, keeping its number: Tenor Saxophone 2 is T. Sax. 2.
        const juce::String full (part.name);
        juce::String name = full;
        if (full.startsWith (inst.name))
        {
            const juce::String shortened = juce::String (inst.shortName) + full.substring (static_cast<int> (inst.name.size()));
            const float width = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), full);
            if (shortNames || width > right - 4.0f) name = shortened;
        }
        g.setColour (style.page.ink);
        g.drawFittedText (name, juce::Rectangle<float> (0, y1, right, y2 - y1).toNearestInt(),
                          juce::Justification::centredRight, 2);
    }
}

} // namespace nt
