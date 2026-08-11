#include "PluginProcessor.h"
#include "PluginEditor.h"

MixerEDUAudioProcessor::MixerEDUAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(BusesProperties()
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
    ),
    apvts(*this, nullptr, "Parameters", createParameterLayout())
#endif
{
    juce::StringArray prefixes = { "low_", "mid_", "high_", "master_" };
    for (int i = 0; i < 4; ++i) {
        bandParams[i].sat_bypass = apvts.getRawParameterValue(prefixes[i] + "sat_bypass");
        bandParams[i].sat_influence = apvts.getRawParameterValue(prefixes[i] + "sat_influence");
        bandParams[i].sat_type = apvts.getRawParameterValue(prefixes[i] + "sat_type");
        bandParams[i].sc_bypass = apvts.getRawParameterValue(prefixes[i] + "sc_bypass");
        bandParams[i].sc_thresh = apvts.getRawParameterValue(prefixes[i] + "sc_thresh");
        bandParams[i].sc_postgain = apvts.getRawParameterValue(prefixes[i] + "sc_postgain");
        bandParams[i].hc_bypass = apvts.getRawParameterValue(prefixes[i] + "hc_bypass");
        bandParams[i].hc_ceiling = apvts.getRawParameterValue(prefixes[i] + "hc_ceiling");
        bandParams[i].lim_bypass = apvts.getRawParameterValue(prefixes[i] + "lim_bypass");
        bandParams[i].lim_attack = apvts.getRawParameterValue(prefixes[i] + "lim_attack");
        bandParams[i].lim_release = apvts.getRawParameterValue(prefixes[i] + "lim_release");
        bandParams[i].lim_ceiling = apvts.getRawParameterValue(prefixes[i] + "lim_ceiling");

        if (i < 3) bandParams[i].solo = apvts.getRawParameterValue(prefixes[i] + "solo");
    }

    cross_low_mid = apvts.getRawParameterValue("cross_low_mid");
    cross_mid_high = apvts.getRawParameterValue("cross_mid_high");
    delta_listen = apvts.getRawParameterValue("delta_listen");
}

MixerEDUAudioProcessor::~MixerEDUAudioProcessor() {}

juce::AudioProcessorValueTreeState::ParameterLayout MixerEDUAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    juce::StringArray prefixes = { "low_", "mid_", "high_", "master_" };

    for (int i = 0; i < 4; ++i) {
        bool isMaster = (i == 3);
        bool defaultBypass = isMaster ? false : true;
        bool hcDefault = true;

        layout.add(std::make_unique<juce::AudioParameterBool>(prefixes[i] + "sat_bypass", "Sat Bypass", defaultBypass));
        layout.add(std::make_unique<juce::AudioParameterFloat>(prefixes[i] + "sat_influence", "Sat Intensity", 0.0f, 100.0f, 0.0f));
        layout.add(std::make_unique<juce::AudioParameterChoice>(prefixes[i] + "sat_type", "Sat Type", juce::StringArray{ "Tape", "Tube", "Transformer", "Solid State" }, 0));

        layout.add(std::make_unique<juce::AudioParameterBool>(prefixes[i] + "sc_bypass", "SC Bypass", defaultBypass));
        layout.add(std::make_unique<juce::AudioParameterFloat>(prefixes[i] + "sc_thresh", "SC Threshold", -36.0f, 0.0f, -6.0f));
        layout.add(std::make_unique<juce::AudioParameterFloat>(prefixes[i] + "sc_postgain", "SC Post Gain", -12.0f, 12.0f, 0.0f));

        layout.add(std::make_unique<juce::AudioParameterBool>(prefixes[i] + "hc_bypass", "HC Bypass", hcDefault));
        layout.add(std::make_unique<juce::AudioParameterFloat>(prefixes[i] + "hc_ceiling", "HC Ceiling", -36.0f, 0.0f, -0.3f));

        layout.add(std::make_unique<juce::AudioParameterBool>(prefixes[i] + "lim_bypass", "Lim Bypass", defaultBypass));
        layout.add(std::make_unique<juce::AudioParameterFloat>(prefixes[i] + "lim_attack", "Lim Attack", 0.1f, 50.0f, 2.0f));
        layout.add(std::make_unique<juce::AudioParameterFloat>(prefixes[i] + "lim_release", "Lim Release", 10.0f, 500.0f, 100.0f));
        layout.add(std::make_unique<juce::AudioParameterFloat>(prefixes[i] + "lim_ceiling", "Lim Ceiling", -36.0f, 0.0f, -0.1f));

        if (!isMaster) layout.add(std::make_unique<juce::AudioParameterBool>(prefixes[i] + "solo", "Solo", false));
    }

    layout.add(std::make_unique<juce::AudioParameterFloat>("cross_low_mid", "Low/Mid Split", juce::NormalisableRange<float>(20.0f, 2000.0f, 1.0f, 0.3f), 200.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("cross_mid_high", "Mid/High Split", juce::NormalisableRange<float>(1000.0f, 20000.0f, 1.0f, 0.3f), 3000.0f));
    layout.add(std::make_unique<juce::AudioParameterBool>("delta_listen", "Delta Listen", false));

    return layout;
}

void MixerEDUAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);
    int totalNumInputChannels = getTotalNumInputChannels();

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32>(totalNumInputChannels);

    lp1.prepare(spec); lp1.setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
    hp1.prepare(spec); hp1.setType(juce::dsp::LinkwitzRileyFilterType::highpass);
    lp2.prepare(spec); lp2.setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
    hp2.prepare(spec); hp2.setType(juce::dsp::LinkwitzRileyFilterType::highpass);

    int maxLookaheadSamples = static_cast<int>(sampleRate * 0.005);

    for (int i = 0; i < 4; ++i) {
        bands[i].delayBuffer.setSize(totalNumInputChannels, maxLookaheadSamples);
        bands[i].delayBuffer.clear();
        bands[i].delayWritePosition = 0;
        bands[i].smoothedGain = 1.0f;
        bands[i].inputEnvelope = 0.0f;
    }

    dryBuffer.setSize(totalNumInputChannels, samplesPerBlock);
    lowBuffer.setSize(totalNumInputChannels, samplesPerBlock);
    midBuffer.setSize(totalNumInputChannels, samplesPerBlock);
    highBuffer.setSize(totalNumInputChannels, samplesPerBlock);
}

void MixerEDUAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);
    juce::ScopedNoDenormals noDenormals;
    int totalNumInputChannels = getTotalNumInputChannels();
    int samples = buffer.getNumSamples();

    for (auto i = totalNumInputChannels; i < getTotalNumOutputChannels(); ++i)
        buffer.clear(i, 0, samples);

    double sr = getSampleRate();
    if (sr <= 0.0) sr = 44100.0;

    if (dryBuffer.getNumSamples() < samples) {
        dryBuffer.setSize(totalNumInputChannels, samples, false, false, true);
        lowBuffer.setSize(totalNumInputChannels, samples, false, false, true);
        midBuffer.setSize(totalNumInputChannels, samples, false, false, true);
        highBuffer.setSize(totalNumInputChannels, samples, false, false, true);
    }

    // Save Dry Signal
    for (int ch = 0; ch < totalNumInputChannels; ++ch)
        dryBuffer.copyFrom(ch, 0, buffer, ch, 0, samples);

    // Conditional bypass logic to prevent All-Pass Phase Shift artifacts when bands are inactive
    bool allBandsBypassed = true;
    for (int i = 0; i < 3; ++i) {
        if (bandParams[i].sat_bypass->load() < 0.5f ||
            bandParams[i].sc_bypass->load() < 0.5f ||
            bandParams[i].hc_bypass->load() < 0.5f ||
            bandParams[i].solo->load() > 0.5f) {
            allBandsBypassed = false;
            break;
        }
    }

    if (allBandsBypassed) {
        // Skip Crossovers. Pass dry directly to Master.
        for (int ch = 0; ch < totalNumInputChannels; ++ch) {
            buffer.copyFrom(ch, 0, dryBuffer, ch, 0, samples);
        }
        for (int i = 0; i < 3; ++i) {
            bands[i].visPeak.store(0.0f);
            bands[i].visInputPeak.store(0.0f);
        }
    }
    else {
        float f1 = cross_low_mid->load();
        float f2 = cross_mid_high->load();
        lp1.setCutoffFrequency(f1); hp1.setCutoffFrequency(f1);
        lp2.setCutoffFrequency(f2); hp2.setCutoffFrequency(f2);

        // Splitting
        for (int ch = 0; ch < totalNumInputChannels; ++ch) {
            auto* in = buffer.getReadPointer(ch);
            auto* low = lowBuffer.getWritePointer(ch);
            auto* mid = midBuffer.getWritePointer(ch);
            auto* high = highBuffer.getWritePointer(ch);

            for (int s = 0; s < samples; ++s) {
                float val = in[s];
                low[s] = lp1.processSample(ch, val);
                float midHigh = hp1.processSample(ch, val);
                mid[s] = lp2.processSample(ch, midHigh);
                high[s] = hp2.processSample(ch, midHigh);
            }
        }

        processStage(lowBuffer, 0, sr);
        processStage(midBuffer, 1, sr);
        processStage(highBuffer, 2, sr);

        bool sLow = bandParams[0].solo->load() > 0.5f;
        bool sMid = bandParams[1].solo->load() > 0.5f;
        bool sHigh = bandParams[2].solo->load() > 0.5f;
        bool anySolo = sLow || sMid || sHigh;

        if (anySolo) {
            if (!sLow) lowBuffer.applyGain(0.0f);
            if (!sMid) midBuffer.applyGain(0.0f);
            if (!sHigh) highBuffer.applyGain(0.0f);
        }

        for (int ch = 0; ch < totalNumInputChannels; ++ch) {
            buffer.copyFrom(ch, 0, lowBuffer, ch, 0, samples);
            buffer.addFrom(ch, 0, midBuffer, ch, 0, samples);
            buffer.addFrom(ch, 0, highBuffer, ch, 0, samples);
        }
    }

    // Process Master Band
    processStage(buffer, 3, sr);

    // Delta Audition
    if (delta_listen->load() > 0.5f) {
        for (int ch = 0; ch < totalNumInputChannels; ++ch) {
            auto* outData = buffer.getWritePointer(ch);
            auto* dryData = dryBuffer.getReadPointer(ch);
            for (int s = 0; s < samples; ++s) {
                outData[s] = outData[s] - dryData[s];
            }
        }
    }

    // Loudness Estimator
    float sumSq = 0.0f;
    int totalReadSamples = samples * totalNumInputChannels;
    for (int ch = 0; ch < totalNumInputChannels; ++ch) {
        const float* r = buffer.getReadPointer(ch);
        for (int s = 0; s < samples; ++s) sumSq += r[s] * r[s];
    }
    float rms = std::sqrt(sumSq / juce::jmax(1, totalReadSamples));
    float nowDb = juce::Decibels::gainToDecibels(rms, -80.0f);
    currentRMSDb.store(currentRMSDb.load() * 0.8f + nowDb * 0.2f);
}

void MixerEDUAudioProcessor::processStage(juce::AudioBuffer<float>& buffer, int bandIndex, double sr)
{
    auto& state = bands[bandIndex];
    auto& params = bandParams[bandIndex];
    int totalNumInputChannels = buffer.getNumChannels();

    float inPeak = 0.0f;
    for (int ch = 0; ch < totalNumInputChannels; ++ch) {
        const float* rawData = buffer.getReadPointer(ch);
        for (int s = 0; s < buffer.getNumSamples(); ++s) {
            inPeak = juce::jmax(inPeak, std::abs(rawData[s]));
        }
    }
    state.visInputPeak.store(inPeak);

    // --- STAGE 1: ANALOG SATURATION ---
    if (params.sat_bypass->load() < 0.5f) {
        float satInfluence = params.sat_influence->load() * 0.01f;
        int satType = juce::roundToInt(params.sat_type->load());

        if (satInfluence > 0.0f) {
            for (int ch = 0; ch < totalNumInputChannels; ++ch) {
                auto* channelData = buffer.getWritePointer(ch);
                for (int s = 0; s < buffer.getNumSamples(); ++s) {
                    float clean = channelData[s];
                    float saturated = clean;

                    switch (satType) {
                    case 0: saturated = std::tanh(clean * 1.5f); break;
                    case 1: saturated = (clean > 0.0f) ? std::tanh(clean * 1.5f) : (std::tanh(clean * 0.8f) * 1.25f); break;
                    case 2: { float v = clean * 1.3f; saturated = (v > 1.570796f) ? 1.0f : ((v < -1.570796f) ? -1.0f : std::sin(v)); } break;
                    case 3: { float v = clean * 2.5f; saturated = v / std::sqrt(1.0f + v * v); } break;
                    }
                    channelData[s] = clean * (1.0f - satInfluence) + saturated * satInfluence;
                }
            }
        }
    }

    // --- STAGE 2: TRANSCENDENTAL SOFT CLIPPER ---
    if (params.sc_bypass->load() < 0.5f) {
        float threshLin = juce::Decibels::decibelsToGain(params.sc_thresh->load());
        float postGainLin = juce::Decibels::decibelsToGain(params.sc_postgain->load());

        for (int ch = 0; ch < totalNumInputChannels; ++ch) {
            auto* data = buffer.getWritePointer(ch);
            for (int s = 0; s < buffer.getNumSamples(); ++s) {
                float x = data[s];
                float absX = std::abs(x);
                float shaped = x;

                if (absX > threshLin) {
                    float headroom = 1.0f - threshLin;
                    if (headroom > 0.00001f) {
                        float overshoot = (absX - threshLin) / headroom;
                        shaped = (x < 0.0f ? -1.0f : 1.0f) * (threshLin + headroom * std::tanh(overshoot));
                    }
                    else {
                        shaped = (x < 0.0f ? -1.0f : 1.0f);
                    }
                }
                data[s] = shaped * postGainLin;
            }
        }
    }

    // --- STAGE 3: PLAIN HARD CLIPPER ---
    if (params.hc_bypass->load() < 0.5f) {
        float ceilLin = juce::Decibels::decibelsToGain(params.hc_ceiling->load());
        for (int ch = 0; ch < totalNumInputChannels; ++ch) {
            auto* data = buffer.getWritePointer(ch);
            for (int s = 0; s < buffer.getNumSamples(); ++s)
                data[s] = juce::jlimit(-ceilLin, ceilLin, data[s]);
        }
    }

    // --- STAGE 4: LOOK-AHEAD LIMITER ---
    float blockMaxPeak = 0.0f;
    float blockMinGain = 1.0f;

    // Limiter is now strictly Master-only to prevent phase alignment glitches inside crossover bands
    bool applyLimiter = (params.lim_bypass->load() < 0.5f) && (bandIndex == 3);

    if (applyLimiter) {
        float attSec = params.lim_attack->load() * 0.001f;
        float relSec = params.lim_release->load() * 0.001f;
        float ceilLin = juce::Decibels::decibelsToGain(params.lim_ceiling->load());

        float attCoeff = std::exp(-1.0f / (attSec * sr));
        float relCoeff = std::exp(-1.0f / (relSec * sr));
        int delayLen = state.delayBuffer.getNumSamples();

        for (int s = 0; s < buffer.getNumSamples(); ++s) {
            float sidechainMax = 0.0f;
            for (int ch = 0; ch < totalNumInputChannels; ++ch) {
                float in = buffer.getReadPointer(ch)[s];
                sidechainMax = juce::jmax(sidechainMax, std::abs(in));
                if (delayLen > 0) state.delayBuffer.setSample(ch, state.delayWritePosition, in);
            }

            if (sidechainMax > state.inputEnvelope) state.inputEnvelope = sidechainMax;
            else state.inputEnvelope = relCoeff * state.inputEnvelope + (1.0f - relCoeff) * sidechainMax;

            float targetGain = (state.inputEnvelope > ceilLin) ? (ceilLin / state.inputEnvelope) : 1.0f;

            if (targetGain < state.smoothedGain) state.smoothedGain = attCoeff * state.smoothedGain + (1.0f - attCoeff) * targetGain;
            else state.smoothedGain = relCoeff * state.smoothedGain + (1.0f - relCoeff) * targetGain;

            int readPos = state.delayWritePosition + 1;
            if (readPos >= delayLen) readPos = 0;

            for (int ch = 0; ch < totalNumInputChannels; ++ch) {
                if (delayLen > 0) {
                    float out = state.delayBuffer.getSample(ch, readPos) * state.smoothedGain;
                    buffer.getWritePointer(ch)[s] = juce::jlimit(-ceilLin, ceilLin, out);
                }
            }

            if (delayLen > 0) state.delayWritePosition = (state.delayWritePosition + 1) % delayLen;

            blockMaxPeak = juce::jmax(blockMaxPeak, sidechainMax);
            blockMinGain = juce::jmin(blockMinGain, state.smoothedGain);
        }
    }
    else {
        for (int ch = 0; ch < totalNumInputChannels; ++ch) {
            auto* data = buffer.getReadPointer(ch);
            for (int s = 0; s < buffer.getNumSamples(); ++s)
                blockMaxPeak = juce::jmax(blockMaxPeak, std::abs(data[s]));
        }
    }

    state.visPeak.store(blockMaxPeak);
    state.visGain.store(blockMinGain);
}

bool MixerEDUAudioProcessor::isBusesLayoutSupported(const juce::AudioProcessor::BusesLayout& layouts) const { juce::ignoreUnused(layouts); return true; }
void MixerEDUAudioProcessor::releaseResources() {}
juce::AudioProcessorEditor* MixerEDUAudioProcessor::createEditor() { return new MixerEDUAudioProcessorEditor(*this); }
void MixerEDUAudioProcessor::getStateInformation(juce::MemoryBlock& destData) { juce::ignoreUnused(destData); }
void MixerEDUAudioProcessor::setStateInformation(const void* data, int sizeInBytes) { juce::ignoreUnused(data, sizeInBytes); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MixerEDUAudioProcessor(); }