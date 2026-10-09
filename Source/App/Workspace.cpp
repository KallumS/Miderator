#include "Workspace.h"

namespace nt
{

Workspace::Workspace (Controller& c) : timeline (c), arrange (c, timeline), roll (c, timeline), controller (c)
{
    addAndMakeVisible (arrange);
    addAndMakeVisible (divider);
    addAndMakeVisible (roll);
    addAndMakeVisible (hbar);
    hbar.setAutoHide (false);
    hbar.addListener (this);
    timeline.addChangeListener (this);
    controller.addChangeListener (this);
}

Workspace::~Workspace()
{
    timeline.removeChangeListener (this);
    controller.removeChangeListener (this);
}

void Workspace::paint (juce::Graphics& g)
{
    g.fillAll (theme::ground);
}

void Workspace::resized()
{
    auto r = getLocalBounds();
    auto bottom = r.removeFromBottom (Timeline::right);
    hbar.setBounds (bottom.withTrimmedLeft (Timeline::left).withTrimmedRight (Timeline::right));
    const int tracks = juce::roundToInt (static_cast<float> (r.getHeight()) * split);
    arrange.setBounds (r.removeFromTop (tracks));
    divider.setBounds (r.removeFromTop (7));
    roll.setBounds (r);
    timeline.viewWidth = std::max (100, getWidth() - Timeline::left - Timeline::right);
    updateScrollbar();
}

void Workspace::updateScrollbar()
{
    const double content = std::max (timeline.contentWidth(), static_cast<double> (timeline.viewWidth));
    hbar.setRangeLimits (0, content, juce::dontSendNotification);
    hbar.setCurrentRange (timeline.scrollX, timeline.viewWidth, juce::dontSendNotification);
}

void Workspace::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    // The score may have grown or shrunk, or the zoom changed: keep the
    // scroll inside what there is.
    if (source == &controller) timeline.scrollTo (timeline.scrollX);
    updateScrollbar();
}

void Workspace::scrollBarMoved (juce::ScrollBar*, double start)
{
    timeline.scrollTo (start);
}

void Workspace::Divider::paint (juce::Graphics& g)
{
    g.fillAll (theme::popup);
    g.setColour (theme::rule);
    g.fillRect (0, 0, getWidth(), 1);
    g.fillRect (0, getHeight() - 1, getWidth(), 1);
    g.setColour (theme::textDim);
    const float cx = static_cast<float> (getWidth()) * 0.5f;
    for (int i = -1; i <= 1; ++i) g.fillEllipse (cx + static_cast<float> (i) * 7.0f - 1.5f, static_cast<float> (getHeight()) * 0.5f - 1.5f, 3.0f, 3.0f);
}

void Workspace::Divider::mouseDrag (const juce::MouseEvent& e)
{
    const float h = static_cast<float> (std::max (1, owner.getHeight() - Timeline::right));
    owner.split = juce::jlimit (0.15f, 0.85f, from + static_cast<float> (e.getDistanceFromDragStartY()) / h);
    owner.resized();
}

} // namespace nt
