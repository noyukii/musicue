#include "AudioEngine.h"

AudioEngine::AudioEngine()
{
    formatManager.registerBasicFormats();

    masterGainSource.setSource(&mixer);
    deviceManager.addAudioCallback(&sourcePlayer);
    sourcePlayer.setSource(&masterGainSource);

    startTimer(30);
}

AudioEngine::~AudioEngine()
{
    stopTimer();
    deviceManager.removeAudioCallback(&sourcePlayer);
    sourcePlayer.setSource(nullptr);
    mixer.removeAllInputs();
    activeCues.clear();
}

void AudioEngine::initialise(const juce::String& savedStateXml)
{
    juce::String error = "no saved state";

    if (auto xml = juce::XmlDocument::parse(savedStateXml))
        error = deviceManager.initialise(0, 2, xml.get(), true);

    if (error.isNotEmpty())
        deviceManager.initialiseWithDefaultDevices(0, 2);
}

juce::String AudioEngine::getAudioStateXml() const
{
    if (auto xml = deviceManager.createStateXml())
        return xml->toString();

    return {};
}

juce::String AudioEngine::getCurrentDeviceName() const
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
        return device->getName();

    return "No output device";
}

bool AudioEngine::playCue(const Cue& cue)
{
    if (! cue.file.existsAsFile())
        return false;

    auto* reader = formatManager.createReaderFor(cue.file);

    if (reader == nullptr)
        return false;

    auto cuePlayer = std::make_unique<CuePlayer>();
    cuePlayer->cueId = cue.id;
    cuePlayer->readerSource = std::make_unique<juce::AudioFormatReaderSource>(reader, true);
    cuePlayer->readerSource->setLooping(cue.loop);
    cuePlayer->transport = std::make_unique<juce::AudioTransportSource>();
    cuePlayer->transport->setSource(cuePlayer->readerSource.get(), 0, nullptr, reader->sampleRate);
    cuePlayer->gainPan = std::make_unique<GainPanSource>();
    cuePlayer->gainPan->setSource(cuePlayer->transport.get());

    cuePlayer->trimStart = juce::jmax(0.0, cue.trimStart);
    cuePlayer->trimEnd = cue.trimEnd;
    cuePlayer->fadeOut = juce::jmax(0.0, cue.fadeOut);
    cuePlayer->loop = cue.loop;
    cuePlayer->pendingStartSeconds = juce::jmax(0.0, cue.preWait);

    mixer.addInputSource(cuePlayer->gainPan.get(), false);

    cuePlayer->gainPan->setPan(static_cast<float>(cue.pan));

    const auto linearGain = juce::Decibels::decibelsToGain(static_cast<float>(cue.gainDb), -60.0f);

    if (cue.fadeIn > 0.0)
    {
        cuePlayer->gainPan->setGainImmediate(0.0f);
        cuePlayer->gainPan->setRampSeconds(cue.fadeIn);
        cuePlayer->gainPan->setGainTarget(linearGain);
    }
    else
    {
        cuePlayer->gainPan->setGainImmediate(linearGain);
    }

    auto* rawPlayer = cuePlayer.get();
    activeCues.add(std::move(cuePlayer));

    if (rawPlayer->pendingStartSeconds <= 0.0)
        startPlaying(*rawPlayer);

    return true;
}

void AudioEngine::startPlaying(CuePlayer& cuePlayer)
{
    if (cuePlayer.trimStart > 0.0)
        cuePlayer.transport->setPosition(cuePlayer.trimStart);

    cuePlayer.transport->start();
}

void AudioEngine::stopCue(int cueId)
{
    for (auto* cuePlayer : activeCues)
    {
        if (cuePlayer->cueId == cueId && ! cuePlayer->stopping)
        {
            cuePlayer->stopping = true;
            cuePlayer->gainPan->setRampSeconds(0.05);
            cuePlayer->gainPan->setGainTarget(0.0f);
        }
    }
}

void AudioEngine::stopAll()
{
    for (auto* cuePlayer : activeCues)
    {
        if (! cuePlayer->stopping)
        {
            cuePlayer->stopping = true;
            cuePlayer->gainPan->setRampSeconds(0.05);
            cuePlayer->gainPan->setGainTarget(0.0f);
        }
    }
}

void AudioEngine::setPaused(bool shouldPause)
{
    paused = shouldPause;

    for (auto* cuePlayer : activeCues)
    {
        if (cuePlayer->stopping)
            continue;

        cuePlayer->paused = shouldPause;

        if (shouldPause)
            cuePlayer->transport->stop();
        else if (cuePlayer->pendingStartSeconds <= 0.0)
            cuePlayer->transport->start();
    }
}

void AudioEngine::updateCueParameters(int cueId, double gainDb, double pan)
{
    const auto linearGain = juce::Decibels::decibelsToGain(static_cast<float>(gainDb), -60.0f);

    for (auto* cuePlayer : activeCues)
    {
        if (cuePlayer->cueId == cueId && ! cuePlayer->stopping)
        {
            cuePlayer->gainPan->setRampSeconds(0.05);
            cuePlayer->gainPan->setGainTarget(linearGain);
            cuePlayer->gainPan->setPan(static_cast<float>(pan));
        }
    }
}

void AudioEngine::setMasterGain(float newGain)
{
    masterGainSource.setGain(newGain);
}

double AudioEngine::getDurationForFile(const juce::File& file)
{
    if (auto* reader = formatManager.createReaderFor(file))
        return static_cast<double>(reader->lengthInSamples) / reader->sampleRate;

    return 0.0;
}

void AudioEngine::timerCallback()
{
    for (int i = activeCues.size(); --i >= 0;)
    {
        auto* cuePlayer = activeCues[i];

        if (cuePlayer->stopping)
        {
            if (cuePlayer->gainPan->getCurrentGain() <= 0.001f)
            {
                const auto id = cuePlayer->cueId;
                mixer.removeInputSource(cuePlayer->gainPan.get());
                activeCues.removeObject(cuePlayer);

                if (onCueFinished != nullptr)
                    onCueFinished(id, false);
            }

            continue;
        }

        if (cuePlayer->pendingStartSeconds > 0.0)
        {
            if (cuePlayer->paused)
                continue;

            cuePlayer->pendingStartSeconds -= 0.03;

            if (cuePlayer->pendingStartSeconds <= 0.0)
                startPlaying(*cuePlayer);

            continue;
        }

        const auto position = cuePlayer->transport->getCurrentPosition();

        if (cuePlayer->paused)
            continue;

        if (cuePlayer->loop && cuePlayer->trimEnd > cuePlayer->trimStart
            && position >= cuePlayer->trimEnd)
            cuePlayer->transport->setPosition(cuePlayer->trimStart);

        if (! cuePlayer->loop)
        {
            if (! cuePlayer->fadeOutApplied && cuePlayer->fadeOut > 0.0)
            {
                const auto end = cuePlayer->trimEnd > cuePlayer->trimStart
                                     ? cuePlayer->trimEnd
                                     : cuePlayer->transport->getLengthInSeconds();

                if (end > 0.0 && position >= end - cuePlayer->fadeOut)
                {
                    cuePlayer->fadeOutApplied = true;
                    cuePlayer->gainPan->setRampSeconds(cuePlayer->fadeOut);
                    cuePlayer->gainPan->setGainTarget(0.0f);
                }
            }

            const auto reachedTrimEnd = cuePlayer->trimEnd > cuePlayer->trimStart
                                     && position >= cuePlayer->trimEnd;

            if (reachedTrimEnd || cuePlayer->transport->hasStreamFinished())
            {
                const auto id = cuePlayer->cueId;
                mixer.removeInputSource(cuePlayer->gainPan.get());
                activeCues.removeObject(cuePlayer);

                if (onCueFinished != nullptr)
                    onCueFinished(id, true);
            }
        }
    }
}
