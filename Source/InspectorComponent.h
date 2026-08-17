#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Palette.h"
#include "Cue.h"

class InspectorComponent : public juce::Component
{
public:
    InspectorComponent();
    ~InspectorComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void setCue(Cue* cue);

    std::function<void()> onCueEdited;
    std::function<juce::String()> outputInfoProvider;

private:
    class BasicsTab;
    class TriggersTab;
    class IOTab;
    class TimeLoopsTab;
    class LevelsTab;
    class TrimTab;

    BasicsTab* basicsTab = nullptr;
    TriggersTab* triggersTab = nullptr;
    IOTab* ioTab = nullptr;
    TimeLoopsTab* timeLoopsTab = nullptr;
    LevelsTab* levelsTab = nullptr;
    TrimTab* trimTab = nullptr;

    juce::TabbedComponent tabs;
    juce::Label emptyLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InspectorComponent)
};
