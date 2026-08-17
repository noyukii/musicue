#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <BinaryData.h>
#include "Palette.h"
#include "Icons.h"

class ToolbarComponent : public juce::Component
{
public:
    ToolbarComponent();

    void paint(juce::Graphics& g) override;
    void resized() override;

    std::function<void()> onAddCue, onAddGroup, onPreview, onStop, onPause, onPanic, onReset;
    std::function<void(float)> onMasterGain;

    float getMasterGain() const { return static_cast<float>(masterGainSlider.getValue()); }
    void setMasterGain(float gain) { masterGainSlider.setValue(gain, juce::dontSendNotification); }
    void setEditingEnabled(bool enabled);

private:
    Icons::IconButton addCueButton, previewButton, stopButton, pauseButton, panicButton, resetButton;
    juce::Slider masterGainSlider;
    juce::Rectangle<int> groupRects[3];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ToolbarComponent)
};
