
// D:\Workspace\Subterraneum_plugins_daw\src\MixerView.h

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
    
    // OnStage: Effects-only mode — no instruments row
    MixerRow inputsRow { "INPUTS", GainSlider::SliderType::Input };
    MixerRow outputsRow { "OUTPUTS", GainSlider::SliderType::Output };
    
    void updateInputsRow();
    void updateOutputsRow();
    
    // Track counts for change detection
    int lastInputCount = 0;
    int lastOutputCount = 0;
    
    // CPU OPTIMIZATION: Track graph changes to avoid expensive iteration
    size_t lastGraphNodeCount = 0;
    
    // Callbacks for gain changes
    void onInputGainChanged(int channel, float gain);
    void onOutputGainChanged(int channel, float gain);
};


