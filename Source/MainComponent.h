#pragma once

#include <functional>
#include <juce_gui_extra/juce_gui_extra.h>
#include "Palette.h"
#include "AudioEngine.h"
#include "CueListComponent.h"
#include "GoButtonComponent.h"
#include "StandbyBarComponent.h"
#include "NotesBarComponent.h"
#include "ToolbarComponent.h"
#include "InspectorComponent.h"
#include "WorkspaceFile.h"
#include "Icons.h"
#include "AppSettings.h"
#include "WorkspaceLauncherComponent.h"
#include "MusiCueLookAndFeel.h"

class SettingsWindow;

class MainComponent : public juce::Component,
                      public juce::DragAndDropContainer,
                      private juce::ChangeListener
{
public:
    explicit MainComponent(juce::PropertiesFile& props);
    ~MainComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;
    void createNewWorkspace();
    void loadWorkspace(const juce::File& source);
    void requestWorkspaceClose(std::function<void()> onConfirmed);

private:
    void triggerGo();
    void stopAllCues();
    void chooseAndAddCues();
    void updateStatusDisplays();
    void saveWorkspace(std::function<void(bool)> completion = {});
    void saveWorkspaceAs(std::function<void(bool)> completion = {});
    void openWorkspace();
    void openWorkspace(const juce::File& source);
    void newWorkspace();
    void showLauncher();
    void showWorkspace();
    void setShowMode(bool enabled);
    void markDirty();
    void addRecentWorkspace(const juce::File& file);
    bool writeWorkspaceTo(const juce::File& target);
    void updateWindowTitle();
    void applySettings();
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    juce::PropertiesFile& properties;
    AppSettings settings;
    WorkspaceLauncherComponent launcher;
    std::unique_ptr<SettingsWindow> settingsWindow;

    MusiCueLookAndFeel lookAndFeel;
    juce::SharedResourcePointer<juce::TooltipWindow> tooltipWindow;
    AudioEngine engine;

    GoButtonComponent goButton;
    StandbyBarComponent standbyBar;
    NotesBarComponent notesBar;
    ToolbarComponent toolbar;
    CueListComponent cueList;
    InspectorComponent inspector;

    juce::Label footerLabel;
    bool inspectorVisible = true;
    juce::StretchableLayoutManager inspectorLayout;
    std::unique_ptr<juce::StretchableLayoutResizerBar> inspectorResizer;
    Icons::IconButton inspectorToggleButton, settingsButton;
    Icons::IconButton openWorkspaceButton, saveWorkspaceButton, saveAsWorkspaceButton;
    juce::TextButton modeButton { "Edit" }, launcherButton { "Workspaces" };

    juce::File workspaceFile;
    juce::File audioExtractFolder;
    std::unique_ptr<juce::FileChooser> fileChooser;
    bool launcherVisible = true;
    bool showMode = false;
    bool dirty = false;
    bool closeConfirmationPending = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
