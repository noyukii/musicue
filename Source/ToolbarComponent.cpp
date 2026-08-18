#include "ToolbarComponent.h"
#include <cmath>
#include <juce_audio_basics/juce_audio_basics.h>

namespace
{
    constexpr float masterMinusInfinityDb = -60.0f;

    juce::String formatMasterDb(double db)
    {
        if (db <= masterMinusInfinityDb + 0.05)
            return juce::String::fromUTF8("-\xe2\x88\x9e");

        if (std::abs(db) < 0.05)
            return "0.0";

        return juce::String(db, 1);
    }
}

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
    muteButton = Icons::makeButton("masterMute", BinaryData::volume_up_svg, BinaryData::volume_up_svgSize,
                                   Palette::textPrimary, "Mute master");
    mutedNormal = Icons::load(BinaryData::volume_off_svg, BinaryData::volume_off_svgSize, Palette::textDim);
    mutedOver = Icons::load(BinaryData::volume_off_svg, BinaryData::volume_off_svgSize,
                            Palette::textDim.brighter(0.3f));

    addCueButton.button->onClick = [this]
    {
        juce::PopupMenu menu;
        menu.addItem("Audio Cue...", [this] { if (onAddCue != nullptr) onAddCue(); });
        menu.addItem("Group Cue", [this] { if (onAddGroup != nullptr) onAddGroup(); });
        menu.addItem("Fade Cue", [this] { if (onAddFade != nullptr) onAddFade(); });
        menu.addItem("Fade Selected...", [this] { if (onAddBulkFade != nullptr) onAddBulkFade(); });
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
    muteButton.button->onClick = [this] { setMuted(! muted); };

    for (auto* button : { addCueButton.button.get(), previewButton.button.get(),
                          stopButton.button.get(), pauseButton.button.get(),
                          panicButton.button.get(), resetButton.button.get(),
                          muteButton.button.get() })
        addAndMakeVisible(*button);

    masterGainSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    masterGainSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    masterGainSlider.setRange(masterMinusInfinityDb, 0.0, 0.1);
    masterGainSlider.setValue(juce::Decibels::gainToDecibels(0.8f, masterMinusInfinityDb));
    masterGainSlider.setDoubleClickReturnValue(true, 0.0);
    masterGainSlider.setSliderSnapsToMousePosition(false);
    masterGainSlider.setTooltip("Master level. Drag to change, double-click for 0 dB.");
    masterGainSlider.setWantsKeyboardFocus(false);
    masterGainSlider.setMouseClickGrabsKeyboardFocus(false);
    masterGainSlider.setColour(juce::Slider::backgroundColourId, Palette::fieldBg);
    masterGainSlider.setColour(juce::Slider::trackColourId, Palette::textDim.brighter(0.12f));
    masterGainSlider.setColour(juce::Slider::thumbColourId, Palette::textPrimary);
    masterGainSlider.onValueChange = [this]
    {
        updateValueLabel();
        notifyMasterGain();
    };
    addAndMakeVisible(masterGainSlider);

    valueLabel.setFont(juce::Font(juce::FontOptions().withHeight(12.5f)));
    valueLabel.setColour(juce::Label::textColourId, Palette::textPrimary);
    valueLabel.setJustificationType(juce::Justification::centredRight);
    valueLabel.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(valueLabel);
    updateValueLabel();
}

float ToolbarComponent::getMasterGain() const
{
    return juce::Decibels::decibelsToGain(static_cast<float>(masterGainSlider.getValue()),
                                         masterMinusInfinityDb);
}

float ToolbarComponent::getAppliedMasterGain() const
{
    return muted ? 0.0f : getMasterGain();
}

void ToolbarComponent::setMasterGain(float gain)
{
    const auto db = juce::jlimit(static_cast<double>(masterMinusInfinityDb), 0.0,
                                 static_cast<double>(juce::Decibels::gainToDecibels(gain, masterMinusInfinityDb)));
    masterGainSlider.setValue(db, juce::dontSendNotification);
    updateValueLabel();
}

void ToolbarComponent::setEditingEnabled(bool enabled)
{
    addCueButton.button->setEnabled(enabled);
    resetButton.button->setEnabled(enabled);
}

void ToolbarComponent::updateTooltips(const ShortcutBindings& shortcuts)
{
    previewButton.button->setTooltip("Preview selected cue" + shortcuts.tooltipSuffix(ShortcutId::preview));
    stopButton.button->setTooltip("Stop selected cue" + shortcuts.tooltipSuffix(ShortcutId::stopSelected));
    pauseButton.button->setTooltip("Pause" + shortcuts.tooltipSuffix(ShortcutId::pause));
    panicButton.button->setTooltip("Panic: stop all cues" + shortcuts.tooltipSuffix(ShortcutId::panic));
    resetButton.button->setTooltip("Reset standby to top" + shortcuts.tooltipSuffix(ShortcutId::resetStandby));
}

void ToolbarComponent::notifyMasterGain()
{
    if (onMasterGain != nullptr)
        onMasterGain(getAppliedMasterGain());
}

void ToolbarComponent::updateValueLabel()
{
    valueLabel.setText(formatMasterDb(masterGainSlider.getValue()), juce::dontSendNotification);
}

void ToolbarComponent::setMuted(bool shouldMute)
{
    if (muted == shouldMute)
        return;

    muted = shouldMute;
    updateMuteAppearance();
    notifyMasterGain();
}

void ToolbarComponent::updateMuteAppearance()
{
    if (muted)
        muteButton.button->setImages(mutedNormal.get(), mutedOver.get(), mutedOver.get());
    else
        muteButton.button->setImages(muteButton.normal.get(), muteButton.over.get(), muteButton.over.get());

    muteButton.button->setTooltip(muted ? "Unmute master" : "Mute master");
    valueLabel.setColour(juce::Label::textColourId, muted ? Palette::textDim : Palette::textPrimary);
    masterGainSlider.setColour(juce::Slider::trackColourId,
                               Palette::textDim.withMultipliedAlpha(muted ? 0.45f : 1.0f).brighter(muted ? 0.0f : 0.12f));
    masterGainSlider.setColour(juce::Slider::thumbColourId,
                               muted ? Palette::textDim : Palette::textPrimary);
    masterGainSlider.repaint();
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
    drawLabel("Master", groupRects[3]);
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

    r.removeFromLeft(8);

    groupRects[3] = r.removeFromRight(232);
    auto master = groupRects[3].withHeight(36);
    muteButton.button->setBounds(master.removeFromLeft(40).reduced(5, 3));
    valueLabel.setBounds(master.removeFromRight(44).reduced(0, 6));
    masterGainSlider.setBounds(master.reduced(2, 8));
}
