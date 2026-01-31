// CRITICAL FIX: Use isInstrument() instead of getPluginDescription().isInstrument
// getPluginDescription() freezes some plugins when called!

#include "MixerView.h"
#include "PluginProcessor.h"  // This includes MeteringProcessor

MixerView::MixerView(SubterraneumAudioProcessor& p) : processor(p) {
    addAndMakeVisible(inputsRow);
    addAndMakeVisible(outputsRow);
    addAndMakeVisible(instrumentsRow);
    
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
    
    // CPU OPTIMIZATION: Only iterate through graph nodes when graph structure changes
    size_t currentNodeCount = processor.mainGraph->getNumNodes();
    size_t currentConnectionCount = processor.mainGraph->getConnections().size();
    
    // Skip expensive iteration if graph hasn't changed
    if (currentNodeCount == lastGraphNodeCount && 
        currentConnectionCount == lastGraphConnectionCount &&
        lastInstrumentCount > 0) {
        return;
    }
    
    lastGraphNodeCount = currentNodeCount;
    lastGraphConnectionCount = currentConnectionCount;
    
    // Count instruments
    std::vector<juce::AudioProcessorGraph::Node*> instruments;
    
    for (auto* node : processor.mainGraph->getNodes()) {
        if (auto* mp = dynamic_cast<MeteringProcessor*>(node->getProcessor())) {
            // =========================================================================
            // CRITICAL FIX: Use isInstrument() instead of getPluginDescription()
            // getPluginDescription() freezes some plugins when called!
            // =========================================================================
            if (mp->getInnerPlugin() && mp->isInstrument()) {
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
                // FREEZE FIX: Use cached name
                instrumentsRow.setChannelName(i, mp->getCachedName());
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
