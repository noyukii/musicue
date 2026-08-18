#include "BulkFadeWindow.h"

BulkFadeWindow::BulkFadeWindow(const juce::Array<Cue>& cues, int primaryCueId, int selectedCount)
{
    setSize(700, 560);
    editor.setAvailableCues(cues);
    Cue::FadeSetup setup;
    setup.fromCueId = primaryCueId;
    editor.setSetup(setup);
    addAndMakeVisible(editor);

    const auto cueWord = selectedCount == 1 ? "cue" : "cues";
    summaryLabel.setText("Configure a fade and apply it to the selected " + juce::String(selectedCount)
                             + " " + cueWord + ".",
                         juce::dontSendNotification);
    summaryLabel.setJustificationType(juce::Justification::centredLeft);
    summaryLabel.setColour(juce::Label::textColourId, Palette::textDim);
    summaryLabel.setFont(juce::Font(juce::FontOptions().withHeight(13.0f)));
    addAndMakeVisible(summaryLabel);

    applyThisButton.setEnabled(primaryCueId > 0);
    applyAllButton.setEnabled(selectedCount > 0);
    applyThisButton.onClick = [this]
    {
        if (onApply != nullptr)
            onApply(false);
    };
    applyAllButton.onClick = [this]
    {
        if (onApply != nullptr)
            onApply(true);
    };
    cancelButton.onClick = [this]
    {
        if (auto* dialog = findParentComponentOfClass<juce::DialogWindow>())
            dialog->exitModalState(0);
    };

    addAndMakeVisible(applyThisButton);
    addAndMakeVisible(applyAllButton);
    addAndMakeVisible(cancelButton);
}

void BulkFadeWindow::paint(juce::Graphics& g)
{
    g.fillAll(Palette::inspectorBg);
}

void BulkFadeWindow::resized()
{
    auto bounds = getLocalBounds().reduced(8);
    auto footer = bounds.removeFromBottom(40);
    bounds.removeFromBottom(8);
    summaryLabel.setBounds(bounds.removeFromTop(22));
    bounds.removeFromTop(4);
    editor.setBounds(bounds);

    cancelButton.setBounds(footer.removeFromRight(96));
    footer.removeFromRight(8);
    applyAllButton.setBounds(footer.removeFromRight(168));
    footer.removeFromRight(8);
    applyThisButton.setBounds(footer.removeFromRight(150));
}
