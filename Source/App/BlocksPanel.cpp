#include "BlocksPanel.h"

#include "ScaleModel.h"
#include "ScoreRenderer.h"
#include "Theme.h"

namespace nt
{

namespace
{
const std::string blocksId = "starting-blocks";

// The engine's block kinds, and what the buttons call them.
struct Kind { const char* engineName; const char* shown; const char* tip; };
const Kind kindList[] = {
    { "Chord", "Chord", "The chord on each degree, struck or held" },
    { "Arpeggio", "Arpeggio", "The chord on each degree, one note at a time" },
    { "Run", "Run", "The scale from each degree, up, down or both" },
    { "Melody", "Interval", "A step or a leap from each degree: a 2nd, a 3rd, up to an octave" },
};

// The block placed alone, as it would go into the caret's part: what the
// preview draws and what is heard.
Score scoreOfBlock (const Controller& c, const GeneratedResult& r)
{
    Score s;
    s.tempos = c.score.tempos;
    const int bar = c.score.barAt (c.caret);
    s.meters = { c.score.meterAtBar (bar) };
    s.meters.front().bar = 0;
    s.keys = { c.score.keyAtBar (bar) };
    s.keys.front().bar = 0;
    Part p;
    p.id = s.newId();
    const auto* cp = c.caretPartPtr();
    p.instrument = cp != nullptr ? cp->instrument : std::string ("pno");
    p.name = instrumentById (p.instrument).name;
    s.parts.push_back (p);
    insertResult (s, r, p.id, 0);
    // A chord the caret's instrument cannot play went to a part of its own:
    // show that one alone.
    if (s.parts.size() > 1 && s.parts.front().notes.empty()) s.parts.erase (s.parts.begin());
    const Tick barLength = s.barStart (1);
    s.bars = std::max (1, static_cast<int> ((r.length + barLength - 1) / std::max<Tick> (1, barLength)));
    s.normalise();
    return s;
}
} // namespace

//==============================================================================

class BlocksPanel::DegreeButton : public juce::Button
{
public:
    DegreeButton() : juce::Button ("degree") {}
    juce::String numeral, name;
    bool chosen = false;
    std::function<void()> onDoubleClick;

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (chosen ? theme::accent : over ? theme::controlHover : theme::control);
        if (down) g.setColour (theme::accent.darker (0.1f));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (theme::ink);
        g.setFont (juce::FontOptions (17.0f, juce::Font::bold));
        g.drawText (numeral, r.removeFromTop (r.getHeight() * 0.55f), juce::Justification::centredBottom, false);
        g.setFont (juce::FontOptions (12.0f));
        g.drawText (name, r, juce::Justification::centredTop, true);
    }

    void mouseDoubleClick (const juce::MouseEvent&) override { if (onDoubleClick) onDoubleClick(); }
};

class BlocksPanel::Preview : public juce::Component
{
public:
    explicit Preview (Controller& c) : controller (c) {}
    Score score;
    engrave::Layout layout;

    void set (const Score& s)
    {
        score = s;
        layout = engrave::layout (score);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto page = controller.lightPage ? theme::lightPage() : theme::darkPage();
        g.setColour (page.paper);
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);
        if (layout.staves.empty()) return;
        // As large as fits, up to the page's own size.
        const float byHeight = static_cast<float> (getHeight() - 8) / static_cast<float> (layout.height + 5.0);
        const float byWidth = static_cast<float> (getWidth() - 12) / static_cast<float> (layout.width + 2.0);
        RenderStyle style;
        style.space = std::clamp (std::min (byHeight, byWidth), 3.0f, 8.0f);
        style.page = page;
        style.showWarnings = false;
        style.showBarNumbers = false;
        const float h = static_cast<float> (layout.height) * style.space;
        const juce::Point<float> origin (6.0f, (static_cast<float> (getHeight()) - h) * 0.5f);
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (getLocalBounds().reduced (2));
        ScoreRenderer::draw (g, layout, score, style, origin, getLocalBounds().toFloat());
    }

private:
    Controller& controller;
};

//==============================================================================

BlocksPanel::BlocksPanel (Controller& c) : controller (c)
{
    preview = std::make_unique<Preview> (controller);
    for (auto* comp : std::initializer_list<juce::Component*> { &key, &settings, preview.get(), &playButton, &insertButton, &target })
        addAndMakeVisible (comp);

    key.setFont (juce::FontOptions (13.0f));
    key.setColour (juce::Label::textColourId, theme::textDim);
    target.setFont (juce::FontOptions (13.0f));
    target.setColour (juce::Label::textColourId, theme::text);

    for (size_t i = 0; i < std::size (kindList); ++i)
    {
        auto* b = kinds.add (new juce::TextButton (kindList[i].shown));
        b->setTooltip (kindList[i].tip);
        b->setClickingTogglesState (false);
        b->onClick = [this, i] { setKind (static_cast<int> (i)); };
        addAndMakeVisible (b);
    }

    settings.context = [this] { return context(); };
    settings.onChange = [this] { remake(); resized(); };

    playButton.setTooltip ("Hear the chosen block again");
    insertButton.setTooltip ("Put the chosen block at the caret, and move the caret past it");
    playButton.onClick = [this] { choose (chosen, true); };
    insertButton.onClick = [this] { insertChosen(); };

    controller.addChangeListener (this);
    setKind (0);
}

BlocksPanel::~BlocksPanel() { controller.removeChangeListener (this); }

GeneratorContext BlocksPanel::context() const { return controller.generatorContext (false); }

void BlocksPanel::paint (juce::Graphics& g) { g.fillAll (theme::ground); }

void BlocksPanel::setKind (int index)
{
    for (int i = 0; i < kinds.size(); ++i)
    {
        kinds[i]->setToggleState (i == index, juce::dontSendNotification);
        kinds[i]->setColour (juce::TextButton::buttonColourId, i == index ? theme::accent : theme::control);
    }
    // The engine's own list of kinds is the menu this replaces.
    const auto ctx = context();
    for (const auto& s : controller.lua.settings (blocksId, ctx))
        if (s.id == "cat")
            for (size_t i = 0; i < s.names.size(); ++i)
                if (s.names[i] == kindList[index].engineName) controller.lua.set (blocksId, "cat", static_cast<int> (i), ctx);
    settings.show (blocksId, { "cat" });
    remake();
    resized();
}

void BlocksPanel::remake()
{
    const auto ctx = context();
    keyRoot = ctx.root;
    keyScale = ctx.scale;
    controller.lua.useKey (blocksId, ctx.root, ctx.scale);
    const auto out = controller.lua.generate (blocksId, ctx, 1, 0);
    blocks = out.results;

    const auto& root = scaleview::roots[static_cast<size_t> (std::clamp (ctx.root, 0, static_cast<int> (scaleview::roots.size()) - 1))];
    const auto& scale = scaleview::scales[static_cast<size_t> (std::clamp (ctx.scale, 0, static_cast<int> (scaleview::scales.size()) - 1))];
    key.setText (juce::String ("In ") + root.name + " " + juce::String (scale.name).toLowerCase() + ", the score's key at the caret",
                 juce::dontSendNotification);

    degrees.clear();
    for (size_t i = 0; i < blocks.size(); ++i)
    {
        const juce::String title (blocks[i].title);
        auto* b = degrees.add (new DegreeButton());
        // "ii  D Minor I Chord Triad": the numeral, then the note it stands on.
        b->numeral = title.upToFirstOccurrenceOf ("  ", false, false);
        b->name = juce::String (blocks[i].detail).upToFirstOccurrenceOf ("  ", false, false);
        b->setTooltip (title + "\n" + juce::String (blocks[i].detail).fromFirstOccurrenceOf ("  ", false, false));
        b->onClick = [this, i] { choose (static_cast<int> (i), true); };
        b->onDoubleClick = [this, i] { choose (static_cast<int> (i), false); insertChosen(); };
        addAndMakeVisible (b);
    }
    if (! out.error.empty()) key.setText ("Starting Blocks failed - this is a bug: " + juce::String (out.error), juce::dontSendNotification);
    choose (std::clamp (chosen, 0, std::max (0, static_cast<int> (blocks.size()) - 1)), false);
}

void BlocksPanel::choose (int index, bool play)
{
    chosen = index;
    for (int i = 0; i < degrees.size(); ++i)
    {
        degrees[i]->chosen = i == index;
        degrees[i]->repaint();
    }
    insertButton.setEnabled (index >= 0 && index < static_cast<int> (blocks.size()));
    if (index < 0 || index >= static_cast<int> (blocks.size())) { preview->set ({}); return; }
    preview->set (scoreOfBlock (controller, blocks[static_cast<size_t> (index)]));
    if (play)
    {
        controller.audio.play (preview->score, 0);
        controller.auditioning = true;
    }
}

void BlocksPanel::insertChosen()
{
    if (chosen < 0 || chosen >= static_cast<int> (blocks.size())) return;
    controller.stop();
    controller.insertGenerated (blocks[static_cast<size_t> (chosen)], false, blocksId);
}

void BlocksPanel::resized()
{
    auto r = getLocalBounds().reduced (10);
    key.setBounds (r.removeFromTop (20));
    r.removeFromTop (6);
    // The kinds, in one row.
    {
        auto line = r.removeFromTop (28);
        const int n = std::max (1, kinds.size());
        const int w = (line.getWidth() - 4 * (n - 1)) / n;
        for (auto* k : kinds)
        {
            k->setBounds (line.removeFromLeft (w));
            line.removeFromLeft (4);
        }
    }
    r.removeFromTop (10);

    // The degrees, four to a row.
    const int perRow = 4, gap = 4;
    const int bw = (r.getWidth() - gap * (perRow - 1)) / perRow;
    const int rows = (degrees.size() + perRow - 1) / perRow;
    auto grid = r.removeFromTop (rows * 50);
    for (int i = 0; i < degrees.size(); ++i)
        degrees[i]->setBounds (grid.getX() + (i % perRow) * (bw + gap), grid.getY() + (i / perRow) * 50, bw, 46);
    r.removeFromTop (8);

    auto bottom = r.removeFromBottom (30);
    const int half = (bottom.getWidth() - 4) / 2;
    playButton.setBounds (bottom.removeFromLeft (half));
    bottom.removeFromLeft (4);
    insertButton.setBounds (bottom);
    r.removeFromBottom (4);
    target.setBounds (r.removeFromBottom (20));
    r.removeFromBottom (6);
    preview->setBounds (r.removeFromBottom (std::min (130, std::max (70, r.getHeight() / 2))));
    r.removeFromBottom (8);
    settings.setBounds (r);
}

void BlocksPanel::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // A new key at the caret makes new blocks.
    const auto ctx = context();
    if (ctx.root != keyRoot || ctx.scale != keyScale) remake();
    juce::String into = "Into: ";
    if (controller.range.active()) into += controller.rangeText();
    else if (const auto* p = controller.caretPartPtr())
    {
        const Tick at = controller.caret;
        const int bar = controller.score.barAt (at);
        const double beat = static_cast<double> (at - controller.score.barStart (bar)) / PPQ + 1.0;
        into += juce::String (p->name) + ", bar " + juce::String (bar + 1) + " beat " + juce::String (beat, 2).trimCharactersAtEnd ("0").trimCharactersAtEnd (".");
    }
    target.setText (into, juce::dontSendNotification);
    preview->repaint();
}

} // namespace nt
