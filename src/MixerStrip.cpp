#include "MixerStrip.h"

// ==============================================================================
// GainSlider Implementation
// ==============================================================================
GainSlider::GainSlider(int channelIndex, SliderType type, std::function<void(int, float)> onGainChanged)
    : channelIdx(channelIndex), sliderType(type), gainChangedCallback(onGainChanged)
{
    channelName = juce::String(channelIndex + 1);
    
    // Slider range: -inf to +25dB, with 0dB at center
    // Using decibels directly for the slider value
    slider.setSliderStyle(juce::Slider::LinearVertical);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setRange(-60.0, 25.0, 0.1);  // -60dB (essentially silence) to +25dB
    slider.setValue(0.0);  // Default to 0dB (unity gain)
    slider.setSkewFactorFromMidPoint(0.0);  // 0dB at center
    slider.setColour(juce::Slider::thumbColourId, getSliderColor());
    slider.setColour(juce::Slider::trackColourId, getSliderColor().darker());
    slider.addListener(this);
    addAndMakeVisible(slider);
}

GainSlider::~GainSlider() {}

juce::Colour GainSlider::getSliderColor() const {
    switch (sliderType) {
        case SliderType::Input: return juce::Colours::cyan;
        case SliderType::Output: return juce::Colours::lime;
        case SliderType::Instrument: return juce::Colours::orange;
        default: return juce::Colours::white;
    }
}

void GainSlider::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds();
    
    // Background
    g.setColour(juce::Colours::black.withAlpha(0.3f));
    g.fillRoundedRectangle(bounds.toFloat(), 3.0f);
    
    // Channel name at bottom
    auto labelArea = bounds.removeFromBottom(18);
    g.setColour(getSliderColor());
    g.setFont(9.0f);
    
    // Truncate long names
    juce::String displayName = channelName;
    if (displayName.length() > 6) 
        displayName = displayName.substring(0, 5) + "..";
    
    g.drawText(displayName, labelArea, juce::Justification::centred, false);
    
    // dB value at top
    auto valueArea = bounds.removeFromTop(14);
    float dbValue = (float)slider.getValue();
    juce::String dbText;
    if (dbValue <= -59.0f)
        dbText = "-inf";
    else
        dbText = juce::String(dbValue, 1);
    
    g.setColour(juce::Colours::white.withAlpha(0.7f));
    g.setFont(8.0f);
    g.drawText(dbText, valueArea, juce::Justification::centred, false);
}

void GainSlider::resized() {
    auto bounds = getLocalBounds();
    bounds.removeFromBottom(18);  // Label area
    bounds.removeFromTop(14);     // Value area
    bounds.reduce(2, 2);
    slider.setBounds(bounds);
}

void GainSlider::sliderValueChanged(juce::Slider*) {
    if (gainChangedCallback) {
        float dbValue = (float)slider.getValue();
        float linearGain = (dbValue <= -59.0f) ? 0.0f : juce::Decibels::decibelsToGain(dbValue);
        gainChangedCallback(channelIdx, linearGain);
    }
    repaint();  // Update dB display
}

void GainSlider::setGainValue(float gainLinear) {
    float dbValue = (gainLinear <= 0.0001f) ? -60.0f : juce::Decibels::gainToDecibels(gainLinear);
    slider.setValue(dbValue, juce::dontSendNotification);
    repaint();
}

float GainSlider::getGainValue() const {
    float dbValue = (float)slider.getValue();
    return (dbValue <= -59.0f) ? 0.0f : juce::Decibels::decibelsToGain(dbValue);
}

// ==============================================================================
// MixerRow Implementation
// ==============================================================================
MixerRow::MixerRow(const juce::String& rowLabel, GainSlider::SliderType t)
    : label(rowLabel), type(t)
{
    viewport.setScrollBarsShown(false, true);
    viewport.setViewedComponent(&contentComp, false);
    addAndMakeVisible(viewport);
}

MixerRow::~MixerRow() {}

void MixerRow::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds();
    
    // Row label on the left
    auto labelArea = bounds.removeFromLeft(60);
    g.setColour(juce::Colours::white.withAlpha(0.6f));
    g.setFont(11.0f);
    g.drawText(label, labelArea, juce::Justification::centredRight, false);
    
    // Separator line
    g.setColour(juce::Colours::white.withAlpha(0.2f));
    g.drawLine((float)labelArea.getRight() + 5, (float)bounds.getY(), 
               (float)labelArea.getRight() + 5, (float)bounds.getBottom(), 1.0f);
}

void MixerRow::resized() {
    auto bounds = getLocalBounds();
    bounds.removeFromLeft(65);  // Label area
    viewport.setBounds(bounds);
    
    // Layout sliders
    // FIX #2: Doubled width from 40 to 80 for better text label visibility
    const int sliderWidth = 80;
    const int spacing = 2;
    int x = 5;
    
    for (auto* slider : sliders) {
        slider->setBounds(x, 2, sliderWidth, bounds.getHeight() - 4);
        x += sliderWidth + spacing;
    }
    
    contentComp.setSize(x + 10, bounds.getHeight());
}

void MixerRow::setChannelCount(int count, std::function<void(int, float)> onGainChanged) {
    sliders.clear();
    
    for (int i = 0; i < count; ++i) {
        auto* slider = new GainSlider(i, type, onGainChanged);
        sliders.add(slider);
        contentComp.addAndMakeVisible(slider);
    }
    
    resized();
}

void MixerRow::setChannelName(int index, const juce::String& name) {
    if (index >= 0 && index < sliders.size()) {
        sliders[index]->setChannelName(name);
    }
}

void MixerRow::setGainValue(int index, float gainLinear) {
    if (index >= 0 && index < sliders.size()) {
        sliders[index]->setGainValue(gainLinear);
    }
}

GainSlider* MixerRow::getSlider(int index) {
    if (index >= 0 && index < sliders.size())
        return sliders[index];
    return nullptr;
}

