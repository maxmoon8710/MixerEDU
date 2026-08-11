#include "PluginProcessor.h"
#include "PluginEditor.h"

MixerEDUAudioProcessorEditor::MixerEDUAudioProcessorEditor(MixerEDUAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    peakHistory.resize(200, 0.0f);
    gainHistory.resize(200, 1.0f);

    // --- Band Selection Setup ---
    addAndMakeVisible(btnLow); addAndMakeVisible(btnMid);
    addAndMakeVisible(btnHigh); addAndMakeVisible(btnMaster);
    addAndMakeVisible(btnSolo);

    btnLow.setClickingTogglesState(true); btnMid.setClickingTogglesState(true);
    btnHigh.setClickingTogglesState(true); btnMaster.setClickingTogglesState(true);

    btnLow.setRadioGroupId(100); btnMid.setRadioGroupId(100);
    btnHigh.setRadioGroupId(100); btnMaster.setRadioGroupId(100);

    btnLow.onClick = [this]() { setBand(0); };
    btnMid.onClick = [this]() { setBand(1); };
    btnHigh.onClick = [this]() { setBand(2); };
    btnMaster.onClick = [this]() { setBand(3); };

    // --- Setup Crossovers (Distinct Colors) ---
    styleCrossoverKnob(crossLowMidSlider, crossLowMidLabel, "Low/Mid Split", " Hz");
    styleCrossoverKnob(crossMidHighSlider, crossMidHighLabel, "Mid/High Split", " Hz");
    crossLowMidAtt = std::make_unique<SliderAtt>(audioProcessor.apvts, "cross_low_mid", crossLowMidSlider);
    crossMidHighAtt = std::make_unique<SliderAtt>(audioProcessor.apvts, "cross_mid_high", crossMidHighSlider);

    // --- Setup Standard Stages ---
    addAndMakeVisible(satBypass);
    satTypeBox.addItemList(juce::StringArray{ "Tape", "Tube", "Transformer", "Solid State" }, 1);
    addAndMakeVisible(satTypeBox);
    styleKnob(satInfSlider, satInfLabel, "Intensity", " %");

    addAndMakeVisible(scBypass);
    styleKnob(scThreshSlider, scThreshLabel, "Threshold", " dB");
    styleKnob(scPostSlider, scPostLabel, "Post Gain", " dB");

    addAndMakeVisible(hcBypass);
    styleKnob(hcCeilSlider, hcCeilLabel, "Ceiling", " dB");

    addAndMakeVisible(limBypass);
    styleKnob(limAttSlider, limAttLabel, "Attack", " ms");
    styleKnob(limRelSlider, limRelLabel, "Release", " ms");
    styleKnob(limCeilSlider, limCeilLabel, "Ceiling", " dB");

    deltaBypass.setButtonText("Delta Audition (Solo Artifacts)");
    addAndMakeVisible(deltaBypass);
    deltaBypassAtt = std::make_unique<ButtonAtt>(audioProcessor.apvts, "delta_listen", deltaBypass);

    setSize(880, 700);
    startTimerHz(30);

    btnMaster.setToggleState(true, juce::dontSendNotification);
    setBand(3);
}

MixerEDUAudioProcessorEditor::~MixerEDUAudioProcessorEditor() {}

void MixerEDUAudioProcessorEditor::setBand(int index)
{
    currentBandIndex = index;
    juce::StringArray prefixes = { "low_", "mid_", "high_", "master_" };
    juce::String p = prefixes[index];

    satBypassAtt.reset(); satBypassAtt = std::make_unique<ButtonAtt>(audioProcessor.apvts, p + "sat_bypass", satBypass);
    satTypeAtt.reset(); satTypeAtt = std::make_unique<ComboAtt>(audioProcessor.apvts, p + "sat_type", satTypeBox);
    satInfAtt.reset(); satInfAtt = std::make_unique<SliderAtt>(audioProcessor.apvts, p + "sat_influence", satInfSlider);

    scBypassAtt.reset(); scBypassAtt = std::make_unique<ButtonAtt>(audioProcessor.apvts, p + "sc_bypass", scBypass);
    scThreshAtt.reset(); scThreshAtt = std::make_unique<SliderAtt>(audioProcessor.apvts, p + "sc_thresh", scThreshSlider);
    scPostAtt.reset(); scPostAtt = std::make_unique<SliderAtt>(audioProcessor.apvts, p + "sc_postgain", scPostSlider);

    hcBypassAtt.reset(); hcBypassAtt = std::make_unique<ButtonAtt>(audioProcessor.apvts, p + "hc_bypass", hcBypass);
    hcCeilAtt.reset(); hcCeilAtt = std::make_unique<SliderAtt>(audioProcessor.apvts, p + "hc_ceiling", hcCeilSlider);

    limBypassAtt.reset(); limBypassAtt = std::make_unique<ButtonAtt>(audioProcessor.apvts, p + "lim_bypass", limBypass);
    limAttAtt.reset(); limAttAtt = std::make_unique<SliderAtt>(audioProcessor.apvts, p + "lim_attack", limAttSlider);
    limRelAtt.reset(); limRelAtt = std::make_unique<SliderAtt>(audioProcessor.apvts, p + "lim_release", limRelSlider);
    limCeilAtt.reset(); limCeilAtt = std::make_unique<SliderAtt>(audioProcessor.apvts, p + "lim_ceiling", limCeilSlider);

    bool isMaster = (index == 3);

    if (isMaster) {
        btnSolo.setVisible(false);
        soloAtt.reset();
    }
    else {
        btnSolo.setVisible(true);
        soloAtt.reset(); soloAtt = std::make_unique<ButtonAtt>(audioProcessor.apvts, p + "solo", btnSolo);
    }

    // Hide Limiter UI if on an individual band
    limBypass.setVisible(isMaster);
    limAttSlider.setVisible(isMaster);
    limRelSlider.setVisible(isMaster);
    limCeilSlider.setVisible(isMaster);
    limAttLabel.setVisible(isMaster);
    limRelLabel.setVisible(isMaster);
    limCeilLabel.setVisible(isMaster);

    std::fill(peakHistory.begin(), peakHistory.end(), 0.0f);
    std::fill(gainHistory.begin(), gainHistory.end(), 1.0f);

    repaint();
}

void MixerEDUAudioProcessorEditor::styleKnob(juce::Slider& s, juce::Label& l, const juce::String& name, const juce::String& suffix)
{
    s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 65, 16);
    s.setTextValueSuffix(suffix);
    addAndMakeVisible(s);

    l.setText(name, juce::dontSendNotification);
    l.setJustificationType(juce::Justification::centred);
    l.attachToComponent(&s, false);
}

void MixerEDUAudioProcessorEditor::styleCrossoverKnob(juce::Slider& s, juce::Label& l, const juce::String& name, const juce::String& suffix)
{
    s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 65, 16);
    s.setTextValueSuffix(suffix);

    // Decouple visually from the standard processing modules
    s.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xffff9900));
    s.setColour(juce::Slider::thumbColourId, juce::Colours::white);
    addAndMakeVisible(s);

    l.setText(name, juce::dontSendNotification);
    l.setJustificationType(juce::Justification::centred);
    l.setColour(juce::Label::textColourId, juce::Colour(0xffff9900));
    l.attachToComponent(&s, false);
}

void MixerEDUAudioProcessorEditor::timerCallback()
{
    peakHistory.erase(peakHistory.begin());
    peakHistory.push_back(audioProcessor.bands[currentBandIndex].visPeak.load());

    gainHistory.erase(gainHistory.begin());
    gainHistory.push_back(audioProcessor.bands[currentBandIndex].visGain.load());

    currentShadeAmp = currentShadeAmp * 0.85f + audioProcessor.bands[currentBandIndex].visInputPeak.load() * 0.15f;

    updateHoverText();
    repaint();
}

void MixerEDUAudioProcessorEditor::updateHoverText()
{
    juce::Point<int> pos = getMouseXYRelative();
    juce::String newText = "Hover over any parameter to view its underlying DSP functionality.";

    if (satTypeBox.getBounds().contains(pos)) newText = "Saturation Memoryless Waveshaping: Tape mode applies f(x) = tanh(1.5x) to round peaks and synthesize odd harmonics.";
    else if (satInfSlider.getBounds().contains(pos)) newText = "Intensity (Wet/Dry): Blends the linear passthrough signal with the non-linear hyperbolic curve.";
    else if (scThreshSlider.getBounds().contains(pos)) newText = "Soft Clip Threshold: The exact amplitude boundary where mathematical linearity ends and asymptotic rounding begins.";
    else if (hcCeilSlider.getBounds().contains(pos)) newText = "Hard Clip: A strict min/max logical limit. Anything exceeding the threshold is digitally sheared, creating aggressive distortion.";
    else if (limAttSlider.getBounds().contains(pos)) newText = "Look-Ahead: Delays the audio 5ms into a buffer, allowing the attenuation envelope to drop BEFORE the transient physically arrives.";
    else if (deltaBypass.getBounds().contains(pos)) newText = "Delta: Calculates f(x) = Output - Input. Solo the clipping artifacts to understand what is being removed from your transients.";
    else if (crossLowMidSlider.getBounds().contains(pos)) newText = "Crossover: Splits the Low and Mid frequency bands using a phase-aligned Linkwitz-Riley filter.";

    currentHoverText = newText;
}

void MixerEDUAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff181818));

    auto area = getLocalBounds();
    auto tooltipArea = area.removeFromBottom(40).reduced(15, 5);
    auto header = area.removeFromTop(35);
    auto knobsArea = area.removeFromTop(380);
    auto visArea = area.reduced(25, 25);

    g.setColour(juce::Colours::white);
    g.setFont(15.0f);
    int colW = header.getWidth() / 4;
    g.drawFittedText("SATURATION", header.removeFromLeft(colW), juce::Justification::centred, 1);
    g.drawFittedText("SOFT CLIPPER", header.removeFromLeft(colW), juce::Justification::centred, 1);
    g.drawFittedText("HARD CLIP", header.removeFromLeft(colW), juce::Justification::centred, 1);

    // Only draw Limiter header if we are on the Master band
    if (currentBandIndex == 3) g.drawFittedText("LIMITER", header, juce::Justification::centred, 1);

    auto tfArea = visArea.removeFromLeft(visArea.getWidth() / 2).reduced(0, 0);
    visArea.removeFromLeft(15);
    auto scopeArea = visArea;

    drawTransferFunction(g, tfArea);
    drawEnvelopeScope(g, scopeArea);

    g.setColour(juce::Colour(0xff2a2a2a));
    g.fillRoundedRectangle(tooltipArea.toFloat(), 4.0f);
    g.setColour(juce::Colours::cyan);
    g.setFont(14.0f);
    g.drawFittedText(currentHoverText, tooltipArea.reduced(10, 0), juce::Justification::centred, 2);
}

void MixerEDUAudioProcessorEditor::drawTransferFunction(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    g.setColour(juce::Colour(0xff101010));
    g.fillRoundedRectangle(bounds.toFloat(), 5.0f);

    g.setColour(juce::Colours::white.withAlpha(0.1f));
    g.drawLine(bounds.getX(), bounds.getCentreY(), bounds.getRight(), bounds.getCentreY());
    g.drawLine(bounds.getCentreX(), bounds.getY(), bounds.getCentreX(), bounds.getBottom());

    g.setColour(juce::Colours::white.withAlpha(0.2f));
    float dashLengths[2] = { 4.0f, 4.0f };

    float posOneY = juce::jmap(1.0f, -1.5f, 1.5f, float(bounds.getBottom()), float(bounds.getY()));
    float negOneY = juce::jmap(-1.0f, -1.5f, 1.5f, float(bounds.getBottom()), float(bounds.getY()));
    juce::Line<float> topCeil(bounds.getX(), posOneY, bounds.getRight(), posOneY);
    juce::Line<float> botCeil(bounds.getX(), negOneY, bounds.getRight(), negOneY);
    g.drawDashedLine(topCeil, dashLengths, 2);
    g.drawDashedLine(botCeil, dashLengths, 2);

    float posOneX = juce::jmap(1.0f, -1.5f, 1.5f, float(bounds.getX()), float(bounds.getRight()));
    float negOneX = juce::jmap(-1.0f, -1.5f, 1.5f, float(bounds.getX()), float(bounds.getRight()));
    juce::Line<float> rightCeil(posOneX, bounds.getY(), posOneX, bounds.getBottom());
    juce::Line<float> leftCeil(negOneX, bounds.getY(), negOneX, bounds.getBottom());
    g.drawDashedLine(rightCeil, dashLengths, 2);
    g.drawDashedLine(leftCeil, dashLengths, 2);

    g.setColour(juce::Colours::white.withAlpha(0.5f));
    g.setFont(12.0f);
    g.drawText("TRANSFER FUNCTION f(x)", bounds.reduced(10), juce::Justification::topLeft, false);

    g.setColour(juce::Colours::white.withAlpha(0.4f));
    g.setFont(10.0f);
    g.drawText("INPUT", bounds.getRight() - 40, bounds.getCentreY() + 4, 35, 15, juce::Justification::right, false);
    g.drawText("OUTPUT", bounds.getCentreX() + 4, bounds.getY() + 4, 45, 15, juce::Justification::left, false);

    auto& params = audioProcessor.bandParams[currentBandIndex];
    bool satOn = params.sat_bypass->load() < 0.5f;
    float satInf = params.sat_influence->load() * 0.01f;
    int satType = juce::roundToInt(params.sat_type->load());

    bool scOn = params.sc_bypass->load() < 0.5f;
    float threshLin = juce::Decibels::decibelsToGain(params.sc_thresh->load());
    float postGainLin = juce::Decibels::decibelsToGain(params.sc_postgain->load());

    bool hcOn = params.hc_bypass->load() < 0.5f;
    float hcCeilLin = juce::Decibels::decibelsToGain(params.hc_ceiling->load());

    auto getShapedY = [&](float x) {
        float y = x;
        if (satOn && satInf > 0.0f) {
            float saturated = y;
            switch (satType) {
            case 0: saturated = std::tanh(y * 1.5f); break;
            case 1: saturated = (y > 0.0f) ? std::tanh(y * 1.5f) : (std::tanh(y * 0.8f) * 1.25f); break;
            case 2: { float v = y * 1.3f; saturated = (v > 1.570796f) ? 1.0f : ((v < -1.570796f) ? -1.0f : std::sin(v)); } break;
            case 3: { float v = y * 2.5f; saturated = v / std::sqrt(1.0f + v * v); } break;
            }
            y = y * (1.0f - satInf) + saturated * satInf;
        }

        if (scOn) {
            float absY = std::abs(y);
            if (absY > threshLin) {
                float headroom = 1.0f - threshLin;
                if (headroom > 0.00001f) {
                    float overshoot = (absY - threshLin) / headroom;
                    y = (y < 0.0f ? -1.0f : 1.0f) * (threshLin + headroom * std::tanh(overshoot));
                }
                else {
                    y = (y < 0.0f ? -1.0f : 1.0f);
                }
            }
            y *= postGainLin;
        }

        if (hcOn) y = juce::jlimit(-hcCeilLin, hcCeilLin, y);
        return y;
        };

    float zeroY = juce::jmap(0.0f, -1.5f, 1.5f, float(bounds.getBottom()), float(bounds.getY()));

    int startPx = juce::roundToInt(juce::jmap(-currentShadeAmp, -1.5f, 1.5f, 0.0f, float(bounds.getWidth())));
    int endPx = juce::roundToInt(juce::jmap(currentShadeAmp, -1.5f, 1.5f, 0.0f, float(bounds.getWidth())));

    startPx = juce::jlimit(0, bounds.getWidth(), startPx);
    endPx = juce::jlimit(0, bounds.getWidth(), endPx);

    if (endPx > startPx) {
        juce::Path fillPath;
        fillPath.startNewSubPath(startPx + bounds.getX(), zeroY);

        for (int px = startPx; px <= endPx; ++px) {
            float x = juce::jmap(float(px), 0.0f, float(bounds.getWidth()), -1.5f, 1.5f);
            float y = getShapedY(x);
            float py = juce::jmap(y, -1.5f, 1.5f, float(bounds.getBottom()), float(bounds.getY()));
            py = juce::jlimit((float)bounds.getY(), (float)bounds.getBottom(), py);
            fillPath.lineTo(px + bounds.getX(), py);
        }
        fillPath.lineTo(endPx + bounds.getX(), zeroY);
        fillPath.closeSubPath();

        if (currentBandIndex == 0) g.setColour(juce::Colour(0x60ff5555));
        else if (currentBandIndex == 1) g.setColour(juce::Colour(0x60ffaa00));
        else if (currentBandIndex == 2) g.setColour(juce::Colour(0x60ffff00));
        else g.setColour(juce::Colour(0x6000ff99));

        g.fillPath(fillPath);
    }

    juce::Path curve;
    bool first = true;
    for (int px = 0; px <= bounds.getWidth(); ++px) {
        float x = juce::jmap(float(px), 0.0f, float(bounds.getWidth()), -1.5f, 1.5f);
        float y = getShapedY(x);
        float py = juce::jmap(y, -1.5f, 1.5f, float(bounds.getBottom()), float(bounds.getY()));
        py = juce::jlimit((float)bounds.getY(), (float)bounds.getBottom(), py);

        if (first) { curve.startNewSubPath(px + bounds.getX(), py); first = false; }
        else { curve.lineTo(px + bounds.getX(), py); }
    }
    g.setColour(juce::Colours::yellow);
    g.strokePath(curve, juce::PathStrokeType(2.0f));
}

void MixerEDUAudioProcessorEditor::drawEnvelopeScope(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    g.setColour(juce::Colour(0xff101010));
    g.fillRoundedRectangle(bounds.toFloat(), 5.0f);

    g.setColour(juce::Colours::white.withAlpha(0.5f));
    g.setFont(12.0f);
    g.drawText("LOOK-AHEAD GAIN ENVELOPE", bounds.reduced(10), juce::Justification::topLeft, false);

    juce::Path peakPath, gainPath;
    float w = bounds.getWidth();
    float h = bounds.getHeight();

    for (size_t i = 0; i < peakHistory.size(); ++i) {
        float x = bounds.getX() + (i / 200.0f) * w;

        float py = bounds.getBottom() - juce::jmap(peakHistory[i], 0.0f, 1.2f, 0.0f, h);
        py = juce::jlimit((float)bounds.getY(), (float)bounds.getBottom(), py);

        float gy = bounds.getBottom() - juce::jmap(gainHistory[i], 0.0f, 1.0f, 0.0f, h);
        gy = juce::jlimit((float)bounds.getY(), (float)bounds.getBottom(), gy);

        if (i == 0) {
            peakPath.startNewSubPath(x, py);
            gainPath.startNewSubPath(x, gy);
        }
        else {
            peakPath.lineTo(x, py);
            gainPath.lineTo(x, gy);
        }
    }

    g.setColour(juce::Colours::red.withAlpha(0.7f));
    g.strokePath(peakPath, juce::PathStrokeType(1.5f));

    g.setColour(juce::Colour(0xff00e5ff));
    g.strokePath(gainPath, juce::PathStrokeType(2.5f));

    if (currentBandIndex == 3) {
        float ceilDb = audioProcessor.bandParams[currentBandIndex].lim_ceiling->load();
        float ceilLin = juce::Decibels::decibelsToGain(ceilDb);

        float ceilY = bounds.getBottom() - juce::jmap(ceilLin, 0.0f, 1.2f, 0.0f, h);
        ceilY = juce::jlimit((float)bounds.getY(), (float)bounds.getBottom(), ceilY);

        g.setColour(juce::Colours::white.withAlpha(0.35f));
        float dashLengths[2] = { 4.0f, 4.0f };
        juce::Line<float> dashLine(bounds.getX(), ceilY, bounds.getRight(), ceilY);
        g.drawDashedLine(dashLine, dashLengths, 2);
    }
}

void MixerEDUAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();

    area.removeFromBottom(40);
    area.removeFromTop(35);

    auto knobsArea = area.removeFromTop(380);
    int colWidth = knobsArea.getWidth() / 4;

    auto c1 = knobsArea.removeFromLeft(colWidth).reduced(15, 0);
    auto c2 = knobsArea.removeFromLeft(colWidth).reduced(15, 0);
    auto c3 = knobsArea.removeFromLeft(colWidth).reduced(15, 0);
    auto c4 = knobsArea.reduced(15, 0);

    // Col 1: Saturation & Band Selectors
    satBypass.setBounds(c1.removeFromTop(24));
    c1.removeFromTop(10);
    satTypeBox.setBounds(c1.removeFromTop(24));
    c1.removeFromTop(20);
    satInfSlider.setBounds(c1.removeFromTop(85));

    c1.removeFromTop(30);
    auto r1 = c1.removeFromTop(24);
    btnLow.setBounds(r1.removeFromLeft(colWidth / 2 - 17));
    btnMid.setBounds(r1.removeFromRight(colWidth / 2 - 17));
    c1.removeFromTop(10);
    auto r2 = c1.removeFromTop(24);
    btnHigh.setBounds(r2.removeFromLeft(colWidth / 2 - 17));
    btnMaster.setBounds(r2.removeFromRight(colWidth / 2 - 17));
    c1.removeFromTop(15);
    btnSolo.setBounds(c1.removeFromTop(24));

    // Col 2: Soft Clip & Crossover 1
    scBypass.setBounds(c2.removeFromTop(24));
    c2.removeFromTop(45);
    scThreshSlider.setBounds(c2.removeFromTop(85));
    c2.removeFromTop(45);
    scPostSlider.setBounds(c2.removeFromTop(85));

    c2.removeFromTop(30);
    crossLowMidSlider.setBounds(c2.removeFromTop(75));

    // Col 3: Hard Clip & Crossover 2
    hcBypass.setBounds(c3.removeFromTop(24));
    c3.removeFromTop(20);
    hcCeilSlider.setBounds(c3.removeFromTop(85));
    c3.removeFromTop(30);
    deltaBypass.setBounds(c3.removeFromTop(30));

    c3.removeFromTop(46);
    crossMidHighSlider.setBounds(c3.removeFromTop(75));

    // Col 4: Limiter 
    limBypass.setBounds(c4.removeFromTop(24));
    c4.removeFromTop(20);
    limAttSlider.setBounds(c4.removeFromTop(85));
    c4.removeFromTop(25);
    limRelSlider.setBounds(c4.removeFromTop(85));
    c4.removeFromTop(25);
    limCeilSlider.setBounds(c4.removeFromTop(85));
}