#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
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

    bool playCue(const Cue& cue);
    void stopCue(int cueId);
    void stopAll();
    void setPaused(bool shouldPause);
    bool isPaused() const { return paused; }
    void updateCueParameters(int cueId, double gainDb, double pan);
    void setMasterGain(float newGain);
    double getDurationForFile(const juce::File& file);

    std::function<void(int cueId, bool completedNaturally)> onCueFinished;

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

        void setGainImmediate(float newGain) { gain.setCurrentAndTargetValue(newGain); }
        void setGainTarget(float newGain) { gain.setTargetValue(newGain); }
        float getCurrentGain() const { return gain.getCurrentValue(); }

        void setRampSeconds(double seconds)
        {
            if (sampleRate > 0.0)
                gain.reset(sampleRate, juce::jmax(0.005, seconds));
        }

        void setPan(float newPan) { pan.setTargetValue(juce::jlimit(-1.0f, 1.0f, newPan)); }

        void prepareToPlay(int samplesPerBlockExpected, double newSampleRate) override
        {
            sampleRate = newSampleRate;
            gain.reset(newSampleRate, 0.02);
            pan.reset(newSampleRate, 0.02);
            rampBuffer.setSize(2, samplesPerBlockExpected);

            if (source != nullptr)
                source->prepareToPlay(samplesPerBlockExpected, newSampleRate);
        }

        void getNextAudioBlock(const juce::AudioSourceChannelInfo& info) override
        {
            if (source == nullptr)
                return;

            source->getNextAudioBlock(info);

            if (info.buffer == nullptr)
                return;

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

        void releaseResources() override
        {
            if (source != nullptr)
                source->releaseResources();
        }

    private:
        juce::AudioSource* source = nullptr;
        juce::LinearSmoothedValue<float> gain { 1.0f };
        juce::LinearSmoothedValue<float> pan { 0.0f };
        juce::AudioBuffer<float> rampBuffer { 2, 0 };
        double sampleRate = 44100.0;
    };

    struct CuePlayer
    {
        int cueId = 0;
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
    };

    void startPlaying(CuePlayer& cuePlayer);
    void timerCallback() override;

    juce::AudioFormatManager formatManager;
    juce::AudioDeviceManager deviceManager;
    juce::AudioSourcePlayer sourcePlayer;
    juce::MixerAudioSource mixer;
    GainSource masterGainSource;
    juce::OwnedArray<CuePlayer> activeCues;
    bool paused = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};
