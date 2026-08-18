#include "UpdateChecker.h"

namespace
{
    constexpr const char* latestReleaseUrl = "https://api.github.com/repos/noyukii/musicue/releases/latest";
}

UpdateChecker::UpdateChecker()
    : juce::Thread("MusiCue Update Checker")
{
}

UpdateChecker::~UpdateChecker()
{
    stopThread(3000);
}

void UpdateChecker::checkForUpdates(juce::String current, std::function<void(Result)> onComplete)
{
    if (isThreadRunning())
        return;

    currentVersion = std::move(current);
    completion = std::move(onComplete);
    startThread();
}

bool UpdateChecker::isNewRelease(const juce::String& latestTag, const juce::String& currentVersion)
{
    const auto normalise = [](juce::String version) { return version.trim().trimCharactersAtStart("vV"); };
    return ! normalise(latestTag).equalsIgnoreCase(normalise(currentVersion));
}

void UpdateChecker::run()
{
    Result result;

    const auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                             .withExtraHeaders("User-Agent: MusiCue\r\nAccept: application/vnd.github+json")
                             .withConnectionTimeoutMs(5000);

    if (auto stream = juce::URL(latestReleaseUrl).createInputStream(options))
    {
        const auto json = juce::JSON::parse(stream->readEntireStreamAsString());

        if (auto* object = json.getDynamicObject())
        {
            const auto tag = object->getProperty("tag_name").toString();

            if (isNewRelease(tag, currentVersion))
            {
                result.updateAvailable = true;
                result.latestVersion = tag.trimCharactersAtStart("vV");
            }
        }
    }

    if (! threadShouldExit() && completion != nullptr)
    {
        auto onComplete = completion;
        juce::MessageManager::callAsync([onComplete, result] { onComplete(result); });
    }
}
