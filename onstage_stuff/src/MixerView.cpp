#include "MixerView.h"
#include "PluginProcessor.h"  // This includes MeteringProcessor

MixerView::MixerView(SubterraneumAudioProcessor& p) : processor(p) {
    addAndMakeVisible(inputsRow);
    addAndMakeVisible(outputsRow);
    addAndMakeVisible(instrumentsRow);
    
    startTimer(200);
}

MixerView::~MixerView() { 
    stopTimer(); 
}

void MixerView::paint(juce::Graphics& g) { 
    g.fillAll(Style::colBackground); 
    
    // Draw section separators
    g.setColour(juce::Colours::white.withAlpha(0.1f));
    
    int rowHeight = getHeight() / 3;
    g.drawLine(0, (float)rowHeight, (float)getWidth(), (float)rowHeight, 1.0f);
    g.drawLine(0, (float)(rowHeight * 2), (float)getWidth(), (float)(rowHeight * 2), 1.0f);
}

void MixerView::resized() { 
    auto bounds = getLocalBounds();
    int rowHeight = bounds.getHeight() / 3;
    
    inputsRow.setBounds(bounds.removeFromTop(rowHeight));
    outputsRow.setBounds(bounds.removeFromTop(rowHeight));
    instrumentsRow.setBounds(bounds);
}

void MixerView::timerCallback() { 
    updateInputsRow();
    updateOutputsRow();
    updateInstrumentsRow();
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

void MixerView::updateInstrumentsRow() {
    if (!processor.mainGraph) return;
    
    // Count instruments
    std::vector<juce::AudioProcessorGraph::Node*> instruments;
    
    for (auto* node : processor.mainGraph->getNodes()) {
        if (auto* mp = dynamic_cast<MeteringProcessor*>(node->getProcessor())) {
            if (mp->getInnerPlugin() && mp->getInnerPlugin()->getPluginDescription().isInstrument) {
                instruments.push_back(node);
            }
        }
    }
    
    int instrumentCount = (int)instruments.size();
    
    if (instrumentCount != lastInstrumentCount) {
        lastInstrumentCount = instrumentCount;
        instrumentNodeIDs.clear();
        
        instrumentsRow.setChannelCount(instrumentCount, [this](int idx, float g) { onInstrumentGainChanged(idx, g); });
        
        for (int i = 0; i < instrumentCount; ++i) {
            auto* node = instruments[i];
            instrumentNodeIDs.push_back(node->nodeID);
            
            if (auto* mp = dynamic_cast<MeteringProcessor*>(node->getProcessor())) {
                instrumentsRow.setChannelName(i, mp->getName());
                instrumentsRow.setGainValue(i, mp->getGain());
            }
        }
    }
}

void MixerView::onInputGainChanged(int channel, float gain) {
    processor.setInputGain(channel, gain);
}

void MixerView::onOutputGainChanged(int channel, float gain) {
    processor.setOutputGain(channel, gain);
}

void MixerView::onInstrumentGainChanged(int index, float gain) {
    if (index >= 0 && index < (int)instrumentNodeIDs.size()) {
        auto nodeID = instrumentNodeIDs[index];
        if (auto* node = processor.mainGraph->getNodeForId(nodeID)) {
            if (auto* mp = dynamic_cast<MeteringProcessor*>(node->getProcessor())) {
                mp->setGain(gain);
            }
        }
    }
}
