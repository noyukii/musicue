#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "FadeEditorComponent.h"

class BulkFadeWindow : public juce::Component
{
public:
    BulkFadeWindow(const juce::Array<Cue>& cues, int primaryCueId, int selectedCount);

    Cue::FadeSetup getSetup() const { return editor.getSetup(); }

    std::function<void(bool applyAll)> onApply;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    FadeEditorComponent editor { FadeEditorComponent::Mode::bulk };
    juce::Label summaryLabel;
    juce::TextButton applyThisButton { "Apply to this cue" };
    juce::TextButton applyAllButton { "Apply to all selected" };
    juce::TextButton cancelButton { "Cancel" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BulkFadeWindow)
};
