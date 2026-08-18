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
    void setAvailableCues(const juce::Array<Cue>& cues);

    std::function<void()> onCueEdited;
    std::function<juce::String()> outputInfoProvider;

private:
    class BasicsTab;
    class TriggersTab;
    class IOTab;
    class TimeLoopsTab;
    class LevelsTab;
    class TrimTab;
    class ModeTab;
    class FadeTab;

    void rebuildTabs(bool groupSelected, bool fadeSelected);

    std::unique_ptr<BasicsTab> basicsTab;
    std::unique_ptr<TriggersTab> triggersTab;
    std::unique_ptr<IOTab> ioTab;
    std::unique_ptr<TimeLoopsTab> timeLoopsTab;
    std::unique_ptr<LevelsTab> levelsTab;
    std::unique_ptr<TrimTab> trimTab;
    std::unique_ptr<ModeTab> modeTab;
    std::unique_ptr<FadeTab> fadeTab;

    juce::TabbedComponent tabs;
    juce::Label emptyLabel;
    bool showingGroupTabs = false;
    bool showingFadeTabs = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InspectorComponent)
};
