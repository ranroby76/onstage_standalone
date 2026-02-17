
// D:\Workspace\Subterraneum_plugins_daw\src\PluginProcessor_05_Graph.cpp
// Graph Management and Node Management
// CRITICAL FIX: Use isInstrument() instead of getPluginDescription().isInstrument
// BUG FIX: Proper frozen state management for multi-mode switching

#include "PluginProcessor.h"

// =============================================================================
// Graph Management
// =============================================================================
bool SubterraneumAudioProcessor::isBusesLayoutSupported(const BusesLayout& /*layouts*/) const {
    return true; 
}

void SubterraneumAudioProcessor::updateGraph() {
    suspendProcessing(true);
    
    audioInputNode = nullptr;
    audioOutputNode = nullptr;
    midiInputNode = nullptr;
    midiOutputNode = nullptr;
    
    mainGraph->clear();
    
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

    audioInputNode = mainGraph->addNode(std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor>(juce::AudioProcessorGraph::AudioGraphIOProcessor::audioInputNode));
    audioOutputNode = mainGraph->addNode(std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor>(juce::AudioProcessorGraph::AudioGraphIOProcessor::audioOutputNode));
    midiInputNode = mainGraph->addNode(std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor>(juce::AudioProcessorGraph::AudioGraphIOProcessor::midiInputNode));
    midiOutputNode = mainGraph->addNode(std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor>(juce::AudioProcessorGraph::AudioGraphIOProcessor::midiOutputNode));
    
    if (audioInputNode) { audioInputNode->properties.set("x", 20); audioInputNode->properties.set("y", 80); }
    if (midiInputNode) { midiInputNode->properties.set("x", 600); midiInputNode->properties.set("y", 80); }
    if (audioOutputNode) { audioOutputNode->properties.set("x", 20); audioOutputNode->properties.set("y", 450); }
    if (midiOutputNode) { midiOutputNode->properties.set("x", 600); midiOutputNode->properties.set("y", 450); }
    
    suspendProcessing(false);
}

void SubterraneumAudioProcessor::resetGraph() {
    // CRITICAL: Send panic to all instruments before clearing graph
    sendMidiPanicToAllInstruments();
    
    updateGraph();
}

// =============================================================================
// Node Management - CRITICAL MIDI PANIC FIXES + I/O NODE PROTECTION
// =============================================================================
void SubterraneumAudioProcessor::removeNode(juce::AudioProcessorGraph::NodeID nodeID) {
    if (!mainGraph) return;
    
    // PROTECTION: Prevent deletion of I/O nodes
    auto* node = mainGraph->getNodeForId(nodeID);
    if (!node) return;
    
    if (node == audioInputNode.get() || 
        node == audioOutputNode.get() || 
        node == midiInputNode.get() || 
        node == midiOutputNode.get())
    {
        return; // Silently ignore deletion attempts on I/O nodes
    }
    
    // =========================================================================
    // CRITICAL FIX: Suspend processing FIRST to prevent crashes
    // Old code called sendAllNotesOffToPlugin() while audio thread was active
    // → race condition → crash
    // =========================================================================
    suspendProcessing(true);
    
    // REMOVED: sendAllNotesOffToPlugin() call - causes crashes
    // When we remove a node, suspending processing is sufficient
    // The node will be disconnected and won't produce sound anymore
    
    mainGraph->removeNode(nodeID);
    suspendProcessing(false);
}

void SubterraneumAudioProcessor::toggleBypass(juce::AudioProcessorGraph::NodeID nodeID) {
    if (auto* node = mainGraph->getNodeForId(nodeID)) {
        bool newBypassState = !node->isBypassed();
        
        if (auto* meteringProc = dynamic_cast<MeteringProcessor*>(node->getProcessor())) {
            // =========================================================================
            // CRITICAL FIX: Freeze FIRST to stop audio processing
            // This prevents race conditions between UI thread and audio thread
            // =========================================================================
            meteringProc->setFrozen(newBypassState);
            
            // =========================================================================
            // REMOVED: sendAllNotesOffToPlugin() - CAUSES CRASHES!
            // Problem: It calls innerPlugin->processBlock() from UI thread while
            // audio thread might still be processing → RACE CONDITION → CRASH
            // Solution: Freezing the plugin stops processing, which is enough.
            // Notes will be released naturally when the plugin is unfrozen.
            // =========================================================================
        }
        
        node->setBypassed(newBypassState);
    }
}

// =============================================================================
// Multi-Mode State Management - BUG FIX for mode switching crashes
// =============================================================================
void SubterraneumAudioProcessor::storeMultiStates() {
    // Store the current bypass state of all nodes before switching to single mode
    multiModeBypassStates.clear();
    for(auto* node : mainGraph->getNodes()) {
        multiModeBypassStates[node->nodeID] = node->isBypassed();
    }
}

void SubterraneumAudioProcessor::restoreMultiStates() {
    // CRITICAL: This is called when switching back to multi-mode
    // We must restore both bypass AND frozen states properly
    for(auto const& [id, bypassed] : multiModeBypassStates) {
        if(auto* node = mainGraph->getNodeForId(id)) {
            // Restore bypass state
            node->setBypassed(bypassed);
            
            // CRITICAL FIX: Ensure frozen state matches bypass state
            // Bypassed = Frozen, Active = Unfrozen
            // This prevents plugins from staying frozen after mode switch
            if (auto* meteringProc = dynamic_cast<MeteringProcessor*>(node->getProcessor())) {
                meteringProc->setFrozen(bypassed);
            }
        }
    }
}

void SubterraneumAudioProcessor::resetBlacklist() {
    knownPluginList.clearBlacklistedFiles();
    if (auto* userSettings = appProperties.getUserSettings()) {
        if (auto xml = knownPluginList.createXml())
            userSettings->setValue("KnownPlugins", xml.get());
        userSettings->saveIfNeeded();
    }
}


