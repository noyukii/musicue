#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include "AppSettings.h"
#include "MusiCueLookAndFeel.h"

class SettingsWindow : public juce::DocumentWindow
{
public:
    SettingsWindow(juce::AudioDeviceManager& deviceManager,
                   AppSettings& settings,
                   juce::PropertiesFile& props,
                   std::function<void()> onSettingsChanged);
    ~SettingsWindow() override;

    void closeButtonPressed() override;

private:
    MusiCueLookAndFeel lookAndFeel;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsWindow)
};
