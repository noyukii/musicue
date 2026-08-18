#include "AudioEngine.h"

namespace
{
constexpr double timerIntervalSeconds = 0.03;
}

void AudioEngine::GainPanSource::setGainImmediate(float newGain)
{
    requestedGain.store(juce::jmax(0.0f, newGain));
    gainImmediate.store(true);
    gainVersion.fetch_add(1, std::memory_order_release);
}

void AudioEngine::GainPanSource::setGainTarget(float newGain, double rampSeconds)
{
    requestedGainRamp.store(juce::jmax(0.005, rampSeconds));
    requestedGain.store(juce::jmax(0.0f, newGain));
    gainImmediate.store(false);
    gainVersion.fetch_add(1, std::memory_order_release);
}

void AudioEngine::GainPanSource::setPan(float newPan, double rampSeconds)
{
    requestedPanRamp.store(juce::jmax(0.005, rampSeconds));
    requestedPan.store(juce::jlimit(-1.0f, 1.0f, newPan));
    panVersion.fetch_add(1, std::memory_order_release);
}

void AudioEngine::GainPanSource::prepareToPlay(int samplesPerBlockExpected, double newSampleRate)
{
    sampleRate = newSampleRate;
    gain.reset(newSampleRate, 0.02);
    pan.reset(newSampleRate, 0.02);
    gain.setCurrentAndTargetValue(requestedGain.load());
    pan.setCurrentAndTargetValue(requestedPan.load());
    currentGain.store(gain.getCurrentValue());
    currentPan.store(pan.getCurrentValue());
    appliedGainVersion = gainVersion.load();
    appliedPanVersion = panVersion.load();
    rampBuffer.setSize(2, samplesPerBlockExpected);

    if (source != nullptr)
        source->prepareToPlay(samplesPerBlockExpected, newSampleRate);
}

void AudioEngine::GainPanSource::getNextAudioBlock(const juce::AudioSourceChannelInfo& info)
{
    if (source == nullptr)
        return;

    source->getNextAudioBlock(info);

    if (info.buffer == nullptr)
        return;

    const auto latestGainVersion = gainVersion.load(std::memory_order_acquire);
    if (latestGainVersion != appliedGainVersion)
    {
        const auto newGain = requestedGain.load();
        if (gainImmediate.load())
            gain.setCurrentAndTargetValue(newGain);
        else
        {
            gain.reset(sampleRate, requestedGainRamp.load());
            gain.setTargetValue(newGain);
        }
        appliedGainVersion = latestGainVersion;
    }

    const auto latestPanVersion = panVersion.load(std::memory_order_acquire);
    if (latestPanVersion != appliedPanVersion)
    {
        pan.reset(sampleRate, requestedPanRamp.load());
        pan.setTargetValue(requestedPan.load());
        appliedPanVersion = latestPanVersion;
    }

    const auto numSamples = juce::jmin(info.numSamples, rampBuffer.getNumSamples());
    const auto numChannels = info.buffer->getNumChannels();

    if (numSamples <= 0 || numChannels <= 0)
        return;

    auto* gainRamp = rampBuffer.getWritePointer(0);
    auto* panRamp = rampBuffer.getWritePointer(1);

    for (int i = 0; i < numSamples; ++i)
    {
        gainRamp[i] = gain.getNextValue();
        panRamp[i] = pan.getNextValue();
    }

    currentGain.store(gain.getCurrentValue(), std::memory_order_relaxed);
    currentPan.store(pan.getCurrentValue(), std::memory_order_relaxed);

    for (int channel = 0; channel < numChannels; ++channel)
    {
        auto* data = info.buffer->getWritePointer(channel, info.startSample);

        for (int i = 0; i < numSamples; ++i)
        {
            auto multiplier = gainRamp[i];

            if (numChannels >= 2)
            {
                const auto angle = (panRamp[i] + 1.0f)
                                 * (juce::MathConstants<float>::pi * 0.25f);

                if (channel == 0)
                    multiplier *= std::cos(angle);
                else if (channel == 1)
                    multiplier *= std::sin(angle);
            }

            data[i] *= multiplier;
        }
    }
}

void AudioEngine::GainPanSource::releaseResources()
{
    if (source != nullptr)
        source->releaseResources();
}

AudioEngine::AudioEngine()
{
    formatManager.registerBasicFormats();

    masterGainSource.setSource(&mixer);
    deviceManager.addAudioCallback(&sourcePlayer);
    sourcePlayer.setSource(&masterGainSource);

    startTimer(static_cast<int>(timerIntervalSeconds * 1000.0));
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

bool AudioEngine::playCue(const Cue& cue, const std::function<const Cue*(int)>& resolveCue)
{
    if (cue.isAudio())
        return playAudioCue(cue);

    if (! cue.isFade() || ! resolveCue)
        return false;

    FadeRun run;
    run.cueId = cue.id;
    run.runId = nextFadeRunId++;
    run.stopPolicy = cue.fadeStopPolicy;
    run.resolveCue = resolveCue;

    for (auto action : cue.fadeActions)
    {
        const auto* target = resolveCue(action.targetCueId);
        if (target == nullptr || ! target->isAudio() || (! action.fadeGain && ! action.fadePan))
            continue;

        action.delaySeconds = juce::jmax(0.0, cue.preWait + action.delaySeconds);
        action.durationSeconds = juce::jmax(0.0, action.durationSeconds);
        action.targetGainDb = juce::jlimit(-60.0, 6.0, action.targetGainDb);
        action.targetPan = juce::jlimit(-1.0, 1.0, action.targetPan);
        action.startGainDb = juce::jlimit(-60.0, 6.0, action.startGainDb);
        if (! action.fadeGain)
            action.startIfStopped = false;

        ActiveFadeAction activeAction;
        activeAction.definition = action;
        run.actions.add(std::move(activeAction));
    }

    if (run.actions.isEmpty())
        return false;

    activeFades.add(std::move(run));
    return true;
}

bool AudioEngine::playAudioCue(const Cue& cue, std::optional<float> initialGain, bool reportStart)
{
    if (! cue.file.existsAsFile())
        return false;

    auto* reader = formatManager.createReaderFor(cue.file);
    if (reader == nullptr)
        return false;

    auto cuePlayer = std::make_unique<CuePlayer>();
    cuePlayer->cueId = cue.id;
    cuePlayer->instanceId = nextInstanceId++;
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
    cuePlayer->pendingStartSeconds = initialGain.has_value() ? 0.0 : juce::jmax(0.0, cue.preWait);
    cuePlayer->playDuration = juce::jmax(0.0, cue.getEffectiveDuration());

    mixer.addInputSource(cuePlayer->gainPan.get(), false);
    cuePlayer->gainPan->setPan(static_cast<float>(cue.pan));

    const auto authoredGain = juce::Decibels::decibelsToGain(static_cast<float>(cue.gainDb), -60.0f);
    if (initialGain.has_value())
    {
        cuePlayer->gainPan->setGainImmediate(*initialGain);
    }
    else if (cue.fadeIn > 0.0)
    {
        cuePlayer->gainPan->setGainImmediate(0.0f);
        cuePlayer->gainPan->setGainTarget(authoredGain, cue.fadeIn);
    }
    else
    {
        cuePlayer->gainPan->setGainImmediate(authoredGain);
    }

    auto* rawPlayer = cuePlayer.get();
    activeCues.add(std::move(cuePlayer));

    if (rawPlayer->pendingStartSeconds <= 0.0)
        startPlaying(*rawPlayer);

    if (reportStart && onCueStarted != nullptr)
        onCueStarted(cue.id);

    return true;
}

void AudioEngine::startPlaying(CuePlayer& cuePlayer)
{
    if (cuePlayer.trimStart > 0.0)
        cuePlayer.transport->setPosition(cuePlayer.trimStart);

    cuePlayer.transport->start();
}

AudioEngine::CuePlayer* AudioEngine::findPlayer(int instanceId) const
{
    for (auto* player : activeCues)
        if (player->instanceId == instanceId)
            return player;
    return nullptr;
}

void AudioEngine::stopPlayer(CuePlayer& cuePlayer)
{
    if (cuePlayer.stopping)
        return;

    cuePlayer.stopping = true;
    cuePlayer.gainFadeOwner = 0;
    cuePlayer.panFadeOwner = 0;
    cuePlayer.gainPan->setGainTarget(0.0f, 0.05);
}

void AudioEngine::stopCue(int cueId)
{
    for (int i = activeFades.size(); --i >= 0;)
        if (activeFades.getReference(i).cueId == cueId)
        {
            const auto run = activeFades[i];
            if (run.stopPolicy == Cue::FadeStopPolicy::stopTargets)
                for (const auto& action : run.actions)
                    for (const auto& instance : action.instances)
                        if (auto* player = findPlayer(instance.instanceId))
                            stopPlayer(*player);
            finishFade(i, false);
        }

    for (auto* cuePlayer : activeCues)
        if (cuePlayer->cueId == cueId)
            stopPlayer(*cuePlayer);
}

void AudioEngine::stopAll()
{
    for (int i = activeFades.size(); --i >= 0;)
        finishFade(i, false);

    for (auto* cuePlayer : activeCues)
        stopPlayer(*cuePlayer);
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
        if (cuePlayer->cueId != cueId || cuePlayer->stopping)
            continue;

        cuePlayer->gainFadeOwner = 0;
        cuePlayer->panFadeOwner = 0;
        cuePlayer->gainPan->setGainTarget(linearGain, 0.05);
        cuePlayer->gainPan->setPan(static_cast<float>(pan), 0.05);
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

double AudioEngine::getFadePlayhead(int cueId) const
{
    for (const auto& run : activeFades)
    {
        if (run.cueId != cueId || run.actions.isEmpty())
            continue;

        auto delay = run.actions.getReference(0).definition.delaySeconds;
        auto duration = 0.0;
        for (const auto& action : run.actions)
        {
            delay = juce::jmin(delay, action.definition.delaySeconds);
            duration = juce::jmax(duration, action.definition.durationSeconds);
        }

        if (run.elapsedSeconds < delay)
            return 0.0;
        if (duration <= 0.0)
            return 1.0;
        return juce::jlimit(0.0, 1.0, (run.elapsedSeconds - delay) / duration);
    }

    return -1.0;
}

double AudioEngine::getCuePlayhead(int cueId) const
{
    for (const auto& run : activeFades)
    {
        if (run.cueId != cueId || run.actions.isEmpty())
            continue;

        auto delay = run.actions.getReference(0).definition.delaySeconds;
        auto duration = 0.0;
        for (const auto& action : run.actions)
        {
            delay = juce::jmin(delay, action.definition.delaySeconds);
            duration = juce::jmax(duration, action.definition.durationSeconds);
        }

        if (run.elapsedSeconds < delay)
            return -1.0;
        if (duration <= 0.0)
            return 1.0;
        return juce::jlimit(0.0, 1.0, (run.elapsedSeconds - delay) / duration);
    }

    const CuePlayer* latest = nullptr;
    for (auto* cuePlayer : activeCues)
    {
        if (cuePlayer->cueId != cueId || cuePlayer->stopping)
            continue;
        if (latest == nullptr || cuePlayer->instanceId > latest->instanceId)
            latest = cuePlayer;
    }

    if (latest == nullptr || latest->pendingStartSeconds > 0.0)
        return -1.0;

    auto duration = latest->playDuration;
    if (duration <= 0.0)
    {
        const auto end = latest->trimEnd > latest->trimStart
            ? latest->trimEnd : latest->transport->getLengthInSeconds();
        duration = juce::jmax(0.0, end - latest->trimStart);
    }

    if (duration <= 0.0)
        return 1.0;

    auto elapsed = latest->transport->getCurrentPosition() - latest->trimStart;
    if (latest->loop && duration > 0.0)
        elapsed = std::fmod(juce::jmax(0.0, elapsed), duration);

    return juce::jlimit(0.0, 1.0, elapsed / duration);
}

float AudioEngine::applyFadeCurve(Cue::FadeCurve curve, float position)
{
    return Cue::applyFadeCurve(curve, position);
}

void AudioEngine::updateFades(double elapsedSeconds)
{
    for (int i = activeFades.size(); --i >= 0;)
    {
        auto& run = activeFades.getReference(i);
        run.elapsedSeconds += elapsedSeconds;

        for (auto& action : run.actions)
        {
            const auto& definition = action.definition;
            if (action.complete || run.elapsedSeconds < definition.delaySeconds)
                continue;

            if (! action.started)
            {
                action.started = true;
                bool hasTarget = false;
                for (auto* player : activeCues)
                    if (player->cueId == definition.targetCueId && ! player->stopping)
                        hasTarget = true;

                if (! hasTarget && definition.startIfStopped && run.resolveCue != nullptr)
                    if (const auto* target = run.resolveCue(definition.targetCueId))
                        playAudioCue(*target,
                                     juce::Decibels::decibelsToGain(
                                         static_cast<float>(definition.startGainDb), -60.0f), true);

                for (auto* player : activeCues)
                {
                    if (player->cueId != definition.targetCueId || player->stopping)
                        continue;

                    FadedInstance instance;
                    instance.instanceId = player->instanceId;
                    instance.startGain = player->gainPan->getCurrentGain();
                    instance.startPan = player->gainPan->getCurrentPan();
                    instance.gainActive = definition.fadeGain;
                    instance.panActive = definition.fadePan;
                    if (instance.gainActive)
                        player->gainFadeOwner = run.runId;
                    if (instance.panActive)
                        player->panFadeOwner = run.runId;
                    action.instances.add(instance);
                }

                if (action.instances.isEmpty())
                {
                    action.complete = true;
                    run.interrupted = true;
                    continue;
                }
            }

            const auto progress = definition.durationSeconds <= 0.0 ? 1.0f
                : static_cast<float>((run.elapsedSeconds - definition.delaySeconds) / definition.durationSeconds);
            const auto curvedProgress = applyFadeCurve(definition.curve, progress);
            const auto targetGain = juce::Decibels::decibelsToGain(
                static_cast<float>(definition.targetGainDb), -60.0f);

            bool hasActiveProperty = false;
            for (auto& instance : action.instances)
            {
                auto* player = findPlayer(instance.instanceId);
                if (player == nullptr || player->stopping)
                {
                    instance.gainActive = false;
                    instance.panActive = false;
                    run.interrupted = true;
                    continue;
                }

                if (instance.gainActive && player->gainFadeOwner != run.runId)
                {
                    instance.gainActive = false;
                    run.interrupted = true;
                }
                if (instance.panActive && player->panFadeOwner != run.runId)
                {
                    instance.panActive = false;
                    run.interrupted = true;
                }

                if (instance.gainActive)
                {
                    const auto gain = instance.startGain + (targetGain - instance.startGain) * curvedProgress;
                    player->gainPan->setGainTarget(gain, timerIntervalSeconds + 0.005);
                    hasActiveProperty = true;
                }
                if (instance.panActive)
                {
                    const auto pan = instance.startPan
                                   + (static_cast<float>(definition.targetPan) - instance.startPan) * curvedProgress;
                    player->gainPan->setPan(pan, timerIntervalSeconds + 0.005);
                    hasActiveProperty = true;
                }
            }

            if (! hasActiveProperty || progress >= 1.0f)
            {
                if (progress >= 1.0f && definition.stopAtEnd && ! run.interrupted)
                    for (const auto& instance : action.instances)
                        if (auto* player = findPlayer(instance.instanceId))
                            stopPlayer(*player);
                action.complete = true;
            }
        }

        bool complete = true;
        for (const auto& action : run.actions)
            complete = complete && action.complete;
        if (complete)
            finishFade(i, ! run.interrupted);
    }
}

void AudioEngine::finishFade(int index, bool completedNaturally)
{
    if (! juce::isPositiveAndBelow(index, activeFades.size()))
        return;
    const auto cueId = activeFades[index].cueId;
    activeFades.remove(index);
    if (onCueFinished != nullptr)
        onCueFinished(cueId, completedNaturally);
}

void AudioEngine::timerCallback()
{
    if (! paused)
        updateFades(timerIntervalSeconds);

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
            cuePlayer->pendingStartSeconds -= timerIntervalSeconds;
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
                    ? cuePlayer->trimEnd : cuePlayer->transport->getLengthInSeconds();
                if (end > 0.0 && position >= end - cuePlayer->fadeOut)
                {
                    cuePlayer->fadeOutApplied = true;
                    cuePlayer->gainFadeOwner = 0;
                    cuePlayer->gainPan->setGainTarget(0.0f, cuePlayer->fadeOut);
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
