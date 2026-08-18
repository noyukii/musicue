#include "SettingsWindow.h"
#include "Palette.h"
#include <vector>

namespace
{
    class AudioTab : public juce::Component,
                     private juce::ChangeListener
    {
    public:
        explicit AudioTab(juce::AudioDeviceManager& dm) : deviceManager(dm)
        {
            setupLabel(deviceLabel, "Output device:");
            setupLabel(sampleRateLabel, "Sample rate:");
            setupLabel(bufferSizeLabel, "Buffer size:");

            deviceBox.onChange = [this]
            {
                auto setup = deviceManager.getAudioDeviceSetup();
                setup.outputDeviceName = deviceBox.getText();
                deviceManager.setAudioDeviceSetup(setup, true);
            };
            sampleRateBox.onChange = [this]
            {
                auto setup = deviceManager.getAudioDeviceSetup();
                setup.sampleRate = sampleRateBox.getSelectedId();
                deviceManager.setAudioDeviceSetup(setup, true);
            };
            bufferSizeBox.onChange = [this]
            {
                auto setup = deviceManager.getAudioDeviceSetup();
                setup.bufferSize = bufferSizeBox.getSelectedId();
                deviceManager.setAudioDeviceSetup(setup, true);
            };

            addAndMakeVisible(deviceBox);
            addAndMakeVisible(sampleRateBox);
            addAndMakeVisible(bufferSizeBox);

            deviceManager.addChangeListener(this);
            refresh();
        }

        ~AudioTab() override
        {
            deviceManager.removeChangeListener(this);
        }

        void paint(juce::Graphics& g) override
        {
            g.fillAll(Palette::panelBg);
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced(28, 24);

            auto row = [&r](juce::Label& label, juce::ComboBox& box)
            {
                auto line = r.removeFromTop(34);
                label.setBounds(line.removeFromLeft(132));
                box.setBounds(line.removeFromLeft(330));
                r.removeFromTop(16);
            };

            row(deviceLabel, deviceBox);
            row(sampleRateLabel, sampleRateBox);
            row(bufferSizeLabel, bufferSizeBox);
        }

    private:
        void setupLabel(juce::Label& label, const juce::String& text)
        {
            label.setText(text, juce::dontSendNotification);
            label.setJustificationType(juce::Justification::centredLeft);
            label.setColour(juce::Label::textColourId, Palette::textDim);
            label.setFont(juce::Font(juce::FontOptions().withHeight(13.0f)));
            addAndMakeVisible(label);
        }

        void changeListenerCallback(juce::ChangeBroadcaster*) override
        {
            refresh();
        }

        void refresh()
        {
            auto setup = deviceManager.getAudioDeviceSetup();

            deviceBox.clear(juce::dontSendNotification);

            if (auto* type = deviceManager.getCurrentDeviceTypeObject())
            {
                const auto names = type->getDeviceNames(false);

                for (int i = 0; i < names.size(); ++i)
                    deviceBox.addItem(names[i], i + 1);

                deviceBox.setSelectedItemIndex(names.indexOf(setup.outputDeviceName),
                                               juce::dontSendNotification);
            }

            sampleRateBox.clear(juce::dontSendNotification);
            bufferSizeBox.clear(juce::dontSendNotification);

            if (auto* device = deviceManager.getCurrentAudioDevice())
            {
                for (auto rate : device->getAvailableSampleRates())
                    sampleRateBox.addItem(juce::String(rate) + " Hz", static_cast<int>(rate));

                sampleRateBox.setSelectedId(static_cast<int>(device->getCurrentSampleRate()),
                                            juce::dontSendNotification);

                for (auto size : device->getAvailableBufferSizes())
                    bufferSizeBox.addItem(juce::String(size) + " samples", size);

                bufferSizeBox.setSelectedId(device->getCurrentBufferSizeSamples(),
                                            juce::dontSendNotification);
            }
        }

        juce::AudioDeviceManager& deviceManager;
        juce::Label deviceLabel, sampleRateLabel, bufferSizeLabel;
        juce::ComboBox deviceBox, sampleRateBox, bufferSizeBox;
    };

    class PlaybackTab : public juce::Component
    {
    public:
        PlaybackTab(AppSettings& s, juce::PropertiesFile& p, std::function<void()> notify)
            : settings(s), props(p), onChanged(std::move(notify))
        {
            standbyLinkedToggle.setButtonText("Standby follows selection");
            standbyLinkedToggle.setColour(juce::ToggleButton::textColourId, Palette::textPrimary);
            standbyLinkedToggle.setToggleState(settings.standbyLinked, juce::dontSendNotification);
            standbyLinkedToggle.onClick = [this]
            {
                settings.standbyLinked = standbyLinkedToggle.getToggleState();
                commit();
            };
            addAndMakeVisible(standbyLinkedToggle);

            auto setupLabel = [this](juce::Label& label, const juce::String& text)
            {
                label.setText(text, juce::dontSendNotification);
                label.setJustificationType(juce::Justification::centredLeft);
                label.setColour(juce::Label::textColourId, Palette::textDim);
                label.setFont(juce::Font(juce::FontOptions().withHeight(13.0f)));
                addAndMakeVisible(label);
            };

            setupLabel(continueLabel, "Default continue mode for new cues:");
            continueBox.addItem("Do not continue", 1);
            continueBox.addItem("Auto-continue", 2);
            continueBox.addItem("Auto-follow", 3);
            continueBox.setSelectedId(settings.defaultContinueMode + 1, juce::dontSendNotification);
            continueBox.onChange = [this]
            {
                settings.defaultContinueMode = continueBox.getSelectedId() - 1;
                commit();
            };
            addAndMakeVisible(continueBox);

            setupLabel(gainLabel, "Master gain:");
            gainSlider.setSliderStyle(juce::Slider::LinearHorizontal);
            gainSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 70, 22);
            gainSlider.setTextValueSuffix(" dB");
            gainSlider.setRange(-60.0, 0.0, 0.1);
            gainSlider.setDoubleClickReturnValue(true, 0.0);
            gainSlider.setSliderSnapsToMousePosition(false);
            gainSlider.setValue(juce::Decibels::gainToDecibels(settings.masterGain, -60.0f),
                                juce::dontSendNotification);
            gainSlider.setColour(juce::Slider::trackColourId, Palette::textDim.brighter(0.12f));
            gainSlider.setColour(juce::Slider::thumbColourId, Palette::textPrimary);
            gainSlider.onValueChange = [this]
            {
                settings.masterGain = juce::Decibels::decibelsToGain(
                    static_cast<float>(gainSlider.getValue()), -60.0f);
                commit();
            };
            addAndMakeVisible(gainSlider);
        }

        void paint(juce::Graphics& g) override
        {
            g.fillAll(Palette::panelBg);
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced(28, 24);

            standbyLinkedToggle.setBounds(r.removeFromTop(32));
            r.removeFromTop(22);

            auto continueRow = r.removeFromTop(34);
            continueLabel.setBounds(continueRow.removeFromLeft(250));
            continueBox.setBounds(continueRow.removeFromLeft(210));
            r.removeFromTop(20);

            auto gainRow = r.removeFromTop(34);
            gainLabel.setBounds(gainRow.removeFromLeft(250));
            gainSlider.setBounds(gainRow.removeFromLeft(230));
        }

    private:
        void commit()
        {
            settings.save(props);

            if (onChanged != nullptr)
                onChanged();
        }

        AppSettings& settings;
        juce::PropertiesFile& props;
        std::function<void()> onChanged;

        juce::ToggleButton standbyLinkedToggle;
        juce::Label continueLabel, gainLabel;
        juce::ComboBox continueBox;
        juce::Slider gainSlider;
    };

    class ShortcutRow : public juce::Component
    {
    public:
        ShortcutRow(ShortcutBindings& map, const ShortcutInfo& info, std::function<void()> changed)
            : bindings(map), id(info.id), onChanged(std::move(changed))
        {
            name.setText(info.name, juce::dontSendNotification);

            name.setJustificationType(juce::Justification::centredLeft);
            name.setColour(juce::Label::textColourId, Palette::textPrimary);
            name.setFont(juce::Font(juce::FontOptions().withHeight(13.0f)));
            addAndMakeVisible(name);

            keyButton.setWantsKeyboardFocus(false);
            keyButton.setMouseClickGrabsKeyboardFocus(false);
            keyButton.onClick = [this] { toggleCapture(); };
            addAndMakeVisible(keyButton);

            clearButton.setButtonText("Clear");
            clearButton.setWantsKeyboardFocus(false);
            clearButton.setMouseClickGrabsKeyboardFocus(false);
            clearButton.onClick = [this]
            {
                stopCapture();
                bindings.clear(id);
                refresh();
                notifyChanged();
            };
            addAndMakeVisible(clearButton);

            setWantsKeyboardFocus(true);
            refresh();
        }

        void refresh()
        {
            capturing = false;
            const auto text = bindings.getDisplayString(id);
            keyButton.setButtonText(text.isNotEmpty() ? text : "None");
        }

        void stopCapture()
        {
            if (! capturing)
                return;

            capturing = false;
            refresh();
        }

        bool keyPressed(const juce::KeyPress& key) override
        {
            if (! capturing)
                return false;

            if (! key.isValid() || key.getKeyCode() == 0)
                return true;

            capturing = false;

            if (const auto* conflict = bindings.findConflict(key, id))
            {
                const auto message = ShortcutBindings::describe(key) + " is already assigned to \""
                    + conflict->name + "\". Reassign it?";
                juce::AlertWindow::showAsync(
                    juce::MessageBoxOptions()
                        .withIconType(juce::MessageBoxIconType::QuestionIcon)
                        .withTitle("Shortcut already in use")
                        .withMessage(message)
                        .withButton("Reassign")
                        .withButton("Cancel")
                        .withAssociatedComponent(this),
                    [safeThis = juce::Component::SafePointer<ShortcutRow>(this), key](int result)
                    {
                        if (safeThis == nullptr)
                            return;

                        if (result == 1)
                        {
                            safeThis->bindings.assign(safeThis->id, key);
                            safeThis->notifyChanged();
                        }

                        safeThis->refresh();
                    });
                return true;
            }

            bindings.assign(id, key);
            refresh();
            notifyChanged();
            return true;
        }

        void focusLost(FocusChangeType) override
        {
            if (capturing)
                stopCapture();
        }

        void resized() override
        {
            auto r = getLocalBounds();
            clearButton.setBounds(r.removeFromRight(64));
            r.removeFromRight(8);
            keyButton.setBounds(r.removeFromRight(200));
            r.removeFromRight(12);
            name.setBounds(r);
        }

        std::function<void(ShortcutRow*)> onCaptureStarted;

    private:
        void toggleCapture()
        {
            if (capturing)
            {
                stopCapture();
                return;
            }

            if (onCaptureStarted != nullptr)
                onCaptureStarted(this);

            capturing = true;
            keyButton.setButtonText("Press a key...");
            grabKeyboardFocus();
        }

        void notifyChanged()
        {
            if (onChanged != nullptr)
                onChanged();
        }

        ShortcutBindings& bindings;
        ShortcutId id;
        std::function<void()> onChanged;
        juce::Label name;
        juce::TextButton keyButton, clearButton;
        bool capturing = false;
    };

    class ShortcutList : public juce::Component
    {
    public:
        ShortcutList(ShortcutBindings& map, std::function<void()> changed)
        {
            juce::String lastCategory;

            for (const auto& info : ShortcutBindings::catalog())
            {
                if (lastCategory != info.category)
                {
                    lastCategory = info.category;
                    auto header = std::make_unique<juce::Label>();
                    header->setText(info.category, juce::dontSendNotification);
                    header->setFont(juce::Font(juce::FontOptions().withHeight(14.0f)));
                    header->setColour(juce::Label::textColourId, Palette::textPrimary);
                    addAndMakeVisible(*header);
                    headers.push_back(std::move(header));
                }

                auto row = std::make_unique<ShortcutRow>(map, info, changed);
                row->onCaptureStarted = [this](ShortcutRow* active)
                {
                    for (auto& other : rows)
                        if (other.get() != active)
                            other->stopCapture();
                };
                addAndMakeVisible(*row);
                rows.push_back(std::move(row));
            }
        }

        int preferredHeight() const
        {
            return static_cast<int>(headers.size()) * 28
                 + static_cast<int>(rows.size()) * 34
                 + 8;
        }

        void refresh()
        {
            for (auto& row : rows)
                row->refresh();
        }

        void resized() override
        {
            auto r = getLocalBounds();
            size_t headerIndex = 0, rowIndex = 0;
            juce::String lastCategory;

            for (const auto& info : ShortcutBindings::catalog())
            {
                if (lastCategory != info.category)
                {
                    lastCategory = info.category;
                    headers[headerIndex++]->setBounds(r.removeFromTop(24));
                    r.removeFromTop(4);
                }

                rows[rowIndex++]->setBounds(r.removeFromTop(30));
                r.removeFromTop(4);
            }
        }

    private:
        std::vector<std::unique_ptr<juce::Label>> headers;
        std::vector<std::unique_ptr<ShortcutRow>> rows;
    };

    class ControlsTab : public juce::Component
    {
    public:
        ControlsTab(AppSettings& s, juce::PropertiesFile& p, std::function<void()> notify)
            : settings(s), props(p), onChanged(std::move(notify)),
              list(settings.shortcuts, [this] { commit(); })
        {
            help.setText("Click a shortcut to change it. Click again or elsewhere to cancel.\n"
                         "Cue hotkeys in the inspector fire independently and take priority.",
                         juce::dontSendNotification);
            help.setFont(juce::Font(juce::FontOptions().withHeight(13.0f)));
            help.setColour(juce::Label::textColourId, Palette::textDim);
            help.setJustificationType(juce::Justification::topLeft);
            addAndMakeVisible(help);

            resetButton.setButtonText("Reset to Defaults");
            resetButton.setWantsKeyboardFocus(false);
            resetButton.onClick = [this]
            {
                settings.shortcuts.resetToDefaults();
                list.refresh();
                commit();
            };
            addAndMakeVisible(resetButton);

            viewport.setViewedComponent(&list, false);
            viewport.setScrollBarsShown(true, false);
            addAndMakeVisible(viewport);
        }

        void paint(juce::Graphics& g) override
        {
            g.fillAll(Palette::panelBg);
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced(28, 20);
            help.setBounds(r.removeFromTop(36));
            r.removeFromTop(8);
            resetButton.setBounds(r.removeFromTop(28).removeFromLeft(160));
            r.removeFromTop(12);
            viewport.setBounds(r);
            list.setSize(juce::jmax(1, viewport.getMaximumVisibleWidth()),
                         juce::jmax(viewport.getHeight(), list.preferredHeight()));
        }

    private:
        void commit()
        {
            settings.save(props);
            list.refresh();

            if (onChanged != nullptr)
                onChanged();
        }

        AppSettings& settings;
        juce::PropertiesFile& props;
        std::function<void()> onChanged;
        juce::Label help;
        juce::TextButton resetButton;
        ShortcutList list;
        juce::Viewport viewport;
    };

    class SettingsComponent : public juce::Component
    {
    public:
        SettingsComponent(juce::AudioDeviceManager& deviceManager,
                          AppSettings& settings,
                          juce::PropertiesFile& props,
                          std::function<void()> notify)
            : tabs(juce::TabbedButtonBar::TabsAtTop)
        {
            audioTab = std::make_unique<AudioTab>(deviceManager);
            playbackTab = std::make_unique<PlaybackTab>(settings, props, notify);
            controlsTab = std::make_unique<ControlsTab>(settings, props, std::move(notify));

            tabs.setTabBarDepth(40);
            tabs.addTab("General", Palette::panelBg, playbackTab.get(), false);
            tabs.addTab("Controls", Palette::panelBg, controlsTab.get(), false);
            tabs.addTab("Audio", Palette::panelBg, audioTab.get(), false);
            addAndMakeVisible(tabs);
        }

        void paint(juce::Graphics& g) override
        {
            g.fillAll(Palette::panelBg);
        }

        void resized() override
        {
            tabs.setBounds(getLocalBounds());
        }

    private:
        juce::TabbedComponent tabs;
        std::unique_ptr<AudioTab> audioTab;
        std::unique_ptr<PlaybackTab> playbackTab;
        std::unique_ptr<ControlsTab> controlsTab;
    };
}

SettingsWindow::SettingsWindow(juce::AudioDeviceManager& deviceManager,
                               AppSettings& settings,
                               juce::PropertiesFile& props,
                               std::function<void()> onSettingsChanged)
    : DocumentWindow("Settings", Palette::panelBg, DocumentWindow::closeButton)
{
    setUsingNativeTitleBar(true);
    setLookAndFeel(&lookAndFeel);
    setContentOwned(new SettingsComponent(deviceManager, settings, props,
                                          std::move(onSettingsChanged)), true);
    setResizable(true, true);
    setResizeLimits(560, 420, 920, 900);
    centreWithSize(640, 540);
}

SettingsWindow::~SettingsWindow()
{
    setLookAndFeel(nullptr);
}

void SettingsWindow::closeButtonPressed()
{
    setVisible(false);
}
