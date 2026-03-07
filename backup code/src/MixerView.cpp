
// CRITICAL FIX: Use isInstrument() instead of getPluginDescription().isInstrument
// getPluginDescription() freezes some plugins when called!

#include "MixerView.h"
#include "PluginProcessor.h"  // This includes MeteringProcessor

MixerView::MixerView(SubterraneumAudioProcessor& p) : processor(p) {
    // OnStage: Effects-only mode — no instruments row
    addAndMakeVisible(inputsRow);
    addAndMakeVisible(outputsRow);
    
    // PERFORMANCE: Opaque components render faster
    setOpaque(true);
    
    // CPU OPTIMIZATION: Changed from 200ms to 300ms - inputs/outputs rarely change
    startTimer(300);
}

MixerView::~MixerView() { 
    stopTimer(); 
}

void MixerView::paint(juce::Graphics& g) { 
    g.fillAll(Style::colBackground); 
    
    // Draw section separator (only one now - between inputs and outputs)
    g.setColour(juce::Colours::white.withAlpha(0.1f));
    
    int rowHeight = getHeight() / 2;
    g.drawLine(0, (float)rowHeight, (float)getWidth(), (float)rowHeight, 1.0f);
}

void MixerView::resized() { 
    auto bounds = getLocalBounds();
    int rowHeight = bounds.getHeight() / 2;
    
    // OnStage: Effects-only mode — inputs and outputs take full space
    inputsRow.setBounds(bounds.removeFromTop(rowHeight));
    outputsRow.setBounds(bounds);
}

void MixerView::timerCallback() { 
    updateInputsRow();
    updateOutputsRow();
    // OnStage: Effects-only mode — no instruments row
}

void MixerView::updateInputsRow() {
    int inputCount = processor.inputChannelNames.size();
    if (inputCount < 2) inputCount = 2;  // Minimum stereo
    
    if (inputCount != lastInputCount) {
        lastInputCount = inputCount;
        inputsRow.setChannelCount(inputCount, [this](int ch, float g) { onInputGainChanged(ch, g); });
        
        // Set channel names
        for (int i = 0; i < inputCount; ++i) {
            inputsRow.setChannelName(i, processor.getDeviceInputChannelName(i));
        }
    }
}

void MixerView::updateOutputsRow() {
    int outputCount = processor.outputChannelNames.size();
    if (outputCount < 2) outputCount = 2;  // Minimum stereo
    
    if (outputCount != lastOutputCount) {
        lastOutputCount = outputCount;
        outputsRow.setChannelCount(outputCount, [this](int ch, float g) { onOutputGainChanged(ch, g); });
        
        // Set channel names
        for (int i = 0; i < outputCount; ++i) {
            outputsRow.setChannelName(i, processor.getDeviceOutputChannelName(i));
        }
    }
}

void MixerView::onInputGainChanged(int channel, float gain) {
    processor.setInputGain(channel, gain);
}

void MixerView::onOutputGainChanged(int channel, float gain) {
    processor.setOutputGain(channel, gain);
}

// OnStage: Effects-only mode — no instrument gain handling needed

