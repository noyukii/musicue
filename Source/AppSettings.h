#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include "Shortcuts.h"

struct AppSettings
{
    float masterGain = 0.8f;
    bool standbyLinked = true;
    int defaultContinueMode = 0; // 0 = none, 1 = auto-continue, 2 = auto-follow
    ShortcutBindings shortcuts;

    void load(const juce::PropertiesFile& props)
    {
        masterGain = static_cast<float>(props.getDoubleValue("masterGain", 0.8));
        standbyLinked = props.getBoolValue("standbyLinked", true);
        defaultContinueMode = props.getIntValue("defaultContinueMode", 0);
        shortcuts.load(props);
    }

    void save(juce::PropertiesFile& props) const
    {
        props.setValue("masterGain", masterGain);
        props.setValue("standbyLinked", standbyLinked);
        props.setValue("defaultContinueMode", defaultContinueMode);
        shortcuts.save(props);
        props.saveIfNeeded();
    }
};
