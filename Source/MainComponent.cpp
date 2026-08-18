#include "MainComponent.h"
#include "SettingsWindow.h"

MainComponent::MainComponent(juce::PropertiesFile& props)
    : properties(props), launcher(props)
{
    settings.load(properties);
    engine.initialise(properties.getValue("audioDeviceState"));

    setLookAndFeel(&lookAndFeel);

    addAndMakeVisible(launcher);
    addAndMakeVisible(goButton);
    addAndMakeVisible(standbyBar);
    addAndMakeVisible(notesBar);
    addAndMakeVisible(toolbar);
    addAndMakeVisible(cueList);
    addAndMakeVisible(inspector);

    inspectorToggleButton = Icons::makeButton("toggleInspector", BinaryData::view_agenda_svg,
                                              BinaryData::view_agenda_svgSize, Palette::textPrimary,
                                              "Show/hide inspector");
    settingsButton = Icons::makeButton("settings", BinaryData::settings_svg,
                                       BinaryData::settings_svgSize, Palette::textPrimary,
                                       "Workspace settings");
    openWorkspaceButton = Icons::makeButton("openWorkspace", BinaryData::workspace_open_svg,
                                            BinaryData::workspace_open_svgSize, Palette::textPrimary,
                                            "Open workspace (Command+O)");
    saveWorkspaceButton = Icons::makeButton("saveWorkspace", BinaryData::workspace_save_svg,
                                            BinaryData::workspace_save_svgSize, Palette::textPrimary,
                                            "Save workspace (Command+S)");
    saveAsWorkspaceButton = Icons::makeButton("saveAsWorkspace", BinaryData::workspace_save_as_svg,
                                              BinaryData::workspace_save_as_svgSize, Palette::textPrimary,
                                              "Save workspace as (Command+Shift+S)");
    addAndMakeVisible(*inspectorToggleButton.button);
    addAndMakeVisible(*settingsButton.button);
    addAndMakeVisible(*openWorkspaceButton.button);
    addAndMakeVisible(*saveWorkspaceButton.button);
    addAndMakeVisible(*saveAsWorkspaceButton.button);
    modeButton.setWantsKeyboardFocus(false);
    launcherButton.setWantsKeyboardFocus(false);
    modeButton.onClick = [this] { setShowMode(! showMode); };
    launcherButton.onClick = [this]
    {
        requestWorkspaceClose([this] { showLauncher(); });
    };
    openWorkspaceButton.button->onClick = [this] { openWorkspace(); };
    saveWorkspaceButton.button->onClick = [this] { saveWorkspace(); };
    saveAsWorkspaceButton.button->onClick = [this] { saveWorkspaceAs(); };
    addAndMakeVisible(modeButton);
    addAndMakeVisible(launcherButton);
    launcher.onNewWorkspace = [this] { newWorkspace(); };
    launcher.onOpenWorkspace = [this] { openWorkspace(); };
    launcher.onOpenRecent = [this](const juce::File& file) { openWorkspace(file); };

    settingsButton.button->onClick = [this]
    {
        if (settingsWindow == nullptr)
            settingsWindow = std::make_unique<SettingsWindow>(engine.getDeviceManager(), settings,
                                                              properties,
                                                              [this] { applySettings(); });

        settingsWindow->setVisible(true);
        settingsWindow->toFront(true);
    };

    goButton.setTooltip("GO (Space)");

    footerLabel.setJustificationType(juce::Justification::centred);
    footerLabel.setColour(juce::Label::textColourId, Palette::textDim);
    footerLabel.setFont(juce::Font(juce::FontOptions().withHeight(12.0f)));
    addAndMakeVisible(footerLabel);

    inspectorLayout.setItemLayout(0, 150, -1, -0.5);
    inspectorLayout.setItemLayout(1, 12, 12, 12);
    inspectorLayout.setItemLayout(2, 150, 560, -0.5);
    inspectorResizer = std::make_unique<juce::StretchableLayoutResizerBar>(&inspectorLayout, 1, false);
    addAndMakeVisible(*inspectorResizer);

    goButton.onClick = [this] { triggerGo(); };

    toolbar.onAddCue = [this] { chooseAndAddCues(); };
    toolbar.onAddGroup = [this] { cueList.addGroupCue(); };
    toolbar.onAddFade = [this] { cueList.addFadeCue(); };
    toolbar.onPreview = [this] { cueList.previewSelected(); };
    toolbar.onStop = [this] { cueList.stopSelectedCue(); };
    toolbar.onPause = [this] { engine.setPaused(! engine.isPaused()); };
    toolbar.onPanic = [this] { stopAllCues(); };
    toolbar.onReset = [this] { cueList.resetStandby(); };
    toolbar.onMasterGain = [this](float gain) { engine.setMasterGain(gain); };

    cueList.durationProvider = [this](const juce::File& file) { return engine.getDurationForFile(file); };
    cueList.onPlayCue = [this](const Cue& cue)
    {
        return engine.playCue(cue, [this](int cueId) -> const Cue*
        {
            for (const auto& candidate : cueList.getCues())
                if (candidate.id == cueId)
                    return &candidate;
            return nullptr;
        });
    };
    cueList.onStopCue = [this](int cueId) { engine.stopCue(cueId); };
    cueList.onSelectionChanged = [this] { inspector.setCue(cueList.getSelectedCue()); };
    cueList.onContentChanged = [this]
    {
        inspector.setAvailableCues(cueList.getCues());
        markDirty(); updateStatusDisplays();
    };

    engine.onCueFinished = [this](int cueId, bool completedNaturally)
    {
        cueList.markCueFinished(cueId);

        if (completedNaturally)
            cueList.handleAutoContinue(cueId);
    };
    engine.onCueStarted = [this](int cueId) { cueList.markCueStarted(cueId); };

    inspector.onCueEdited = [this]
    {
        if (auto* cue = cueList.getSelectedCue())
            engine.updateCueParameters(cue->id, cue->gainDb, cue->pan);
        cueList.refreshDisplay();
        markDirty();
        updateStatusDisplays();
    };
    inspector.playingCueProvider = [this](int excludeId)
    {
        return cueList.firstPlayingAudioCueId(excludeId);
    };

    inspectorToggleButton.button->onClick = [this]
    {
        inspectorVisible = ! inspectorVisible;
        inspector.setVisible(inspectorVisible);
        inspectorResizer->setVisible(inspectorVisible);
        resized();
    };

    updateStatusDisplays();
    applySettings();
    engine.getDeviceManager().addChangeListener(this);

    setWantsKeyboardFocus(true);
    grabKeyboardFocus();
    setSize(1150, 760);
    showLauncher();
    checkForUpdates();
}

MainComponent::~MainComponent()
{
    engine.getDeviceManager().removeChangeListener(this);
    setLookAndFeel(nullptr);
    audioExtractFolder.deleteRecursively();
}

void MainComponent::createNewWorkspace()
{
    newWorkspace();
}

void MainComponent::loadWorkspace(const juce::File& source)
{
    openWorkspace(source);
}

void MainComponent::requestWorkspaceClose(std::function<void()> onConfirmed)
{
    if (! dirty)
    {
        if (onConfirmed)
            onConfirmed();
        return;
    }

    if (closeConfirmationPending)
        return;

    closeConfirmationPending = true;
    const auto workspaceName = workspaceFile != juce::File()
                                   ? workspaceFile.getFileNameWithoutExtension()
                                   : "Untitled Workspace";
    const auto options = juce::MessageBoxOptions()
                             .withIconType(juce::MessageBoxIconType::QuestionIcon)
                             .withTitle("Save changes before closing?")
                             .withMessage("Save changes to " + workspaceName + " before closing the workspace?")
                             .withButton("Save")
                             .withButton("Discard Changes")
                             .withButton("Cancel")
                             .withAssociatedComponent(this);

    juce::AlertWindow::showAsync(options,
        [this, confirmedAction = std::move(onConfirmed)](int result) mutable
        {
            closeConfirmationPending = false;

            if (result == 1)
            {
                saveWorkspace([actionAfterSave = std::move(confirmedAction)](bool saved) mutable
                {
                    if (saved && actionAfterSave)
                        actionAfterSave();
                });
            }
            else if (result == 2)
            {
                dirty = false;
                updateWindowTitle();
                if (confirmedAction)
                    confirmedAction();
            }
        });
}

void MainComponent::applySettings()
{
    toolbar.setMasterGain(settings.masterGain);
    engine.setMasterGain(settings.masterGain);
    cueList.setStandbyLinked(settings.standbyLinked);
    cueList.setDefaultContinueMode(settings.defaultContinueMode);
}

void MainComponent::changeListenerCallback(juce::ChangeBroadcaster*)
{
    properties.setValue("audioDeviceState", engine.getAudioStateXml());
    properties.saveIfNeeded();
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(Palette::windowBg);
}

void MainComponent::resized()
{
    if (launcherVisible)
    {
        launcher.setBounds(getLocalBounds());
        return;
    }

    auto area = getLocalBounds().reduced(10);

    auto topRow = area.removeFromTop(96);
    goButton.setBounds(topRow.removeFromLeft(140));
    topRow.removeFromLeft(10);
    standbyBar.setBounds(topRow.removeFromTop(44));
    topRow.removeFromTop(8);
    notesBar.setBounds(topRow);

    area.removeFromTop(10);
    toolbar.setBounds(area.removeFromTop(52));
    area.removeFromTop(8);

    auto footerArea = area.removeFromBottom(26);
    area.removeFromBottom(6);
    area.setLeft(0);
    area.setRight(getWidth());

    if (inspectorVisible)
    {
        juce::Component* split[] = { &cueList, inspectorResizer.get(), &inspector };
        inspectorLayout.layOutComponents(split, 3, area.getX(), area.getY(),
                                         area.getWidth(), area.getHeight(), true, true);
    }
    else
    {
        cueList.setBounds(area);
    }

    settingsButton.button->setBounds(footerArea.removeFromRight(30));
    inspectorToggleButton.button->setBounds(footerArea.removeFromRight(30));
    modeButton.setBounds(footerArea.removeFromRight(64));
    launcherButton.setBounds(footerArea.removeFromLeft(100));
    footerArea.removeFromLeft(6);
    openWorkspaceButton.button->setBounds(footerArea.removeFromLeft(30));
    footerArea.removeFromLeft(4);
    saveWorkspaceButton.button->setBounds(footerArea.removeFromLeft(30));
    footerArea.removeFromLeft(4);
    saveAsWorkspaceButton.button->setBounds(footerArea.removeFromLeft(30));
    footerLabel.setBounds(footerArea);
}

bool MainComponent::keyPressed(const juce::KeyPress& key)
{
    if (launcherVisible)
        return false;

    if (cueList.triggerHotkey(key))
        return true;

    if (key == juce::KeyPress::spaceKey)
    {
        triggerGo();
        return true;
    }

    if (key == juce::KeyPress::escapeKey)
    {
        stopAllCues();
        return true;
    }

    if (key.getModifiers().isCommandDown())
    {
        const auto character = key.getTextCharacter();

        if (character == 's') { saveWorkspace(); return true; }
        if (character == 'S') { saveWorkspaceAs(); return true; }
        if (character == 'o') { openWorkspace(); return true; }
        if (character == 'd') { cueList.duplicateSelectedCue(); return true; }
        if (character == 'c') { cueList.copySelectedCue(); return true; }
        if (character == 'v') { cueList.pasteCues(); return true; }
        if (character == 'p') { cueList.previewSelected(); return true; }
        if (character == 'z') { cueList.undo(); return true; }
        if (character == ']') { cueList.indentSelectedCue(); return true; }
        if (character == '[') { cueList.outdentSelectedCue(); return true; }

        if (key == juce::KeyPress::upKey) { cueList.moveSelectedCue(-1); return true; }
        if (key == juce::KeyPress::downKey) { cueList.moveSelectedCue(1); return true; }
    }
    else if (key == juce::KeyPress::backspaceKey || key == juce::KeyPress::deleteKey)
    {
        cueList.deleteSelectedCue();
        return true;
    }
    else if (key == juce::KeyPress::leftKey)
    {
        cueList.collapseSelectedGroup();
        return true;
    }
    else if (key == juce::KeyPress::rightKey)
    {
        cueList.expandSelectedGroup();
        return true;
    }

    return false;
}

void MainComponent::triggerGo()
{
    cueList.triggerStandby();
}

void MainComponent::stopAllCues()
{
    engine.stopAll();
    cueList.markAllIdle();
}

void MainComponent::updateStatusDisplays()
{
    if (auto* standby = cueList.getStandbyCue())
    {
        standbyBar.setText(standby->number + " - " + standby->name);
        goButton.setArmed(true);
    }
    else
    {
        standbyBar.setText({});
        goButton.setArmed(false);
    }

    const auto count = cueList.getCueCount();
    footerLabel.setText(juce::String(count) + (count == 1 ? " cue" : " cues") + " - "
                            + engine.getCurrentDeviceName(),
                        juce::dontSendNotification);
}

void MainComponent::chooseAndAddCues()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        "Add audio cues", juce::File(), "*.wav;*.aif;*.aiff;*.mp3;*.flac;*.ogg");

    const auto flags = juce::FileBrowserComponent::openMode
                     | juce::FileBrowserComponent::canSelectFiles
                     | juce::FileBrowserComponent::canSelectMultipleItems;

    fileChooser->launchAsync(flags, [this](const juce::FileChooser& chooser)
    {
        for (const auto& file : chooser.getResults())
            cueList.addCueFromFile(file);

        fileChooser.reset();
    });
}

void MainComponent::saveWorkspace(std::function<void(bool)> completion)
{
    if (workspaceFile == juce::File())
        saveWorkspaceAs(std::move(completion));
    else
    {
        const auto saved = writeWorkspaceTo(workspaceFile);
        if (completion)
            completion(saved);
    }
}

void MainComponent::saveWorkspaceAs(std::function<void(bool)> completion)
{
    fileChooser = std::make_unique<juce::FileChooser>(
        "Save Workspace",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("Untitled.musicue"),
        "*.musicue");

    const auto flags = juce::FileBrowserComponent::saveMode
                     | juce::FileBrowserComponent::canSelectFiles
                     | juce::FileBrowserComponent::warnAboutOverwriting;

    fileChooser->launchAsync(flags, [this, saveFinished = std::move(completion)](const juce::FileChooser& chooser) mutable
    {
        auto target = chooser.getResult();
        auto saved = false;

        if (target != juce::File())
        {
            if (! target.hasFileExtension("musicue"))
                target = target.withFileExtension(".musicue");

            const auto previousWorkspaceFile = workspaceFile;
            workspaceFile = target;
            saved = writeWorkspaceTo(target);
            if (! saved)
                workspaceFile = previousWorkspaceFile;
        }

        fileChooser.reset();
        if (saveFinished)
            saveFinished(saved);
    });
}

bool MainComponent::writeWorkspaceTo(const juce::File& target)
{
    WorkspaceData data;
    data.cues = cueList.getCues();
    data.standbyCueId = cueList.getStandbyCueId();
    data.masterGain = toolbar.getMasterGain();
    data.notes = notesBar.getText();

    if (WorkspaceFile::save(target, data))
    {
        dirty = false;
        addRecentWorkspace(target);
        updateWindowTitle();
        return true;
    }

    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                           "Save Failed",
                                           "Couldn't write " + target.getFullPathName());
    return false;
}

void MainComponent::openWorkspace()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        "Open Workspace",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        "*.musicue");

    const auto flags = juce::FileBrowserComponent::openMode
                     | juce::FileBrowserComponent::canSelectFiles;

    fileChooser->launchAsync(flags, [this](const juce::FileChooser& chooser)
    {
        if (const auto source = chooser.getResult(); source != juce::File())
            requestWorkspaceClose([this, source] { openWorkspace(source); });

        fileChooser.reset();
    });
}

void MainComponent::openWorkspace(const juce::File& source)
{
    if (source.hasFileExtension("cue"))
    {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                               "Legacy workspace unsupported",
                                               "MusiCue opens .musicue workspaces only. Existing .cue files were not changed.");
        return;
    }

    if (! source.hasFileExtension("musicue"))
        return;

    stopAllCues();
    auto extractRoot = juce::File::getSpecialLocation(juce::File::tempDirectory)
                           .getChildFile("MusiCue").getChildFile(juce::Uuid().toString());
    WorkspaceData data;
    if (! WorkspaceFile::load(source, data, extractRoot))
    {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                               "Open Failed", "Couldn't open " + source.getFullPathName());
        return;
    }

    audioExtractFolder.deleteRecursively();
    audioExtractFolder = extractRoot;
    workspaceFile = source;
    toolbar.setMasterGain(data.masterGain);
    engine.setMasterGain(data.masterGain);
    notesBar.setText(data.notes);
    cueList.setCues(std::move(data.cues), data.standbyCueId);
    dirty = false;
    showWorkspace();
    addRecentWorkspace(source);
    updateWindowTitle();
    resized();
}

void MainComponent::newWorkspace()
{
    stopAllCues();
    workspaceFile = juce::File();
    notesBar.setText({});
    cueList.setCues({}, 0);
    dirty = false;
    showWorkspace();
    updateWindowTitle();
    resized();
}

void MainComponent::showLauncher()
{
    launcher.refresh();
    launcherVisible = true;
    launcher.setVisible(true);
    for (auto* component : { static_cast<juce::Component*>(&goButton), static_cast<juce::Component*>(&standbyBar),
                             static_cast<juce::Component*>(&notesBar), static_cast<juce::Component*>(&toolbar),
                             static_cast<juce::Component*>(&cueList), static_cast<juce::Component*>(&inspector),
                             static_cast<juce::Component*>(&footerLabel), static_cast<juce::Component*>(&modeButton),
                             static_cast<juce::Component*>(&launcherButton) })
        component->setVisible(false);
    inspectorResizer->setVisible(false);
    settingsButton.button->setVisible(false);
    inspectorToggleButton.button->setVisible(false);
    openWorkspaceButton.button->setVisible(false);
    saveWorkspaceButton.button->setVisible(false);
    saveAsWorkspaceButton.button->setVisible(false);
    resized();
}

void MainComponent::showWorkspace()
{
    launcherVisible = false;
    launcher.setVisible(false);
    for (auto* component : { static_cast<juce::Component*>(&goButton), static_cast<juce::Component*>(&standbyBar),
                             static_cast<juce::Component*>(&notesBar), static_cast<juce::Component*>(&toolbar),
                             static_cast<juce::Component*>(&cueList), static_cast<juce::Component*>(&footerLabel),
                             static_cast<juce::Component*>(&modeButton), static_cast<juce::Component*>(&launcherButton) })
        component->setVisible(true);
    settingsButton.button->setVisible(true);
    openWorkspaceButton.button->setVisible(true);
    saveWorkspaceButton.button->setVisible(true);
    saveAsWorkspaceButton.button->setVisible(true);
    inspector.setVisible(! showMode);
    inspectorResizer->setVisible(! showMode);
    inspectorToggleButton.button->setVisible(! showMode);
}

void MainComponent::setShowMode(bool enabled)
{
    showMode = enabled;
    modeButton.setButtonText(showMode ? "Show" : "Edit");
    inspectorVisible = ! showMode;
    inspector.setVisible(! showMode);
    inspectorResizer->setVisible(! showMode);
    toolbar.setEditingEnabled(! showMode);
    cueList.setEditingEnabled(! showMode);
    inspectorToggleButton.button->setVisible(! showMode);
    resized();
}

void MainComponent::markDirty()
{
    if (! launcherVisible)
    {
        dirty = true;
        updateWindowTitle();
    }
}

void MainComponent::addRecentWorkspace(const juce::File& file)
{
    auto recent = juce::StringArray::fromTokens(properties.getValue("recentWorkspaces"), "\n", "");
    recent.removeString(file.getFullPathName());
    recent.insert(0, file.getFullPathName());
    while (recent.size() > 8)
        recent.remove(recent.size() - 1);
    properties.setValue("recentWorkspaces", recent.joinIntoString("\n"));
    properties.saveIfNeeded();
}

void MainComponent::checkForUpdates()
{
    const auto skippedVersion = properties.getValue("skippedUpdateVersion");

    updateChecker.checkForUpdates(juce::JUCEApplication::getInstance()->getApplicationVersion(),
        [safeThis = juce::Component::SafePointer<MainComponent>(this), skippedVersion](UpdateChecker::Result result)
        {
            if (safeThis == nullptr || ! result.updateAvailable || result.latestVersion == skippedVersion)
                return;

            safeThis->showUpdatePopup(result.latestVersion);
        });
}

void MainComponent::showUpdatePopup(const juce::String& version)
{
    if (updatePopupShowing)
        return;

    updatePopupShowing = true;
    auto safeThis = juce::Component::SafePointer<MainComponent>(this);

    const auto options = juce::MessageBoxOptions()
                             .withIconType(juce::MessageBoxIconType::InfoIcon)
                             .withTitle("MusiCue " + version + " is available")
                             .withMessage("A new version of MusiCue has been released.\n"
                                          "You are currently running version "
                                          + juce::JUCEApplication::getInstance()->getApplicationVersion() + ".")
                             .withButton("Update")
                             .withButton("Dismiss")
                             .withButton("Do Not Ask Again")
                             .withAssociatedComponent(this);

    juce::AlertWindow::showAsync(options,
        [safeThis, version](int result)
        {
            if (safeThis == nullptr)
                return;

            safeThis->updatePopupShowing = false;

            if (result == 1)
                juce::URL("https://noyukii.github.io/musicue").launchInDefaultBrowser();
            else if (result == 3)
            {
                safeThis->properties.setValue("skippedUpdateVersion", version);
                safeThis->properties.saveIfNeeded();
            }
        });
}

void MainComponent::updateWindowTitle()
{
    const auto name = workspaceFile != juce::File()
                          ? workspaceFile.getFileNameWithoutExtension()
                          : "Untitled Workspace";

    if (auto* window = findParentComponentOfClass<juce::DocumentWindow>())
        window->setName((dirty ? "* " : "") + name + " - MusiCue");
}
