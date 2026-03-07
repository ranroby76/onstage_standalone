// I/O Channel Management
// OnStage: Effects-only mode - no MIDI I/O nodes

#include "PluginProcessor.h"

// =============================================================================
// I/O Channel Management
// =============================================================================
void SubterraneumAudioProcessor::updateIOChannelCount() {
    if (!mainGraph) return;
    
    // CRITICAL FIX: Suspend audio processing while modifying graph topology
    // Without this, the audio thread may access nodes being removed/recreated
    suspendProcessing(true);
    
    int numIns = 2;
    int numOuts = 2;

    if (standaloneDeviceManager) {
        if (auto* device = standaloneDeviceManager->getCurrentAudioDevice()) {
            inputChannelNames = device->getInputChannelNames();
            outputChannelNames = device->getOutputChannelNames();
            numIns = inputChannelNames.size();
            numOuts = outputChannelNames.size();
        }
    }
    
    if (numIns < 2) numIns = 2;
    if (numOuts < 2) numOuts = 2;

    mainGraph->setPlayConfigDetails(numIns, numOuts, getSampleRate(), getBlockSize());
    
    // Save positions of audio I/O nodes
    float audioInX = 20, audioInY = 80;
    float audioOutX = 20, audioOutY = 450;
    
    if (audioInputNode) {
        audioInX = (float)audioInputNode->properties.getWithDefault("x", 50.0);
        audioInY = (float)audioInputNode->properties.getWithDefault("y", 50.0);
    }
    if (audioOutputNode) {
        audioOutX = (float)audioOutputNode->properties.getWithDefault("x", 800.0);
        audioOutY = (float)audioOutputNode->properties.getWithDefault("y", 300.0);
    }
    
    // FIX: Store old audio I/O node IDs to identify their connections
    juce::AudioProcessorGraph::NodeID oldAudioInID = audioInputNode ? audioInputNode->nodeID : juce::AudioProcessorGraph::NodeID();
    juce::AudioProcessorGraph::NodeID oldAudioOutID = audioOutputNode ? audioOutputNode->nodeID : juce::AudioProcessorGraph::NodeID();
    
    // Store all connections BEFORE removing audio I/O nodes
    struct SavedConnection {
        juce::AudioProcessorGraph::NodeID sourceNode;
        int sourceChannel;
        juce::AudioProcessorGraph::NodeID destNode;
        int destChannel;
    };
    
    std::vector<SavedConnection> connectionsToRestore;
    
    for (const auto& conn : mainGraph->getConnections()) {
        // Skip connections that involve the old audio I/O nodes (these will be lost)
        bool involvesAudioIO = (conn.source.nodeID == oldAudioInID || 
                                conn.destination.nodeID == oldAudioInID ||
                                conn.source.nodeID == oldAudioOutID || 
                                conn.destination.nodeID == oldAudioOutID);
        
        if (!involvesAudioIO) {
            // Save this connection for restoration
            SavedConnection saved;
            saved.sourceNode = conn.source.nodeID;
            saved.sourceChannel = conn.source.channelIndex;
            saved.destNode = conn.destination.nodeID;
            saved.destChannel = conn.destination.channelIndex;
            connectionsToRestore.push_back(saved);
        }
    }
    
    // Remove audio I/O nodes
    if (audioInputNode) mainGraph->removeNode(audioInputNode->nodeID);
    if (audioOutputNode) mainGraph->removeNode(audioOutputNode->nodeID);
    
    // Create new audio I/O nodes
    audioInputNode = mainGraph->addNode(std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor>(juce::AudioProcessorGraph::AudioGraphIOProcessor::audioInputNode));
    audioOutputNode = mainGraph->addNode(std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor>(juce::AudioProcessorGraph::AudioGraphIOProcessor::audioOutputNode));
    
    // Restore positions
    if (audioInputNode) { audioInputNode->properties.set("x", audioInX); audioInputNode->properties.set("y", audioInY); }
    if (audioOutputNode) { audioOutputNode->properties.set("x", audioOutX); audioOutputNode->properties.set("y", audioOutY); }
    
    // Restore all saved connections
    for (const auto& saved : connectionsToRestore) {
        // Verify nodes still exist
        auto* sourceNode = mainGraph->getNodeForId(saved.sourceNode);
        auto* destNode = mainGraph->getNodeForId(saved.destNode);
        
        if (sourceNode && destNode) {
            // Restore connection
            mainGraph->addConnection({
                { saved.sourceNode, saved.sourceChannel },
                { saved.destNode, saved.destChannel }
            });
        }
    }
    
    if (standaloneDeviceManager) {
        if (auto* device = standaloneDeviceManager->getCurrentAudioDevice()) {
            mainGraph->prepareToPlay(device->getCurrentSampleRate(), device->getCurrentBufferSizeSamples());
        }
    }
    
    suspendProcessing(false);
}

void SubterraneumAudioProcessor::updateActiveChannels() {
}
