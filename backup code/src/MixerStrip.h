#pragma once

#include <JuceHeader.h>
#include "Style.h"

// A single gain slider for the mixer - compact design
class GainSlider : public juce::Component, public juce::Slider::Listener {
public:
    enum class SliderType { Input, Output, Instrument };
    
    GainSlider(int channelIndex, SliderType type, std::function<void(int, float)> onGainChanged);
    ~GainSlider() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    void sliderValueChanged(juce::Slider* slider) override;
    
    void setChannelName(const juce::String& name) { channelName = name; repaint(); }
    void setGainValue(float gainLinear);
    float getGainValue() const;
    
    int getChannelIndex() const { return channelIdx; }
    
private:
    int channelIdx;
    SliderType sliderType;
    juce::String channelName;
    juce::Slider slider;
    std::function<void(int, float)> gainChangedCallback;
    
    juce::Colour getSliderColor() const;
};

// A horizontal row of gain sliders
class MixerRow : public juce::Component {
public:
    MixerRow(const juce::String& rowLabel, GainSlider::SliderType type);
    ~MixerRow() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    void setChannelCount(int count, std::function<void(int, float)> onGainChanged);
    void setChannelName(int index, const juce::String& name);
    void setGainValue(int index, float gainLinear);
    
    GainSlider* getSlider(int index);
    int getSliderCount() const { return sliders.size(); }
    
private:
    juce::String label;
    GainSlider::SliderType type;
    juce::OwnedArray<GainSlider> sliders;
    juce::Viewport viewport;
    juce::Component contentComp;
};
