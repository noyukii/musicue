#include "FadeEditorComponent.h"
#include <cmath>

namespace
{
    juce::String formatFadeTime(double seconds)
    {
        const auto mins = static_cast<int>(seconds / 60.0);
        const auto secs = seconds - static_cast<double>(mins) * 60.0;
        return juce::String::formatted("%02d:%06.3f", mins, secs);
    }

    double parseFadeTime(const juce::String& text)
    {
        const auto trimmed = text.trim();
        if (trimmed.containsChar(':'))
        {
            const auto mins = trimmed.upToFirstOccurrenceOf(":", false, false).getDoubleValue();
            const auto secs = trimmed.fromFirstOccurrenceOf(":", false, false).getDoubleValue();
            return mins * 60.0 + secs;
        }
        return trimmed.getDoubleValue();
    }
}

void FadeEditorComponent::CurveVisualizer::setState(Cue::FadeCurve newCurve, bool outgoing, bool incoming)
{
    curve = newCurve;
    hasOutgoing = outgoing;
    hasIncoming = incoming;
    repaint();
}

void FadeEditorComponent::CurveVisualizer::setPlayhead(double progress)
{
    const auto next = progress < 0.0 ? -1.0 : juce::jlimit(0.0, 1.0, progress);
    if (next < 0.0 && playhead < 0.0)
        return;
    if (next >= 0.0 && playhead >= 0.0 && std::abs(next - playhead) < 0.002)
        return;
    playhead = next;
    repaint();
}

void FadeEditorComponent::CurveVisualizer::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(Palette::fieldBg);
    g.fillRoundedRectangle(bounds, 6.0f);
    g.setColour(Palette::divider.brighter(0.25f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 6.0f, 1.0f);

    auto area = bounds.reduced(12.0f, 14.0f);
    if (area.getWidth() < 8.0f || area.getHeight() < 8.0f)
        return;

    g.setColour(Palette::divider.brighter(0.4f));
    g.drawLine(area.getX(), area.getCentreY(), area.getRight(), area.getCentreY(), 1.0f);
    g.drawLine(area.getX(), area.getY(), area.getX(), area.getBottom(), 1.0f);

    auto buildPath = [&](bool fadeIn)
    {
        juce::Path path;
        const int steps = 64;
        for (int i = 0; i <= steps; ++i)
        {
            const auto t = static_cast<float>(i) / static_cast<float>(steps);
            const auto curved = Cue::applyFadeCurve(curve, t);
            const auto x = area.getX() + area.getWidth() * t;
            const auto gain = fadeIn ? curved : 1.0f - curved;
            const auto y = area.getBottom() - area.getHeight() * gain;
            if (i == 0)
                path.startNewSubPath(x, y);
            else
                path.lineTo(x, y);
        }
        return path;
    };

    if (hasOutgoing)
    {
        auto path = buildPath(false);
        g.setColour(juce::Colour(0xffc47b5a).withAlpha(0.18f));
        auto fill = path;
        fill.lineTo(area.getRight(), area.getBottom());
        fill.lineTo(area.getX(), area.getBottom());
        fill.closeSubPath();
        g.fillPath(fill);
        g.setColour(juce::Colour(0xffe08a62));
        g.strokePath(path, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));
    }

    if (hasIncoming)
    {
        auto path = buildPath(true);
        g.setColour(Palette::standbyGreen.withAlpha(0.16f));
        auto fill = path;
        fill.lineTo(area.getRight(), area.getY());
        fill.lineTo(area.getX(), area.getY());
        fill.closeSubPath();
        g.fillPath(fill);
        g.setColour(Palette::standbyGreen);
        g.strokePath(path, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));
    }

    g.setFont(juce::Font(juce::FontOptions().withHeight(11.0f)));
    g.setColour(Palette::textDim);
    if (hasOutgoing)
        g.drawText("Out", juce::roundToInt(area.getX()), juce::roundToInt(area.getY()) - 2,
                   40, 14, juce::Justification::centredLeft);
    if (hasIncoming)
        g.drawText("In", juce::roundToInt(area.getRight()) - 40, juce::roundToInt(area.getY()) - 2,
                   40, 14, juce::Justification::centredRight);

    if (! hasOutgoing && ! hasIncoming)
        g.drawText("Select tracks to preview the fade", getLocalBounds(), juce::Justification::centred);

    if (playhead >= 0.0)
    {
        const auto x = area.getX() + area.getWidth() * static_cast<float>(playhead);
        g.setColour(juce::Colours::white.withAlpha(0.10f));
        g.fillRect(area.getX(), area.getY(), juce::jmax(0.0f, x - area.getX()), area.getHeight());
        g.setColour(juce::Colours::white.withAlpha(0.92f));
        g.drawLine(x, area.getY(), x, area.getBottom(), 1.6f);

        juce::Path marker;
        marker.addTriangle(x - 5.0f, area.getY(), x + 5.0f, area.getY(), x, area.getY() + 7.0f);
        g.fillPath(marker);
        marker.clear();
        marker.addTriangle(x - 5.0f, area.getBottom(), x + 5.0f, area.getBottom(), x, area.getBottom() - 7.0f);
        g.fillPath(marker);
    }
}

FadeEditorComponent::FadeEditorComponent(Mode editorMode)
    : mode(editorMode)
{
    styleLabel(mainLabel, "Main track", true);
    styleLabel(otherLabel, "Other track", true);
    styleLabel(mainValueLabel, "Each selected cue", true);
    mainValueLabel.setColour(juce::Label::textColourId, Palette::textPrimary);
    styleLabel(autoStartLabel, "", true);
    autoStartLabel.setFont(juce::Font(juce::FontOptions().withHeight(11.5f)));

    styleLabel(delayLabel, "Delay:");
    styleLabel(durationLabel, "Duration:");
    styleLabel(curveLabel, "Curve:");
    styleLabel(stopPolicyLabel, "Stop fade:");
    styleLabel(startGainLabel, "Start gain:");

    mainBox.setTextWhenNothingSelected("Choose audio cue");
    otherBox.setTextWhenNothingSelected("Nothing");
    mainBox.onChange = [this] { applyFields(); };
    otherBox.onChange = [this] { applyFields(true); };
    addAndMakeVisible(mainLabel);
    addAndMakeVisible(otherLabel);
    addAndMakeVisible(autoStartLabel);

    if (mode == Mode::bulk)
    {
        addAndMakeVisible(mainValueLabel);
        mainBox.setVisible(false);
    }
    else
    {
        addAndMakeVisible(mainBox);
        mainValueLabel.setVisible(false);
    }

    addAndMakeVisible(otherBox);

    styleEditor(delayEditor);
    styleEditor(durationEditor);
    for (auto* editor : { &delayEditor, &durationEditor })
    {
        editor->onReturnKey = [this, editor]
        {
            applyFields();
            editor->giveAwayKeyboardFocus();
        };
        editor->onFocusLost = [this] { applyFields(); };
        addAndMakeVisible(*editor);
    }

    for (int i = 0; i < Cue::numFadeCurves; ++i)
        curveBox.addItem(Cue::fadeCurveName(static_cast<Cue::FadeCurve>(i)), i + 1);
    curveBox.onChange = [this] { applyFields(); };
    stopPolicyBox.addItem("Hold targets", 1);
    stopPolicyBox.addItem("Stop targets", 2);
    stopPolicyBox.onChange = [this] { applyFields(); };
    addAndMakeVisible(delayLabel);
    addAndMakeVisible(durationLabel);
    addAndMakeVisible(curveLabel);
    addAndMakeVisible(stopPolicyLabel);
    addAndMakeVisible(curveBox);
    addAndMakeVisible(stopPolicyBox);
    addAndMakeVisible(visualizer);

    gainToggle.setButtonText("Fade gain");
    panToggle.setButtonText("Fade pan");
    stopToggle.setButtonText("Stop at end");
    for (auto* toggle : { &gainToggle, &panToggle, &stopToggle })
    {
        toggle->onClick = [this] { applyFields(); };
        toggle->setWantsKeyboardFocus(false);
        addAndMakeVisible(*toggle);
    }

    configureSlider(targetGainSlider, -60.0, 6.0, 0.1, " dB");
    configureSlider(targetPanSlider, -1.0, 1.0, 0.01, {});
    configureSlider(startGainSlider, -60.0, 6.0, 0.1, " dB");
    addAndMakeVisible(startGainLabel);

    refreshTargets();
    refreshFields();

    if (mode == Mode::inspector)
        startTimerHz(30);
}

FadeEditorComponent::~FadeEditorComponent()
{
    stopTimer();
}

void FadeEditorComponent::timerCallback()
{
    visualizer.setPlayhead(playheadProvider != nullptr ? playheadProvider() : -1.0);
}

void FadeEditorComponent::setAvailableCues(const juce::Array<Cue>& cues)
{
    availableCues = cues;
    refreshTargets();
}

void FadeEditorComponent::setSetup(const Cue::FadeSetup& setup)
{
    currentSetup = setup;
    refreshTargets();
    refreshFields();
}

Cue::FadeSetup FadeEditorComponent::getSetup() const
{
    return currentSetup;
}

int FadeEditorComponent::getMinimumHeight() const
{
    return 448;
}

void FadeEditorComponent::paint(juce::Graphics& g)
{
    g.fillAll(Palette::inspectorBg);
}

void FadeEditorComponent::resized()
{
    auto bounds = getLocalBounds().reduced(16);
    constexpr int rowHeight = 24;
    constexpr int rowGap = 8;
    constexpr int fieldLabelWidth = 78;

    auto placeLabel = [fieldLabelWidth](juce::Label& label, juce::Rectangle<int>& row, int width = 0)
    {
        label.setBounds(row.removeFromLeft(width > 0 ? width : fieldLabelWidth));
        row.removeFromLeft(6);
    };

    auto bottom = bounds.removeFromBottom(rowHeight * 4 + rowGap * 3);
    bounds.removeFromBottom(14);

    auto layoutSide = [](juce::Rectangle<int> area, juce::Label& title, juce::Component& control,
                         juce::Label* extra)
    {
        title.setBounds(area.removeFromTop(18));
        area.removeFromTop(8);
        control.setBounds(area.removeFromTop(28));
        if (extra != nullptr)
        {
            area.removeFromTop(8);
            extra->setBounds(area.removeFromTop(36));
        }
    };

    if (bounds.getWidth() >= 400)
    {
        const auto sideWidth = juce::jlimit(120, 180, bounds.getWidth() / 4);
        auto left = bounds.removeFromLeft(sideWidth);
        auto right = bounds.removeFromRight(sideWidth);
        bounds.removeFromLeft(10);
        bounds.removeFromRight(10);
        layoutSide(left, mainLabel, mode == Mode::bulk ? static_cast<juce::Component&>(mainValueLabel)
                                                       : static_cast<juce::Component&>(mainBox),
                   nullptr);
        layoutSide(right, otherLabel, otherBox, &autoStartLabel);
    }
    else
    {
        auto selectors = bounds.removeFromTop(70);
        auto left = selectors.removeFromLeft(selectors.getWidth() / 2);
        selectors.removeFromLeft(8);
        layoutSide(left, mainLabel, mode == Mode::bulk ? static_cast<juce::Component&>(mainValueLabel)
                                                       : static_cast<juce::Component&>(mainBox),
                   nullptr);
        layoutSide(selectors, otherLabel, otherBox, &autoStartLabel);
        bounds.removeFromTop(10);
    }

    auto placeField = [&](juce::Label& label, juce::Component& field)
    {
        auto row = bounds.removeFromTop(rowHeight);
        placeLabel(label, row);
        field.setBounds(row);
        bounds.removeFromTop(rowGap);
    };

    placeField(durationLabel, durationEditor);
    placeField(delayLabel, delayEditor);
    bounds.removeFromTop(4);
    visualizer.setBounds(bounds.removeFromTop(juce::jmax(90, bounds.getHeight() - (rowHeight + rowGap) * 2 - 8)));
    bounds.removeFromTop(10);
    placeField(curveLabel, curveBox);
    placeField(stopPolicyLabel, stopPolicyBox);

    auto placeToggleRow = [&](juce::ToggleButton& toggle, juce::Slider* slider)
    {
        auto row = bottom.removeFromTop(rowHeight);
        toggle.setBounds(row.removeFromLeft(170));
        row.removeFromLeft(12);
        if (slider != nullptr)
            slider->setBounds(row);
        bottom.removeFromTop(rowGap);
    };

    placeToggleRow(gainToggle, &targetGainSlider);
    placeToggleRow(panToggle, &targetPanSlider);
    placeToggleRow(stopToggle, nullptr);

    auto startRow = bottom.removeFromTop(rowHeight);
    placeLabel(startGainLabel, startRow, 78);
    startGainSlider.setBounds(startRow);
}

void FadeEditorComponent::styleLabel(juce::Label& label, const juce::String& text, bool centred)
{
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(centred ? juce::Justification::centred
                                       : juce::Justification::centredRight);
    label.setColour(juce::Label::textColourId, Palette::textDim);
    label.setFont(juce::Font(juce::FontOptions().withHeight(12.5f)));
}

void FadeEditorComponent::styleEditor(juce::TextEditor& editor)
{
    editor.setMultiLine(false);
    editor.setScrollbarsShown(false);
    editor.setColour(juce::TextEditor::backgroundColourId, Palette::fieldBg);
    editor.setColour(juce::TextEditor::outlineColourId, Palette::divider);
    editor.setColour(juce::TextEditor::focusedOutlineColourId, Palette::controlDown);
    editor.setColour(juce::TextEditor::textColourId, Palette::textPrimary);
    editor.setIndents(6, 0);
    editor.setFont(juce::Font(juce::FontOptions().withHeight(13.0f)));
    editor.setSelectAllWhenFocused(false);
}

void FadeEditorComponent::configureSlider(juce::Slider& slider, double minimum, double maximum,
                                          double interval, const juce::String& suffix)
{
    slider.setSliderStyle(juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 58, 22);
    slider.setRange(minimum, maximum, interval);
    slider.setTextValueSuffix(suffix);
    slider.setColour(juce::Slider::thumbColourId, Palette::textPrimary);
    slider.onValueChange = [this] { applyFields(); };
    addAndMakeVisible(slider);
}

int FadeEditorComponent::comboIdForCue(int cueId) const
{
    return cueId > 0 ? cueId + cueIdOffset : 0;
}

int FadeEditorComponent::cueIdFromCombo(int comboId) const
{
    return comboId >= cueIdOffset ? comboId - cueIdOffset : 0;
}

juce::String FadeEditorComponent::nameForCue(int id) const
{
    for (const auto& cue : availableCues)
        if (cue.id == id)
            return cue.number + " - " + cue.name;
    return "Missing target";
}

void FadeEditorComponent::refreshTargets()
{
    mainBox.clear(juce::dontSendNotification);
    otherBox.clear(juce::dontSendNotification);
    otherBox.addItem("Nothing", nothingId);
    if (mode == Mode::bulk)
        otherBox.addItem("Next in line", nextInLineId);

    for (const auto& cue : availableCues)
        if (cue.isAudio())
        {
            const auto itemId = comboIdForCue(cue.id);
            const auto label = cue.number + " - " + cue.name;
            mainBox.addItem(label, itemId);
            otherBox.addItem(label, itemId);
        }

    if (mode != Mode::bulk)
        mainBox.setSelectedId(comboIdForCue(currentSetup.fromCueId), juce::dontSendNotification);

    if (currentSetup.toNextInLine && mode == Mode::bulk)
        otherBox.setSelectedId(nextInLineId, juce::dontSendNotification);
    else if (currentSetup.toCueId > 0)
        otherBox.setSelectedId(comboIdForCue(currentSetup.toCueId), juce::dontSendNotification);
    else
        otherBox.setSelectedId(nothingId, juce::dontSendNotification);
}

void FadeEditorComponent::refreshFields()
{
    updating = true;
    delayEditor.setText(formatFadeTime(currentSetup.delaySeconds), false);
    durationEditor.setText(formatFadeTime(currentSetup.durationSeconds), false);
    curveBox.setSelectedId(static_cast<int>(currentSetup.curve) + 1, juce::dontSendNotification);
    stopPolicyBox.setSelectedId(static_cast<int>(currentSetup.stopPolicy) + 1, juce::dontSendNotification);
    gainToggle.setToggleState(currentSetup.fadeGain, juce::dontSendNotification);
    targetGainSlider.setValue(currentSetup.targetGainDb, juce::dontSendNotification);
    panToggle.setToggleState(currentSetup.fadePan, juce::dontSendNotification);
    targetPanSlider.setValue(currentSetup.targetPan, juce::dontSendNotification);
    stopToggle.setToggleState(currentSetup.stopAtEnd, juce::dontSendNotification);
    startGainSlider.setValue(currentSetup.startGainDb, juce::dontSendNotification);

    targetGainSlider.setEnabled(currentSetup.fadeGain);
    targetPanSlider.setEnabled(currentSetup.fadePan);

    if (mode != Mode::bulk)
        mainBox.setSelectedId(comboIdForCue(currentSetup.fromCueId), juce::dontSendNotification);

    if (currentSetup.toNextInLine && mode == Mode::bulk)
        otherBox.setSelectedId(nextInLineId, juce::dontSendNotification);
    else if (currentSetup.toCueId > 0)
        otherBox.setSelectedId(comboIdForCue(currentSetup.toCueId), juce::dontSendNotification);
    else
        otherBox.setSelectedId(nothingId, juce::dontSendNotification);

    const auto hasIncoming = currentSetup.toNextInLine || currentSetup.toCueId > 0;
    startGainLabel.setVisible(hasIncoming);
    startGainSlider.setVisible(hasIncoming);
    autoStartLabel.setText(hasIncoming ? "Starts playing automatically"
                                       : "Nothing selected",
                           juce::dontSendNotification);

    visualizer.setState(currentSetup.curve, true, hasIncoming);
    updating = false;
}

void FadeEditorComponent::applyFields(bool notifyIncoming)
{
    if (updating)
        return;

    const auto previousIncoming = currentSetup.toCueId;
    currentSetup.delaySeconds = juce::jmax(0.0, parseFadeTime(delayEditor.getText()));
    currentSetup.durationSeconds = juce::jmax(0.0, parseFadeTime(durationEditor.getText()));
    currentSetup.curve = Cue::fadeCurveFromInt(curveBox.getSelectedId() - 1);
    currentSetup.stopPolicy = static_cast<Cue::FadeStopPolicy>(juce::jlimit(
        0, 1, stopPolicyBox.getSelectedId() - 1));
    currentSetup.fadeGain = gainToggle.getToggleState();
    currentSetup.targetGainDb = targetGainSlider.getValue();
    currentSetup.fadePan = panToggle.getToggleState();
    currentSetup.targetPan = targetPanSlider.getValue();
    currentSetup.stopAtEnd = stopToggle.getToggleState();
    currentSetup.startGainDb = startGainSlider.getValue();

    if (mode != Mode::bulk)
        currentSetup.fromCueId = cueIdFromCombo(mainBox.getSelectedId());

    const auto otherId = otherBox.getSelectedId();
    currentSetup.toNextInLine = mode == Mode::bulk && otherId == nextInLineId;
    currentSetup.toCueId = currentSetup.toNextInLine ? 0 : cueIdFromCombo(otherId);
    if (currentSetup.toCueId == currentSetup.fromCueId && currentSetup.fromCueId > 0)
        currentSetup.toCueId = 0;

    refreshFields();
    notifyChange();

    if (notifyIncoming && onIncomingSelected != nullptr && currentSetup.toCueId > 0
        && currentSetup.toCueId != previousIncoming)
        onIncomingSelected(currentSetup.toCueId);
}

void FadeEditorComponent::notifyChange()
{
    if (onChange != nullptr)
        onChange();
}
