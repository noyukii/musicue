#pragma once

#include <juce_events/juce_events.h>

/** Checks GitHub for a newer MusiCue release on a background thread and
    reports the result back on the message thread. */
class UpdateChecker : private juce::Thread
{
public:
    struct Result
    {
        bool updateAvailable = false;
        juce::String latestVersion;
    };

    UpdateChecker();
    ~UpdateChecker() override;

    void checkForUpdates(juce::String currentVersion, std::function<void(Result)> onComplete);

    static bool isNewRelease(const juce::String& latestTag, const juce::String& currentVersion);

private:
    void run() override;

    juce::String currentVersion;
    std::function<void(Result)> completion;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UpdateChecker)
};
