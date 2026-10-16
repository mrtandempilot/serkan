#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include "Shapes.h"

class PulseDuckProcessor : public juce::AudioProcessor
{
public:
    PulseDuckProcessor();
    ~PulseDuckProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

    // --- shared with the UI ---
    static constexpr int historySize = 400;
    std::array<std::atomic<float>, historySize> inputHistory;
    std::atomic<float> uiPhase { 0.0f };
    std::atomic<bool>  uiActive { true };

    enum Mode { modeSync = 0, modeMidi, modeAudio };
    static double beatsForRate (int rateIndex);

private:
    void trigger() { phase = 0.0; running = true; }

    std::atomic<float>* rateP = nullptr;
    std::atomic<float>* modeP = nullptr;
    std::atomic<float>* shapeP = nullptr;
    std::atomic<float>* mixP = nullptr;
    std::atomic<float>* bandP = nullptr;
    std::atomic<float>* bandFreqP = nullptr;
    std::atomic<float>* listenP = nullptr;

    double sr = 44100.0;
    double lastBpm = 120.0;
    double phase = 1.0;
    bool running = false;
    float smoothedGain = 1.0f;
    float smoothCoef = 0.1f;

    // audio-trigger detector
    float envFast = 0.0f, envSlow = 0.0f;
    float fastRel = 0.0f, slowAtt = 0.0f, slowRel = 0.0f;
    int cooldown = 0, cooldownSamples = 0;

    // band split
    juce::dsp::LinkwitzRileyFilter<float> lowpass, highpass;
    float currentCutoff = -1.0f;

    std::vector<float> gains;
    int lastBin = -1;
    float binPeak = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PulseDuckProcessor)
};
