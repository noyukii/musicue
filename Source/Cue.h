#pragma once

#include <juce_core/juce_core.h>

struct Cue
{
    enum class Kind { audio, group };

    int id = 0;
    int parentId = 0;       // 0 means root cue list
    Kind kind = Kind::audio;
    juce::String number;
    juce::String name;
    juce::File file;
    double durationSeconds = 0.0;
    double preWait = 0.0;
    double postWait = 0.0;
    int continueMode = 0; // 0 = none, 1 = auto-continue, 2 = auto-follow
    bool armed = true;
    bool flagged = false;
    bool autoLoad = false;
    bool skipIfDisarmed = false;
    juce::String notes;
    double gainDb = 0.0;
    double pan = 0.0;        // -1 left .. +1 right
    double trimStart = 0.0;
    double trimEnd = 0.0;    // 0 = play to end of file
    double fadeIn = 0.0;
    double fadeOut = 0.0;
    bool loop = false;
    juce::String hotkey;     // KeyPress text description, empty = none
    int playCount = 0;
    bool collapsed = false;
    juce::String target = "Main Output";

    bool isGroup() const { return kind == Kind::group; }
    bool isAudio() const { return kind == Kind::audio; }

    double getEffectiveDuration() const
    {
        if (trimEnd > trimStart)
            return trimEnd - trimStart;

        return juce::jmax(0.0, durationSeconds - trimStart);
    }
};
