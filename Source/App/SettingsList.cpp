#include "SettingsList.h"

#include "Theme.h"

namespace nt
{

SettingsList::SettingsList (Controller& c) : controller (c)
{
    addAndMakeVisible (view);
    view.setViewedComponent (&holder, false);
    view.setScrollBarsShown (true, false);
}

void SettingsList::show (const std::string& generatorId, const std::set<std::string>& skip)
{
    current = generatorId;
    skipped = skip;
    const auto ctx = context ? context() : GeneratorContext {};
    const auto settings = controller.lua.settings (current, ctx);

    labels.clear();
    boxes.clear();
    for (const auto& s : settings)
    {
        if (skip.count (s.id) != 0 || skip.count (s.label) != 0) continue;
        auto label = std::make_unique<juce::Label> (s.id, s.label);
        label->setFont (juce::FontOptions (13.0f));
        label->setColour (juce::Label::textColourId, theme::stepNumber);
        auto box = std::make_unique<juce::ComboBox> (s.id);
        for (size_t i = 0; i < s.names.size(); ++i)
            box->addItem (s.names[i], static_cast<int> (i) + 1);
        box->setSelectedId (s.index + 1, juce::dontSendNotification);
        juce::String tip = s.hint;
        if (s.index >= 0 && s.index < static_cast<int> (s.hints.size()) && ! s.hints[static_cast<size_t> (s.index)].empty())
            tip = (tip.isEmpty() ? juce::String() : tip + "\n") + s.hints[static_cast<size_t> (s.index)];
        box->setTooltip (tip);
        label->setTooltip (tip);
        const std::string settingId = s.id;
        auto* raw = box.get();
        box->onChange = [this, settingId, raw]
        {
            controller.lua.set (current, settingId, raw->getSelectedId() - 1, context ? context() : GeneratorContext {});
            // Showing or hiding one setting can depend on another; the menu
            // being changed cannot be deleted from inside its own callback.
            juce::Component::SafePointer<SettingsList> safe (this);
            juce::MessageManager::callAsync ([safe]
            {
                if (safe == nullptr) return;
                safe->refresh();
                if (safe->onChange) safe->onChange();
            });
        };
        holder.addAndMakeVisible (*label);
        holder.addAndMakeVisible (*box);
        labels.push_back (std::move (label));
        boxes.push_back (std::move (box));
    }
    resized();
}

void SettingsList::resized()
{
    view.setBounds (getLocalBounds());
    const int w = view.getWidth() - view.getScrollBarThickness() - 2;
    int y = 2;
    for (size_t i = 0; i < boxes.size(); ++i)
    {
        labels[i]->setBounds (0, y, w * 2 / 5, 26);
        boxes[i]->setBounds (w * 2 / 5, y, w - w * 2 / 5, 26);
        y += 30;
    }
    holder.setSize (w, y);
}

} // namespace nt
