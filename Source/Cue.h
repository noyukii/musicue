#pragma once

#include <cmath>
#include <juce_core/juce_core.h>

struct Cue
{
    enum class Kind { audio, group, fade };
    enum class GroupMode { timeline, playlist, startFirstAndEnter, startFirst, startRandom };
    enum class FadeCurve
    {
        linear = 0,
        easeIn,
        easeOut,
        sCurve,
        inverseSCurve,
        exponential,
        logarithmic,
        equalPower
    };
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

    static constexpr int numFadeCurves = static_cast<int>(FadeCurve::equalPower) + 1;

    static FadeCurve fadeCurveFromInt(int value)
    {
        return static_cast<FadeCurve>(juce::jlimit(0, numFadeCurves - 1, value));
    }

    static juce::String fadeCurveName(FadeCurve curve)
    {
        switch (curve)
        {
            case FadeCurve::easeIn:        return "Ease in";
            case FadeCurve::easeOut:       return "Ease out";
            case FadeCurve::sCurve:        return "S-curve";
            case FadeCurve::inverseSCurve: return "Inverse S-curve";
            case FadeCurve::exponential:   return "Exponential";
            case FadeCurve::logarithmic:   return "Logarithmic";
            case FadeCurve::equalPower:    return "Equal power";
            case FadeCurve::linear:        break;
        }
        return "Linear";
    }

    static float applyFadeCurve(FadeCurve curve, float position)
    {
        const auto t = juce::jlimit(0.0f, 1.0f, position);
        switch (curve)
        {
            case FadeCurve::easeIn:
                return t * t;
            case FadeCurve::easeOut:
                return 1.0f - (1.0f - t) * (1.0f - t);
            case FadeCurve::sCurve:
                return t * t * (3.0f - 2.0f * t);
            case FadeCurve::inverseSCurve:
                if (t < 0.5f)
                {
                    const auto u = t * 2.0f;
                    return 0.5f * (1.0f - (1.0f - u) * (1.0f - u));
                }
                {
                    const auto u = t * 2.0f - 1.0f;
                    return 0.5f + 0.5f * u * u;
                }
            case FadeCurve::exponential:
            {
                constexpr auto k = 4.0f;
                return (std::exp(k * t) - 1.0f) / (std::exp(k) - 1.0f);
            }
            case FadeCurve::logarithmic:
            {
                constexpr auto k = 4.0f;
                return std::log1p(t * (std::exp(k) - 1.0f)) / k;
            }
            case FadeCurve::equalPower:
                return std::sin(t * juce::MathConstants<float>::halfPi);
            case FadeCurve::linear:
                break;
        }
        return t;
    }

    static int nextAudioCueId(const juce::Array<Cue>& cues, int afterId)
    {
        auto seen = false;
        for (const auto& cue : cues)
        {
            if (! seen)
            {
                if (cue.id == afterId)
                    seen = true;
                continue;
            }

            if (cue.isAudio())
                return cue.id;
        }
        return 0;
    }

    // Canonical song-to-song transition: fade the old song out and stop it,
    // fade the new song in from silence (starting it if needed).
    static juce::Array<FadeAction> makeCrossfadeActions(int fromCueId, int toCueId,
                                                        double durationSeconds = 3.0,
                                                        FadeCurve curve = FadeCurve::sCurve)
    {
        juce::Array<FadeAction> actions;

        if (fromCueId > 0)
        {
            FadeAction fadeOut;
            fadeOut.targetCueId = fromCueId;
            fadeOut.durationSeconds = durationSeconds;
            fadeOut.targetGainDb = -60.0;
            fadeOut.stopAtEnd = true;
            fadeOut.curve = curve;
            actions.add(fadeOut);
        }

        if (toCueId > 0)
        {
            FadeAction fadeIn;
            fadeIn.targetCueId = toCueId;
            fadeIn.durationSeconds = durationSeconds;
            fadeIn.targetGainDb = 0.0;
            fadeIn.startIfStopped = true;
            fadeIn.startGainDb = -60.0;
            fadeIn.curve = curve;
            actions.add(fadeIn);
        }

        return actions;
    }

    static juce::String makeFadeName(const juce::Array<Cue>& cues, int fromId, int toId)
    {
        auto nameOf = [&cues](int cueId) -> juce::String
        {
            for (const auto& cue : cues)
                if (cue.id == cueId)
                    return cue.name;
            return {};
        };

        if (toId > 0)
            return "Crossfade to " + nameOf(toId);
        if (fromId > 0)
            return "Fade out " + nameOf(fromId);
        return "(Untitled Fade Cue)";
    }

    struct FadeSetup
    {
        int fromCueId = 0;
        int toCueId = 0;
        bool toNextInLine = false;
        double delaySeconds = 0.0;
        double durationSeconds = 3.0;
        FadeCurve curve = FadeCurve::sCurve;
        FadeStopPolicy stopPolicy = FadeStopPolicy::hold;
        bool fadeGain = true;
        double targetGainDb = -60.0;
        bool fadePan = false;
        double targetPan = 0.0;
        bool stopAtEnd = true;
        double startGainDb = -60.0;

        juce::Array<FadeAction> toActions(int resolvedFromId, int resolvedToId) const
        {
            auto actions = Cue::makeCrossfadeActions(resolvedFromId, resolvedToId, durationSeconds, curve);
            for (auto& action : actions)
            {
                action.delaySeconds = delaySeconds;
                if (action.startIfStopped)
                {
                    action.startIfStopped = true;
                    action.startGainDb = startGainDb;
                }
                else
                {
                    action.fadeGain = fadeGain;
                    action.targetGainDb = targetGainDb;
                    action.fadePan = fadePan;
                    action.targetPan = targetPan;
                    action.stopAtEnd = stopAtEnd;
                }
            }
            return actions;
        }

        static FadeSetup fromActions(const juce::Array<FadeAction>& actions, FadeStopPolicy policy)
        {
            FadeSetup setup;
            setup.stopPolicy = policy;
            if (actions.isEmpty())
                return setup;

            const FadeAction* outgoing = nullptr;
            const FadeAction* incoming = nullptr;
            for (const auto& action : actions)
            {
                if (action.startIfStopped)
                    incoming = &action;
                else if (outgoing == nullptr)
                    outgoing = &action;
            }

            if (outgoing == nullptr && incoming == nullptr)
                outgoing = &actions.getReference(0);
            else if (outgoing == nullptr && actions.size() > 1)
                outgoing = &actions.getReference(0) == incoming ? &actions.getReference(1)
                                                                : &actions.getReference(0);

            if (outgoing != nullptr)
            {
                setup.fromCueId = outgoing->targetCueId;
                setup.delaySeconds = outgoing->delaySeconds;
                setup.durationSeconds = outgoing->durationSeconds;
                setup.curve = outgoing->curve;
                setup.fadeGain = outgoing->fadeGain;
                setup.targetGainDb = outgoing->targetGainDb;
                setup.fadePan = outgoing->fadePan;
                setup.targetPan = outgoing->targetPan;
                setup.stopAtEnd = outgoing->stopAtEnd;
            }

            if (incoming != nullptr)
            {
                setup.toCueId = incoming->targetCueId;
                setup.startGainDb = incoming->startGainDb;
                if (outgoing == nullptr)
                {
                    setup.delaySeconds = incoming->delaySeconds;
                    setup.durationSeconds = incoming->durationSeconds;
                    setup.curve = incoming->curve;
                    setup.fadeGain = incoming->fadeGain;
                }
            }

            return setup;
        }
    };

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
