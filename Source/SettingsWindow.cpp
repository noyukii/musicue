#include "SettingsWindow.h"
#include "Palette.h"

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
            gainSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 22);
            gainSlider.setRange(0.0, 1.0, 0.01);
            gainSlider.setValue(settings.masterGain, juce::dontSendNotification);
            gainSlider.setColour(juce::Slider::trackColourId, juce::Colour(0xff777777));
            gainSlider.setColour(juce::Slider::thumbColourId, Palette::textPrimary);
            gainSlider.onValueChange = [this]
            {
                settings.masterGain = static_cast<float>(gainSlider.getValue());
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

    class ControlsTab : public juce::Component
    {
    public:
        ControlsTab()
        {
            title.setText("Keyboard controls", juce::dontSendNotification);
            title.setFont(juce::Font(juce::FontOptions().withHeight(17.0f)));
            title.setColour(juce::Label::textColourId, Palette::textPrimary);
            detail.setText("Space     GO standby cue\nEscape    Panic\nCommand-S    Save workspace\nCommand-P    Preview selected cue\nCue hotkeys trigger their assigned cue independently.", juce::dontSendNotification);
            detail.setFont(juce::Font(juce::FontOptions().withHeight(13.0f)));
            detail.setColour(juce::Label::textColourId, Palette::textDim);
            detail.setJustificationType(juce::Justification::topLeft);
            addAndMakeVisible(title); addAndMakeVisible(detail);
        }
        void paint(juce::Graphics& g) override { g.fillAll(Palette::panelBg); }
        void resized() override { auto r = getLocalBounds().reduced(28, 24); title.setBounds(r.removeFromTop(28)); r.removeFromTop(14); detail.setBounds(r); }
    private:
        juce::Label title, detail;
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
            playbackTab = std::make_unique<PlaybackTab>(settings, props, std::move(notify));
            controlsTab = std::make_unique<ControlsTab>();

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
    setResizable(false, false);
    centreWithSize(560, 360);
}

SettingsWindow::~SettingsWindow()
{
    setLookAndFeel(nullptr);
}

void SettingsWindow::closeButtonPressed()
{
    setVisible(false);
}
