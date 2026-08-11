#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <vector>

class MixerEDUAudioProcessorEditor : public juce::AudioProcessorEditor, public juce::Timer
{
public:
    MixerEDUAudioProcessorEditor(MixerEDUAudioProcessor&);
    ~MixerEDUAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    void styleKnob(juce::Slider& s, juce::Label& l, const juce::String& name, const juce::String& suffix);
    void styleCrossoverKnob(juce::Slider& s, juce::Label& l, const juce::String& name, const juce::String& suffix);
    void updateHoverText();
    void drawTransferFunction(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawEnvelopeScope(juce::Graphics& g, juce::Rectangle<int> bounds);
    void setBand(int index);

    // Multi-band Selection Controls
    juce::TextButton btnLow{ "LOW" }, btnMid{ "MID" }, btnHigh{ "HIGH" }, btnMaster{ "MASTER" };
    juce::ToggleButton btnSolo{ "SOLO BAND" };

    // Crossover Sliders
    juce::Slider crossLowMidSlider, crossMidHighSlider;
    juce::Label crossLowMidLabel, crossMidHighLabel;

    // Standard Controls
    juce::ToggleButton satBypass{ "Bypass" }, scBypass{ "Bypass" }, hcBypass{ "Bypass" }, limBypass{ "Bypass" };
    juce::ToggleButton deltaBypass{ "Enable Delta Audition (Listen to Artifacts Only)" };

    juce::ComboBox satTypeBox;
    juce::Slider satInfSlider, scThreshSlider, scPostSlider, hcCeilSlider, limAttSlider, limRelSlider, limCeilSlider;
    juce::Label satInfLabel, scThreshLabel, scPostLabel, hcCeilLabel, limAttLabel, limRelLabel, limCeilLabel;

    // Attachments
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAtt = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<ButtonAtt> satBypassAtt, scBypassAtt, hcBypassAtt, limBypassAtt, deltaBypassAtt, soloAtt;
    std::unique_ptr<ComboAtt> satTypeAtt;
    std::unique_ptr<SliderAtt> satInfAtt, scThreshAtt, scPostAtt, hcCeilAtt, limAttAtt, limRelAtt, limCeilAtt;
    std::unique_ptr<SliderAtt> crossLowMidAtt, crossMidHighAtt;

    // Visualizer Data
    std::vector<float> peakHistory;
    std::vector<float> gainHistory;
    float currentShadeAmp{ 0.0f };
    int currentBandIndex{ 3 };

    juce::String currentHoverText{ "Hover over any parameter to view its underlying DSP functionality." };

    MixerEDUAudioProcessor& audioProcessor;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixerEDUAudioProcessorEditor)
};