#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <BinaryData.h>
#include "Palette.h"
#include "Icons.h"
#include "Shortcuts.h"

class ToolbarComponent : public juce::Component
{
public:
    ToolbarComponent();

    void paint(juce::Graphics& g) override;
    void resized() override;

    std::function<void()> onAddCue, onAddGroup, onAddFade, onAddBulkFade, onPreview, onStop, onPause, onPanic, onReset;
    std::function<void(float)> onMasterGain;

    float getMasterGain() const;
    float getAppliedMasterGain() const;
    void setMasterGain(float gain);
    void setEditingEnabled(bool enabled);
    void updateTooltips(const ShortcutBindings& shortcuts);

private:
    void notifyMasterGain();
    void updateValueLabel();
    void setMuted(bool shouldMute);
    void updateMuteAppearance();

    Icons::IconButton addCueButton, previewButton, stopButton, pauseButton, panicButton, resetButton, muteButton;
    std::unique_ptr<juce::Drawable> mutedNormal, mutedOver;
    juce::Slider masterGainSlider;
    juce::Label valueLabel;
    bool muted = false;
    juce::Rectangle<int> groupRects[4];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ToolbarComponent)
};
