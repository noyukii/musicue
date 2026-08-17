#include "WorkspaceLauncherComponent.h"
#include <BinaryData.h>

namespace
{
constexpr int leftRailWidth = 354;
constexpr int tabHeight = 48;
constexpr int footerHeight = 54;
constexpr int inset = 26;

void setLabel(juce::Label& label, const juce::String& text, float size, juce::Colour colour)
{
    label.setText(text, juce::dontSendNotification);
    label.setFont(juce::Font(juce::FontOptions().withHeight(size)));
    label.setColour(juce::Label::textColourId, colour);
    label.setJustificationType(juce::Justification::centredLeft);
}
}

WorkspaceLauncherComponent::RecentWorkspaceButton::RecentWorkspaceButton(juce::File workspaceFile)
    : Button(workspaceFile.getFileNameWithoutExtension()), file(std::move(workspaceFile))
{
    setTooltip(file.getFullPathName());
    setWantsKeyboardFocus(false);
}

void WorkspaceLauncherComponent::RecentWorkspaceButton::paintButton(juce::Graphics& g,
                                                                     bool highlighted,
                                                                     bool down)
{
    auto bounds = getLocalBounds().toFloat().reduced(0.5f);
    auto fill = highlighted ? Palette::controlDown : Palette::controlBg;
    if (down)
        fill = fill.brighter(0.1f);
    g.setColour(fill);
    g.fillRoundedRectangle(bounds, 7.0f);

    const auto textWidth = getWidth() - 48;
    g.setColour(Palette::textPrimary);
    g.setFont(juce::Font(juce::FontOptions().withHeight(14.0f)));
    g.drawFittedText(file.getFileNameWithoutExtension(), 14, 7, textWidth, 22,
                     juce::Justification::centredLeft, 1);
    g.setColour(Palette::textDim);
    g.setFont(juce::Font(juce::FontOptions().withHeight(11.5f)));
    g.drawFittedText(file.getParentDirectory().getFullPathName(), 14, 29, textWidth, 18,
                     juce::Justification::centredLeft, 1);

    juce::Path chevron;
    const auto x = static_cast<float>(getWidth() - 20);
    const auto y = static_cast<float>(getHeight()) * 0.5f;
    chevron.startNewSubPath(x - 3.0f, y - 5.0f);
    chevron.lineTo(x + 2.0f, y);
    chevron.lineTo(x - 3.0f, y + 5.0f);
    g.setColour(Palette::textDim);
    g.strokePath(chevron, juce::PathStrokeType(1.5f));
}

WorkspaceLauncherComponent::WorkspaceLauncherComponent(juce::PropertiesFile& p) : properties(p)
{
    appIcon = juce::ImageFileFormat::loadFrom(BinaryData::appicon_musicue_png,
                                               BinaryData::appicon_musicue_pngSize);
    setLabel(title, "MusiCue", 28.0f, Palette::textPrimary);
    setLabel(subtitle, "Audio show control", 14.0f, Palette::textDim);
    setLabel(openDetail, "Open a saved .musicue workspace", 12.5f, Palette::textDim);
    setLabel(newDetail, "Start a blank audio workspace", 12.5f, Palette::textDim);
    setLabel(recentTitle, "Recent Workspaces", 16.0f, Palette::textPrimary);
    setLabel(emptyRecent, "No workspaces have been saved yet.", 15.0f, Palette::textPrimary);

    for (auto* label : { &title, &subtitle, &openDetail, &newDetail, &recentTitle, &emptyRecent })
        addAndMakeVisible(*label);

    styleAction(openButton);
    styleAction(newButton);
    newButton.setColour(juce::TextButton::buttonColourId, Palette::standbyGreen.darker(0.62f));
    newButton.setColour(juce::TextButton::buttonOnColourId, Palette::standbyGreen.darker(0.48f));
    openButton.onClick = [this] { if (onOpenWorkspace) onOpenWorkspace(); };
    newButton.onClick = [this] { if (onNewWorkspace) onNewWorkspace(); };

    for (auto* button : { &openButton, &newButton })
        addAndMakeVisible(*button);

    recentViewport.setViewedComponent(&recentList, false);
    recentViewport.setScrollBarsShown(true, false);
    recentViewport.setScrollBarThickness(8);
    recentViewport.setColour(juce::ScrollBar::thumbColourId, Palette::controlDown);
    addAndMakeVisible(recentViewport);

    refresh();
}

void WorkspaceLauncherComponent::styleAction(juce::TextButton& button)
{
    button.getProperties().set("alignLeft", true);
    button.setColour(juce::TextButton::buttonColourId, Palette::controlBg);
    button.setColour(juce::TextButton::buttonOnColourId, Palette::controlDown);
    button.setColour(juce::TextButton::textColourOffId, Palette::textPrimary);
    button.setColour(juce::TextButton::textColourOnId, Palette::textPrimary);
    button.setConnectedEdges(0);
    button.setWantsKeyboardFocus(false);
}

void WorkspaceLauncherComponent::refresh()
{
    recentButtons.clear(true);
    const auto paths = juce::StringArray::fromTokens(properties.getValue("recentWorkspaces"), "\n", "");
    for (const auto& path : paths)
    {
        const juce::File file(path);
        if (! file.existsAsFile() || ! file.hasFileExtension("musicue"))
            continue;

        auto* button = recentButtons.add(new RecentWorkspaceButton(file));
        button->onClick = [this, file] { if (onOpenRecent) onOpenRecent(file); };
        recentList.addAndMakeVisible(button);
    }

    emptyRecent.setVisible(recentButtons.isEmpty());
    recentViewport.setVisible(! recentButtons.isEmpty());
    resized();
}

void WorkspaceLauncherComponent::paint(juce::Graphics& g)
{
    g.fillAll(Palette::windowBg);

    const auto bounds = getLocalBounds();
    const auto dividerX = juce::jmin(leftRailWidth, bounds.getWidth() - 300);
    g.setColour(Palette::divider.brighter(0.25f));
    g.fillRect(dividerX, 0, 1, bounds.getHeight());
    g.fillRect(dividerX, tabHeight - 1, bounds.getWidth() - dividerX, 1);
    g.fillRect(dividerX, bounds.getBottom() - footerHeight, bounds.getWidth() - dividerX, 1);

    if (appIcon.isValid())
        g.drawImageWithin(appIcon, 34, 30, 108, 108,
                          juce::RectanglePlacement::centred,
                          false);

    const auto row = [&](int y, bool active)
    {
        if (active)
        {
            g.setColour(Palette::controlBg);
            g.fillRect(12, y, dividerX - 24, 58);
        }
    };
    row(202, false);
    row(278, false);

    if (recentButtons.isEmpty())
    {
        g.setColour(Palette::selection.brighter(0.08f));
        g.fillRect(dividerX + 1, tabHeight, bounds.getWidth() - dividerX - 1, 46);
    }
}

void WorkspaceLauncherComponent::resized()
{
    const auto bounds = getLocalBounds();
    const auto dividerX = juce::jmin(leftRailWidth, bounds.getWidth() - 300);

    title.setBounds(150, 49, dividerX - 170, 32);
    subtitle.setBounds(151, 83, dividerX - 175, 22);

    openButton.setBounds(26, 190, dividerX - 52, 44);
    openDetail.setBounds(32, 240, dividerX - 58, 20);
    newButton.setBounds(26, 278, dividerX - 52, 44);
    newDetail.setBounds(32, 328, dividerX - 58, 20);

    auto right = bounds.withLeft(dividerX + 1);
    recentTitle.setBounds(right.removeFromTop(tabHeight).reduced(inset, 0));

    auto content = right.withTrimmedBottom(footerHeight).reduced(inset, 16);
    if (recentButtons.isEmpty())
    {
        emptyRecent.setBounds(content.removeFromTop(30));
    }
    else
    {
        recentViewport.setBounds(content);
        const auto rowHeight = 56;
        const auto gap = 7;
        recentList.setSize(content.getWidth(),
                           juce::jmax(content.getHeight(), recentButtons.size() * (rowHeight + gap)));
        auto rows = recentList.getLocalBounds();
        for (auto* button : recentButtons)
        {
            button->setBounds(rows.removeFromTop(rowHeight));
            rows.removeFromTop(gap);
        }
    }
}
