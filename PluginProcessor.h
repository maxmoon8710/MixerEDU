#pragma once
#include <JuceHeader.h>
#include <array>

// Holds the memory and state variables for each individual band
struct BandState {
    juce::AudioBuffer<float> delayBuffer;
    int delayWritePosition{ 0 };
    float smoothedGain{ 1.0f };
    float inputEnvelope{ 0.0f };
    std::atomic<float> visPeak{ 0.0f };
    std::atomic<float> visGain{ 1.0f };
    std::atomic<float> visInputPeak{ 0.0f };
};

// Holds fast, direct pointers to the atomic parameters for real-time processing
struct BandParams {
    std::atomic<float>* sat_bypass{ nullptr };
    std::atomic<float>* sat_influence{ nullptr };
    std::atomic<float>* sat_type{ nullptr };
    std::atomic<float>* sc_bypass{ nullptr };
    std::atomic<float>* sc_thresh{ nullptr };
    std::atomic<float>* sc_postgain{ nullptr };
    std::atomic<float>* hc_bypass{ nullptr };
    std::atomic<float>* hc_ceiling{ nullptr };
    std::atomic<float>* lim_bypass{ nullptr };
    std::atomic<float>* lim_attack{ nullptr };
    std::atomic<float>* lim_release{ nullptr };
    std::atomic<float>* lim_ceiling{ nullptr };
    std::atomic<float>* solo{ nullptr };
};

class MixerEDUAudioProcessor : public juce::AudioProcessor
{
public:
    MixerEDUAudioProcessor();
    ~MixerEDUAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int index) override {}
    const juce::String getProgramName(int index) override { return {}; }
    void changeProgramName(int index, const juce::String& newName) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> currentRMSDb{ -80.0f };

    // Multi-band States & Params (0=Low, 1=Mid, 2=High, 3=Master)
    BandState bands[4];
    BandParams bandParams[4];

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Abstracted DSP processing chain
    void processStage(juce::AudioBuffer<float>& buffer, int bandIndex, double sr);

    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> lowBuffer, midBuffer, highBuffer;

    juce::dsp::LinkwitzRileyFilter<float> lp1, hp1, lp2, hp2;

    std::atomic<float>* cross_low_mid{ nullptr };
    std::atomic<float>* cross_mid_high{ nullptr };
    std::atomic<float>* delta_listen{ nullptr };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixerEDUAudioProcessor)
};