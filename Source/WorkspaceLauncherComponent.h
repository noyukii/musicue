#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Palette.h"

class WorkspaceLauncherComponent : public juce::Component
{
public:
    explicit WorkspaceLauncherComponent(juce::PropertiesFile& properties);
    void paint(juce::Graphics&) override;
    void resized() override;
    void refresh();

    std::function<void()> onNewWorkspace;
    std::function<void()> onOpenWorkspace;
    std::function<void(const juce::File&)> onOpenRecent;

private:
    class RecentWorkspaceButton final : public juce::Button
    {
    public:
        explicit RecentWorkspaceButton(juce::File workspaceFile);
        void paintButton(juce::Graphics&, bool highlighted, bool down) override;
        const juce::File file;
    };

    void styleAction(juce::TextButton& button);

    juce::PropertiesFile& properties;
    juce::Image appIcon;
    juce::TextButton openButton { "Open Workspace" }, newButton { "New Workspace" };
    juce::Label title, subtitle, openDetail, newDetail, recentTitle, emptyRecent;
    juce::Viewport recentViewport;
    juce::Component recentList;
    juce::OwnedArray<RecentWorkspaceButton> recentButtons;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WorkspaceLauncherComponent)
};
