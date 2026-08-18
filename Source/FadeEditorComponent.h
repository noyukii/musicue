#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Cue.h"
#include "Palette.h"

class FadeEditorComponent : public juce::Component,
                            private juce::Timer
{
public:
    enum class Mode { inspector, bulk };

    explicit FadeEditorComponent(Mode mode);
    ~FadeEditorComponent() override;

    void setAvailableCues(const juce::Array<Cue>& cues);
    void setSetup(const Cue::FadeSetup& setup);
    Cue::FadeSetup getSetup() const;

    int getMinimumHeight() const;

    std::function<void()> onChange;
    std::function<void(int cueId)> onIncomingSelected;
    std::function<double()> playheadProvider;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    class CurveVisualizer : public juce::Component
    {
    public:
        void setState(Cue::FadeCurve curve, bool hasOutgoing, bool hasIncoming);
        void setPlayhead(double progress);
        void paint(juce::Graphics& g) override;

    private:
        Cue::FadeCurve curve = Cue::FadeCurve::sCurve;
        bool hasOutgoing = false;
        bool hasIncoming = false;
        double playhead = -1.0;
    };

    void timerCallback() override;

    void styleLabel(juce::Label& label, const juce::String& text, bool centred = false);
    void styleEditor(juce::TextEditor& editor);
    void configureSlider(juce::Slider& slider, double minimum, double maximum, double interval,
                         const juce::String& suffix);
    void refreshTargets();
    void refreshFields();
    void applyFields(bool notifyIncoming = false);
    void notifyChange();
    juce::String nameForCue(int id) const;
    int comboIdForCue(int cueId) const;
    int cueIdFromCombo(int comboId) const;

    static constexpr int nothingId = 1;
    static constexpr int nextInLineId = 2;
    static constexpr int cueIdOffset = 1000;

    Mode mode = Mode::inspector;
    juce::Array<Cue> availableCues;
    Cue::FadeSetup currentSetup;
    bool updating = false;

    juce::Label mainLabel, otherLabel, mainValueLabel, autoStartLabel;
    juce::Label delayLabel, durationLabel, curveLabel, stopPolicyLabel, startGainLabel;
    juce::ComboBox mainBox, otherBox, curveBox, stopPolicyBox;
    juce::TextEditor delayEditor, durationEditor;
    juce::ToggleButton gainToggle, panToggle, stopToggle;
    juce::Slider targetGainSlider, targetPanSlider, startGainSlider;
    CurveVisualizer visualizer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FadeEditorComponent)
};
