/*
    SettingsList - one generator's settings as a column of menus.

    Drawn from what the generator's adapter says it has, so nothing here knows
    what any engine can do: a setting added to an engine shows up by itself.
    Used by the Generate tab and the Starting Blocks toolbox.
*/

#pragma once

#include "Controller.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <set>

namespace nt
{

class SettingsList : public juce::Component
{
public:
    explicit SettingsList (Controller& c);

    // Lists `generatorId`'s settings as they stand, leaving out any whose id
    // or label is in `skip`.
    void show (const std::string& generatorId, const std::set<std::string>& skip);
    void refresh() { show (current, skipped); }
    int count() const { return static_cast<int> (boxes.size()); }
    int contentHeight() const { return count() * 30 + 4; }

    // The context a change is made in, and what to do after one.
    std::function<GeneratorContext()> context;
    std::function<void()> onChange;

    void resized() override;

private:
    Controller& controller;
    juce::Viewport view;
    juce::Component holder;
    std::vector<std::unique_ptr<juce::Label>> labels;
    std::vector<std::unique_ptr<juce::ComboBox>> boxes;
    std::string current;
    std::set<std::string> skipped;
};

} // namespace nt
