#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <atomic>
#include <cstdint>
#include <optional>
#include "Cue.h"

class AudioEngine : private juce::Timer
{
public:
    AudioEngine();
    ~AudioEngine() override;

    void initialise(const juce::String& savedStateXml);
    juce::String getAudioStateXml() const;
    juce::AudioDeviceManager& getDeviceManager() { return deviceManager; }
    juce::String getCurrentDeviceName() const;

    bool playCue(const Cue& cue, const std::function<const Cue*(int)>& resolveCue = {});
    void stopCue(int cueId);
    void stopAll();
    void setPaused(bool shouldPause);
    bool isPaused() const { return paused; }
    void updateCueParameters(int cueId, double gainDb, double pan);
    void setMasterGain(float newGain);
    double getDurationForFile(const juce::File& file);

    std::function<void(int cueId, bool completedNaturally)> onCueFinished;
    std::function<void(int cueId)> onCueStarted;

private:
    class GainSource : public juce::AudioSource
    {
    public:
        void setSource(juce::AudioSource* newSource) noexcept { source = newSource; }
        void setGain(float newGain) { gain.setTargetValue(newGain); }

        void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override
        {
            gain.reset(sampleRate, 0.05);
            if (source != nullptr)
                source->prepareToPlay(samplesPerBlockExpected, sampleRate);
        }

        void getNextAudioBlock(const juce::AudioSourceChannelInfo& info) override
        {
            if (source == nullptr)
                return;

            source->getNextAudioBlock(info);

            if (info.buffer != nullptr)
                for (int channel = 0; channel < info.buffer->getNumChannels(); ++channel)
                    gain.applyGain(info.buffer->getWritePointer(channel, info.startSample),
                                   info.numSamples);
        }

        void releaseResources() override
        {
            if (source != nullptr)
                source->releaseResources();
        }

    private:
        juce::AudioSource* source = nullptr;
        juce::LinearSmoothedValue<float> gain { 0.8f };
    };

    class GainPanSource : public juce::AudioSource
    {
    public:
        void setSource(juce::AudioSource* newSource) noexcept { source = newSource; }

        void setGainImmediate(float newGain);
        void setGainTarget(float newGain, double rampSeconds = 0.02);
        void setPan(float newPan, double rampSeconds = 0.02);
        float getCurrentGain() const { return currentGain.load(); }
        float getCurrentPan() const { return currentPan.load(); }

        void prepareToPlay(int samplesPerBlockExpected, double newSampleRate) override;
        void getNextAudioBlock(const juce::AudioSourceChannelInfo& info) override;
        void releaseResources() override;

    private:
        juce::AudioSource* source = nullptr;
        juce::LinearSmoothedValue<float> gain { 1.0f };
        juce::LinearSmoothedValue<float> pan { 0.0f };
        juce::AudioBuffer<float> rampBuffer { 2, 0 };
        double sampleRate = 44100.0;
        std::atomic<float> requestedGain { 1.0f }, requestedPan { 0.0f };
        std::atomic<double> requestedGainRamp { 0.02 }, requestedPanRamp { 0.02 };
        std::atomic<unsigned> gainVersion { 0 }, panVersion { 0 };
        std::atomic<bool> gainImmediate { false };
        unsigned appliedGainVersion = 0, appliedPanVersion = 0;
        std::atomic<float> currentGain { 1.0f }, currentPan { 0.0f };
    };

    struct CuePlayer
    {
        int cueId = 0;
        int instanceId = 0;
        std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
        std::unique_ptr<juce::AudioTransportSource> transport;
        std::unique_ptr<GainPanSource> gainPan;
        double trimStart = 0.0;
        double trimEnd = 0.0;
        double fadeOut = 0.0;
        double pendingStartSeconds = 0.0;
        bool loop = false;
        bool stopping = false;
        bool paused = false;
        bool fadeOutApplied = false;
        std::uint64_t gainFadeOwner = 0;
        std::uint64_t panFadeOwner = 0;
    };

    struct FadedInstance
    {
        int instanceId = 0;
        float startGain = 0.0f;
        float startPan = 0.0f;
        bool gainActive = false;
        bool panActive = false;
    };

    struct ActiveFadeAction
    {
        Cue::FadeAction definition;
        bool started = false;
        bool complete = false;
        juce::Array<FadedInstance> instances;
    };

    struct FadeRun
    {
        int cueId = 0;
        std::uint64_t runId = 0;
        Cue::FadeStopPolicy stopPolicy = Cue::FadeStopPolicy::hold;
        double elapsedSeconds = 0.0;
        bool interrupted = false;
        juce::Array<ActiveFadeAction> actions;
        std::function<const Cue*(int)> resolveCue;
    };

    void startPlaying(CuePlayer& cuePlayer);
    bool playAudioCue(const Cue& cue, std::optional<float> initialGain = std::nullopt,
                      bool reportStart = false);
    CuePlayer* findPlayer(int instanceId) const;
    void stopPlayer(CuePlayer& cuePlayer);
    void updateFades(double elapsedSeconds);
    void finishFade(int index, bool completedNaturally);
    static float applyFadeCurve(Cue::FadeCurve curve, float position);
    void timerCallback() override;

    juce::AudioFormatManager formatManager;
    juce::AudioDeviceManager deviceManager;
    juce::AudioSourcePlayer sourcePlayer;
    juce::MixerAudioSource mixer;
    GainSource masterGainSource;
    juce::OwnedArray<CuePlayer> activeCues;
    juce::Array<FadeRun> activeFades;
    int nextInstanceId = 1;
    std::uint64_t nextFadeRunId = 1;
    bool paused = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};
