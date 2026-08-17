#include "ToolbarComponent.h"

ToolbarComponent::ToolbarComponent()
{
    addCueButton = Icons::makeButton("addCue", BinaryData::add_svg, BinaryData::add_svgSize,
                                     Palette::textPrimary, "Add audio cues");
    previewButton = Icons::makeButton("preview", BinaryData::play_arrow_svg, BinaryData::play_arrow_svgSize,
                                      Palette::textPrimary, "Preview selected cue (Cmd+P)");
    stopButton = Icons::makeButton("stop", BinaryData::stop_svg, BinaryData::stop_svgSize,
                                   Palette::textPrimary, "Stop selected cue");
    pauseButton = Icons::makeButton("pause", BinaryData::pause_svg, BinaryData::pause_svgSize,
                                    Palette::textPrimary, "Pause");
    panicButton = Icons::makeButton("panic", BinaryData::fiber_manual_record_svg,
                                    BinaryData::fiber_manual_record_svgSize, Palette::panicRed,
                                    "Panic: stop all cues");
    resetButton = Icons::makeButton("reset", BinaryData::undo_svg, BinaryData::undo_svgSize,
                                    Palette::textPrimary, "Reset standby to top");

    addCueButton.button->onClick = [this]
    {
        juce::PopupMenu menu;
        menu.addItem("Audio Cue...", [this] { if (onAddCue != nullptr) onAddCue(); });
        menu.addItem("Group Cue", [this] { if (onAddGroup != nullptr) onAddGroup(); });
        menu.addSeparator();
        for (const auto& type : { "Video", "MIDI", "Network", "Light", "Cart" })
            menu.addItem(juce::String(type) + " cue - not supported yet", false, false, [] {});
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(*addCueButton.button));
    };
    previewButton.button->onClick = [this] { if (onPreview != nullptr) onPreview(); };
    stopButton.button->onClick = [this] { if (onStop != nullptr) onStop(); };
    pauseButton.button->onClick = [this] { if (onPause != nullptr) onPause(); };
    panicButton.button->onClick = [this] { if (onPanic != nullptr) onPanic(); };
    resetButton.button->onClick = [this] { if (onReset != nullptr) onReset(); };

    for (auto* button : { addCueButton.button.get(), previewButton.button.get(),
                          stopButton.button.get(), pauseButton.button.get(),
                          panicButton.button.get(), resetButton.button.get() })
        addAndMakeVisible(*button);

    masterGainSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    masterGainSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    masterGainSlider.setRange(0.0, 1.0, 0.01);
    masterGainSlider.setValue(0.8);
    masterGainSlider.setTooltip("Master gain");
    masterGainSlider.setWantsKeyboardFocus(false);
    masterGainSlider.setMouseClickGrabsKeyboardFocus(false);
    masterGainSlider.setColour(juce::Slider::backgroundColourId, juce::Colours::transparentBlack);
    masterGainSlider.setColour(juce::Slider::trackColourId, juce::Colour(0xff777777));
    masterGainSlider.setColour(juce::Slider::thumbColourId, Palette::textPrimary);
    masterGainSlider.onValueChange = [this]
    {
        if (onMasterGain != nullptr)
            onMasterGain(static_cast<float>(masterGainSlider.getValue()));
    };
    addAndMakeVisible(masterGainSlider);
}

void ToolbarComponent::setEditingEnabled(bool enabled)
{
    addCueButton.button->setEnabled(enabled);
    resetButton.button->setEnabled(enabled);
}

void ToolbarComponent::paint(juce::Graphics& g)
{
    g.setColour(Palette::panelBg);
    for (const auto& rect : groupRects)
        g.fillRoundedRectangle(rect.toFloat(), 6.0f);

    g.setColour(Palette::textDim);
    g.setFont(juce::Font(juce::FontOptions().withHeight(9.5f)));
    const auto drawLabel = [&g](const juce::String& text, juce::Rectangle<int> bounds)
    {
        g.drawText(text, bounds.removeFromBottom(14), juce::Justification::centred, true);
    };

    drawLabel("Add", groupRects[0]);
    auto playback = groupRects[1];
    for (const auto& label : { "Preview", "Stop", "Pause", "Panic" })
        drawLabel(label, playback.removeFromLeft(44));
    drawLabel("Reset", groupRects[2]);
}

void ToolbarComponent::resized()
{
    auto r = getLocalBounds();

    groupRects[0] = r.removeFromLeft(44);
    addCueButton.button->setBounds(groupRects[0].withHeight(36).reduced(5, 3));
    r.removeFromLeft(8);

    groupRects[1] = r.removeFromLeft(4 * 44);
    auto g2 = groupRects[1];
    previewButton.button->setBounds(g2.removeFromLeft(44).withHeight(36).reduced(5, 3));
    stopButton.button->setBounds(g2.removeFromLeft(44).withHeight(36).reduced(5, 3));
    pauseButton.button->setBounds(g2.removeFromLeft(44).withHeight(36).reduced(5, 3));
    panicButton.button->setBounds(g2.removeFromLeft(44).withHeight(36).reduced(5, 3));
    r.removeFromLeft(8);

    groupRects[2] = r.removeFromLeft(44);
    resetButton.button->setBounds(groupRects[2].withHeight(36).reduced(5, 3));

    masterGainSlider.setBounds(r.removeFromRight(160).reduced(6, 4));
}
