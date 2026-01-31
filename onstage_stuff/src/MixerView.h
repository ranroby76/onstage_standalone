#pragma once

#include <JuceHeader.h>
#include "Style.h"
#include "MixerStrip.h"

class SubterraneumAudioProcessor;
class MeteringProcessor;

class MixerView : public juce::Component, public juce::Timer {
public:
    MixerView(SubterraneumAudioProcessor& processor);
    ~MixerView() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;
    
private:
    SubterraneumAudioProcessor& processor;
    
    MixerRow inputsRow { "INPUTS", GainSlider::SliderType::Input };
    MixerRow outputsRow { "OUTPUTS", GainSlider::SliderType::Output };
    MixerRow instrumentsRow { "INSTRUMENTS", GainSlider::SliderType::Instrument };
    
    void updateInputsRow();
    void updateOutputsRow();
    void updateInstrumentsRow();
    
    // Track counts for change detection
    int lastInputCount = 0;
    int lastOutputCount = 0;
    int lastInstrumentCount = 0;
    
    // Callbacks for gain changes
    void onInputGainChanged(int channel, float gain);
    void onOutputGainChanged(int channel, float gain);
    void onInstrumentGainChanged(int index, float gain);
    
    // Store instrument node IDs for mapping
    std::vector<juce::AudioProcessorGraph::NodeID> instrumentNodeIDs;
};
