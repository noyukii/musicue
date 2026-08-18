#pragma once

#include <juce_core/juce_core.h>

struct Cue
{
    enum class Kind { audio, group, fade };
    enum class GroupMode { timeline, playlist, startFirstAndEnter, startFirst, startRandom };
    enum class FadeCurve { linear, easeIn, easeOut, sCurve };
    enum class FadeStopPolicy { hold, stopTargets };

    struct FadeAction
    {
        int targetCueId = 0;
        double delaySeconds = 0.0;
        double durationSeconds = 1.0;
        bool fadeGain = true;
        double targetGainDb = -60.0;
        bool fadePan = false;
        double targetPan = 0.0;
        bool startIfStopped = false;
        double startGainDb = -60.0;
        bool stopAtEnd = false;
        FadeCurve curve = FadeCurve::linear;
    };

    int id = 0;
    int parentId = 0;       // 0 means root cue list
    Kind kind = Kind::audio;
    GroupMode groupMode = GroupMode::timeline;
    FadeStopPolicy fadeStopPolicy = FadeStopPolicy::hold;
    juce::Array<FadeAction> fadeActions;
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
    bool isFade() const { return kind == Kind::fade; }

    double getEffectiveDuration() const
    {
        if (isFade())
        {
            double duration = 0.0;
            for (const auto& action : fadeActions)
                duration = juce::jmax(duration, action.delaySeconds + action.durationSeconds);
            return duration;
        }

        if (trimEnd > trimStart)
            return trimEnd - trimStart;

        return juce::jmax(0.0, durationSeconds - trimStart);
    }
};
