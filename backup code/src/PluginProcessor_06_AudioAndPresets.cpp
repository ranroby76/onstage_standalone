
#include "PluginProcessor.h"
#include <vector>
#include "PluginEditor.h"
#include "RegistrationManager.h"
#include "SimpleConnectorProcessor.h"
#include "StereoMeterProcessor.h"
#include "RecorderProcessor.h"
#include "TransientSplitterProcessor.h"
#include "ContainerProcessor.h"

// =============================================================================
// Audio Processing - CPU OPTIMIZED + FIX 3: Latency Compensation
// =============================================================================
void SubterraneumAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    int inputs = getTotalNumInputChannels();
    int outputs = getTotalNumOutputChannels();
    
    if (standaloneDeviceManager) {
        if (auto* device = standaloneDeviceManager->getCurrentAudioDevice()) {
            auto inputNames = device->getInputChannelNames();
            auto outputNames = device->getOutputChannelNames();
            inputs = inputNames.size();
            outputs = outputNames.size();
        }
    }
    
    if (inputs < 2) inputs = 2;
    if (outputs < 2) outputs = 2;

    mainGraph->setPlayConfigDetails(inputs, outputs, sampleRate, samplesPerBlock);
    mainGraph->prepareToPlay(sampleRate, samplesPerBlock);
    
    // CPU FIX: Reset meter counter on prepare
    meterUpdateCounter = 0;
}

void SubterraneumAudioProcessor::releaseResources() {
    if (mainGraph)
        mainGraph->releaseResources();
}

void SubterraneumAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    if (!mainGraph) {
        buffer.clear();
        midiMessages.clear();
        return;
    }
    
    // CPU FIX #1: REMOVED updateDemoMode() call!
    // updateDemoMode() is already called in UI timers (PluginEditor::timerCallback)
    // Just read the atomic state - this is very cheap
    bool demoSilence = RegistrationManager::getInstance().isDemoSilenceActive();
    
    // CPU FIX #2: Throttle meter updates - only calculate RMS every Nth buffer
    bool shouldUpdateMeters = (++meterUpdateCounter >= meterUpdateInterval);
    if (shouldUpdateMeters) {
        meterUpdateCounter = 0;
    }
    
    // If audio input is disabled OR demo silence is active, clear the input buffer
    if (!audioInputEnabled.load() || demoSilence) {
        buffer.clear();
        mainInputRms[0] = 0.0f;
        mainInputRms[1] = 0.0f;
        
        // FIX: Send All-Notes-Off to kill oscillators/stuck instruments
        // This prevents "caged leftover sound" from plugins with internal oscillators
        static int audioOffPanicCounter = 0;
        if (++audioOffPanicCounter >= 10) {  // Send every 10 buffers to ensure it's received
            audioOffPanicCounter = 0;
            for (int ch = 1; ch <= 16; ++ch) {
                midiMessages.addEvent(juce::MidiMessage::allNotesOff(ch), 0);
                midiMessages.addEvent(juce::MidiMessage::allSoundOff(ch), 0);
            }
        }
    } else {
        // Apply input gains (before graph processing)
        for (int ch = 0; ch < buffer.getNumChannels() && ch < maxMixerChannels; ++ch) {
            float gain = inputGains[ch].load();
            if (gain != 1.0f) {
                buffer.applyGain(ch, 0, buffer.getNumSamples(), gain);
            }
        }
        
        // CPU FIX #3: Only calculate input RMS when needed (throttled)
        if (shouldUpdateMeters) {
            if (buffer.getNumChannels() > 0) mainInputRms[0] = buffer.getRMSLevel(0, 0, buffer.getNumSamples());
            if (buffer.getNumChannels() > 1) mainInputRms[1] = buffer.getRMSLevel(1, 0, buffer.getNumSamples());
        }
    }
    
    // FIX: MIDI input control with All-Notes-Off on disable
    bool midiEnabled = midiInputEnabled.load() && !demoSilence;
    
    if (!midiEnabled) {
        // Clear all incoming MIDI
        midiMessages.clear();
        
        // Send All-Notes-Off on all channels to kill stuck notes
        static int allNotesOffCounter = 0;
        if (++allNotesOffCounter >= 10) {  // Send every 10 buffers to ensure it's received
            allNotesOffCounter = 0;
            for (int ch = 1; ch <= 16; ++ch) {
                midiMessages.addEvent(juce::MidiMessage::allNotesOff(ch), 0);
                midiMessages.addEvent(juce::MidiMessage::allSoundOff(ch), 0);
            }
        }
        
        mainMidiInFlash = false;
    } else {
        // HARDWARE MIDI CHANNEL FILTERING - Apply before virtual keyboard
        // This filters hardware MIDI based on per-device channel masks
        applyHardwareMidiChannelFiltering(midiMessages);
        
        // Process virtual keyboard MIDI (only when MIDI input is enabled)
        keyboardState.processNextMidiBuffer(midiMessages, 0, buffer.getNumSamples(), true);
        
        // Track MIDI input activity
        if (!midiMessages.isEmpty()) {
            mainMidiInNoteCount++;
            mainMidiInFlash = true;
        } else {
            mainMidiInFlash = false;
        }
    }

    // ==========================================================================
    // CRITICAL FIX: MIDI preprocessing to fix note hanging issues
    // Convert velocity=0 to note-off and filter duplicate note-ons
    // ==========================================================================
    if (!midiMessages.isEmpty())
    {
        juce::MidiBuffer preprocessed;
        juce::Array<int> activeNotes;  // Track currently playing notes
        
        for (const auto metadata : midiMessages)
        {
            auto msg = metadata.getMessage();
            int sample = metadata.samplePosition;
            
            // FIX 1: Convert velocity=0 note-ons to proper note-offs
            if (msg.isNoteOn() && msg.getVelocity() == 0)
            {
                msg = juce::MidiMessage::noteOff(msg.getChannel(), msg.getNoteNumber());
            }
            
            // FIX 2: Filter duplicate note-ons (same note already playing)
            if (msg.isNoteOn())
            {
                int noteNum = msg.getNoteNumber();
                if (activeNotes.contains(noteNum))
                {
                    // Note is already playing - send note-off first
                    preprocessed.addEvent(
                        juce::MidiMessage::noteOff(msg.getChannel(), noteNum),
                        sample);
                }
                activeNotes.addIfNotAlreadyThere(noteNum);
                preprocessed.addEvent(msg, sample);
            }
            else if (msg.isNoteOff())
            {
                int noteNum = msg.getNoteNumber();
                activeNotes.removeAllInstancesOf(noteNum);
                preprocessed.addEvent(msg, sample);
            }
            else
            {
                // Pass through all other MIDI messages
                preprocessed.addEvent(msg, sample);
            }
        }
        
        midiMessages.swapWith(preprocessed);
    }

    // Process the graph with crash protection
    try {
        mainGraph->processBlock(buffer, midiMessages);
    } catch (...) {
        buffer.clear();
        midiMessages.clear();
    }
    
    // If audio output is disabled OR demo silence is active, clear the output buffer
    if (!audioOutputEnabled.load() || demoSilence) {
        buffer.clear();
        mainOutputRms[0] = 0.0f;
        mainOutputRms[1] = 0.0f;
    } else {
        // Apply output gains (after graph processing)
        for (int ch = 0; ch < buffer.getNumChannels() && ch < maxMixerChannels; ++ch) {
            float gain = outputGains[ch].load();
            if (gain != 1.0f) {
                buffer.applyGain(ch, 0, buffer.getNumSamples(), gain);
            }
        }

        // CPU FIX #4: Only calculate output RMS when needed (throttled)
        if (shouldUpdateMeters) {
            if (buffer.getNumChannels() > 0) mainOutputRms[0] = buffer.getRMSLevel(0, 0, buffer.getNumSamples());
            if (buffer.getNumChannels() > 1) mainOutputRms[1] = buffer.getRMSLevel(1, 0, buffer.getNumSamples());
        }
    }
    
    // Recording - capture final output
    if (isRecording.load() && backgroundWriter != nullptr) {
        backgroundWriter->write(buffer.getArrayOfReadPointers(), buffer.getNumSamples());
    }
}

// =============================================================================
// Recording Methods
// =============================================================================
bool SubterraneumAudioProcessor::startRecording() {
    stopRecording();
    
    if (!writerThread.isThreadRunning())
        writerThread.startThread();
    
    double sampleRate = getSampleRate();
    if (sampleRate <= 0) sampleRate = 44100.0;
    
    lastRecordingFile = juce::File::getSpecialLocation(juce::File::userMusicDirectory)
                            .getNonexistentChildFile("OnStage_Recording", ".wav");
    
    auto* wavFormat = new juce::WavAudioFormat();
    auto* outputStream = new juce::FileOutputStream(lastRecordingFile);
    
    if (outputStream->failedToOpen()) {
        delete outputStream;
        delete wavFormat;
        return false;
    }
    
    auto* writer = wavFormat->createWriterFor(outputStream, sampleRate, juce::AudioChannelSet::stereo(), 24, {}, 0);
    
    if (writer != nullptr) {
        backgroundWriter.reset(new juce::AudioFormatWriter::ThreadedWriter(writer, writerThread, 32768));
        isRecording.store(true);
        delete wavFormat;
        return true;
    }
    
    delete wavFormat;
    return false;
}

void SubterraneumAudioProcessor::stopRecording() {
    isRecording.store(false);
    backgroundWriter.reset();
}

// =============================================================================
// Editor
// =============================================================================
juce::AudioProcessorEditor* SubterraneumAudioProcessor::createEditor() {
    return new SubterraneumAudioProcessorEditor(*this);
}
bool SubterraneumAudioProcessor::hasEditor() const { return true; }

// =============================================================================
// Plugin Info
// =============================================================================
const juce::String SubterraneumAudioProcessor::getName() const { return "OnStage"; }
bool SubterraneumAudioProcessor::acceptsMidi() const { return true; }
bool SubterraneumAudioProcessor::producesMidi() const { return true; }
bool SubterraneumAudioProcessor::isMidiEffect() const { return false; }
double SubterraneumAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int SubterraneumAudioProcessor::getNumPrograms() { return 1; }
int SubterraneumAudioProcessor::getCurrentProgram() { return 0; }
void SubterraneumAudioProcessor::setCurrentProgram(int index) { juce::ignoreUnused(index); }
const juce::String SubterraneumAudioProcessor::getProgramName(int index) { juce::ignoreUnused(index); return {}; }
void SubterraneumAudioProcessor::changeProgramName(int index, const juce::String& newName) { juce::ignoreUnused(index, newName); }

// =============================================================================
// State
// =============================================================================
void SubterraneumAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    juce::XmlElement xml("OnStageState");
    copyXmlToBinary(xml, destData);
}

void SubterraneumAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
}

// =============================================================================
// Preset Save/Load
// =============================================================================
void SubterraneumAudioProcessor::loadUserPreset(const juce::File& file) {
    suspendProcessing(true);
    
    auto xml = juce::parseXML(file);
    if (!xml) {
        suspendProcessing(false);
        return;
    }
    
    std::map<int, juce::AudioProcessorGraph::NodeID> nodeIdMap;
    
    instrumentSelectorMultiMode = xml->getBoolAttribute("multiMode", false);
    audioInputEnabled.store(xml->getBoolAttribute("audioInputEnabled", true));
    audioOutputEnabled.store(xml->getBoolAttribute("audioOutputEnabled", true));
    midiInputEnabled.store(xml->getBoolAttribute("midiInputEnabled", true));
    rackZoomLevel = juce::jlimit(0.25f, 1.0f, (float)xml->getDoubleAttribute("rackZoomLevel", 1.0));
    
    {
        std::vector<juce::AudioProcessorGraph::NodeID> toRemove;
        for (auto* node : mainGraph->getNodes()) {
            if (node != audioInputNode.get() &&
                node != audioOutputNode.get() &&
                node != playbackNode.get()) {

                toRemove.push_back(node->nodeID);
            }
        }
        for (auto nid : toRemove)
            mainGraph->removeNode(nid);
    }
    
    if (auto* nodes = xml->getChildByName("Nodes")) {
        for (auto* nodeXml : nodes->getChildIterator()) {
            if (nodeXml->hasTagName("Node")) {
                juce::String type = nodeXml->getStringAttribute("type");
                int oldId = nodeXml->getIntAttribute("id");
                double x = nodeXml->getDoubleAttribute("x");
                double y = nodeXml->getDoubleAttribute("y");
                
                if (type == "AudioInput" && audioInputNode) {
                    nodeIdMap[oldId] = audioInputNode->nodeID;
                    audioInputNode->properties.set("x", x);
                    audioInputNode->properties.set("y", y);
                } else if (type == "AudioOutput" && audioOutputNode) {
                    nodeIdMap[oldId] = audioOutputNode->nodeID;
                    audioOutputNode->properties.set("x", x);
                    audioOutputNode->properties.set("y", y);
                }
            }
        }
    }

    if (auto* nodes = xml->getChildByName("Nodes")) {
        for (auto* nodeXml : nodes->getChildIterator()) {
            if (nodeXml->hasTagName("Node")) {
                juce::String type = nodeXml->getStringAttribute("type");
                int oldId = nodeXml->getIntAttribute("id");
                double x = nodeXml->getDoubleAttribute("x");
                double y = nodeXml->getDoubleAttribute("y");
                
                // === System Tools ===
                if (type == "SystemTool") {
                    juce::String toolName = nodeXml->getStringAttribute("toolName");
                    std::unique_ptr<juce::AudioProcessor> toolProc;
                    
                    if (toolName == "SimpleConnector")       toolProc = std::make_unique<SimpleConnectorProcessor>();
                    else if (toolName == "StereoMeter")      toolProc = std::make_unique<StereoMeterProcessor>();
                    else if (toolName == "Recorder")          toolProc = std::make_unique<RecorderProcessor>();
                    else if (toolName == "TransientSplitter") toolProc = std::make_unique<TransientSplitterProcessor>();
                    else if (toolName == "Container") {
                        auto container = std::make_unique<ContainerProcessor>();
                        container->setParentProcessor(this);
                        toolProc = std::move(container);
                    }
                    
                    if (toolProc) {
                        // Restore state before adding to graph
                        if (auto* stateXml = nodeXml->getChildByName("State")) {
                            juce::MemoryBlock mb;
                            mb.fromBase64Encoding(stateXml->getAllSubText());
                            if (mb.getSize() > 0) {
                                try {
                                    toolProc->setStateInformation(mb.getData(), (int)mb.getSize());
                                } catch (...) {}
                            }
                        }
                        
                        auto nodePtr = mainGraph->addNode(std::move(toolProc));
                        if (nodePtr) {
                            nodeIdMap[oldId] = nodePtr->nodeID;
                            nodePtr->properties.set("x", x);
                            nodePtr->properties.set("y", y);
                        }
                    }
                    continue;
                }
                
                if (type != "Plugin") continue; 

                juce::PluginDescription desc;
                desc.fileOrIdentifier = nodeXml->getStringAttribute("identifier");
                desc.uniqueId = nodeXml->getIntAttribute("uid");
                desc.name = nodeXml->getStringAttribute("name");
                desc.pluginFormatName = nodeXml->getStringAttribute("format");

                juce::String msg;
                std::unique_ptr<juce::AudioPluginInstance> instance;
                
                try {
                    instance = formatManager.createPluginInstance(desc, getSampleRate(), getBlockSize(), msg);
                } catch (...) {}
                
                // Fallback: try alternate UID if first attempt failed
                if (!instance) {
                    for (auto& alt : knownPluginList.getTypes()) {
                        if (alt.name == desc.name && alt.pluginFormatName == desc.pluginFormatName && alt.uniqueId != desc.uniqueId) {
                            try {
                                juce::String altMsg;
                                instance = formatManager.createPluginInstance(alt, getSampleRate(), getBlockSize(), altMsg);
                                if (instance) break;
                            } catch (...) {}
                        }
                    }
                }
                
                
                // Nuclear fallback: re-scan .vst3 in-process for correct UIDs
                if (!instance && desc.pluginFormatName == "VST3") {
                    for (int fi = 0; fi < formatManager.getNumFormats(); ++fi) {
                        auto* format = formatManager.getFormat(fi);
                        if (format->getName() != "VST3") continue;
                        juce::OwnedArray<juce::PluginDescription> freshDescs;
                        format->findAllTypesForFile(freshDescs, desc.fileOrIdentifier);
                        for (auto* fresh : freshDescs) {
                            if (fresh->name == desc.name) {
                                try {
                                    juce::String freshMsg;
                                    instance = formatManager.createPluginInstance(*fresh, getSampleRate(), getBlockSize(), freshMsg);
                                    if (instance) { knownPluginList.addType(*fresh); break; }
                                } catch (...) {}
                            }
                        }
                        break;
                    }
                }
                if (!instance) continue;
                
                if (instance) {
                    auto nodePtr = mainGraph->addNode(std::make_unique<MeteringProcessor>(std::move(instance)));
                    auto* node = nodePtr.get();
                    if (node) {
                        nodeIdMap[oldId] = node->nodeID;
                        
                        node->properties.set("x", nodeXml->getDoubleAttribute("x"));
                        node->properties.set("y", nodeXml->getDoubleAttribute("y"));
                        
                        bool isBypassed = nodeXml->getBoolAttribute("bypassed", false);
                        node->setBypassed(isBypassed);
                        
                        if (auto* mp = dynamic_cast<MeteringProcessor*>(node->getProcessor())) {
                            mp->setMidiChannelMask(nodeXml->getIntAttribute("midiChannelMask", 0x0001));  // Default to channel 1
                            mp->setPassThrough(nodeXml->getBoolAttribute("passThrough", false));
                            mp->setFrozen(isBypassed);
                            
                            if (auto* stateXml = nodeXml->getChildByName("State")) {
                                juce::MemoryBlock mb;
                                mb.fromBase64Encoding(stateXml->getAllSubText());
                                try {
                                    mp->getInnerPlugin()->setStateInformation(mb.getData(), (int)mb.getSize());
                                } catch (...) {
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    if (auto* conns = xml->getChildByName("Connections")) {
        for (auto* cXml : conns->getChildIterator()) {
            if (cXml->hasTagName("Connection")) {
                int srcOldId = cXml->getIntAttribute("srcNode");
                int dstOldId = cXml->getIntAttribute("dstNode");
                int srcCh = cXml->getIntAttribute("srcCh");
                int dstCh = cXml->getIntAttribute("dstCh");
                bool isMidi = cXml->getBoolAttribute("srcMidi");
                
                auto srcIt = nodeIdMap.find(srcOldId);
                auto dstIt = nodeIdMap.find(dstOldId);
                
                if (srcIt != nodeIdMap.end() && dstIt != nodeIdMap.end()) {
                    juce::AudioProcessorGraph::Connection conn;
                    conn.source.nodeID = srcIt->second;
                    conn.destination.nodeID = dstIt->second;
                    
                    if (isMidi) {
                        conn.source.channelIndex = juce::AudioProcessorGraph::midiChannelIndex;
                        conn.destination.channelIndex = juce::AudioProcessorGraph::midiChannelIndex;
                    } else {
                        conn.source.channelIndex = srcCh;
                        conn.destination.channelIndex = dstCh;
                    }
                    
                    mainGraph->addConnection(conn);
                }
            }
        }
    }
    
    // FIX 2: Second pass - detect and enable sidechain after all connections are loaded
    for (auto* node : mainGraph->getNodes()) {
        if (auto* meteringProc = dynamic_cast<MeteringProcessor*>(node->getProcessor())) {
            if (meteringProc->hasSidechain()) {
                // Check if this node has any connections to sidechain inputs (channels 2-3)
                bool hasSidechainConnection = false;
                for (auto& conn : mainGraph->getConnections()) {
                    if (conn.destination.nodeID == node->nodeID && 
                        !conn.destination.isMIDI() &&
                        conn.destination.channelIndex >= 2) {
                        hasSidechainConnection = true;
                        break;
                    }
                }
                
                // Enable or disable sidechain based on connections
                if (hasSidechainConnection) {
                    meteringProc->enableSidechain();
                } else {
                    meteringProc->disableSidechain();
                }
            }
        }
    }
    
    suspendProcessing(false);
}

void SubterraneumAudioProcessor::saveUserPreset(const juce::File& file) {
    juce::XmlElement root("OnStagePatch");
    
    root.setAttribute("multiMode", instrumentSelectorMultiMode);
    root.setAttribute("audioInputEnabled", audioInputEnabled.load());
    root.setAttribute("audioOutputEnabled", audioOutputEnabled.load());
    root.setAttribute("midiInputEnabled", midiInputEnabled.load());
    root.setAttribute("rackZoomLevel", (double)rackZoomLevel);
    
    auto* nodes = root.createNewChildElement("Nodes");
    for (auto* node : mainGraph->getNodes()) {
        auto* nodeXml = nodes->createNewChildElement("Node");
        
        auto* proc = node->getProcessor();
        bool isIO = (dynamic_cast<juce::AudioProcessorGraph::AudioGraphIOProcessor*>(proc) != nullptr);
        
        nodeXml->setAttribute("id", (int)node->nodeID.uid);
        nodeXml->setAttribute("x", (double)node->properties["x"]);
        nodeXml->setAttribute("y", (double)node->properties["y"]);
        nodeXml->setAttribute("bypassed", node->isBypassed());
        
        if (isIO) {
            if (node == audioInputNode.get()) nodeXml->setAttribute("type", "AudioInput");
            else if (node == audioOutputNode.get()) nodeXml->setAttribute("type", "AudioOutput");
            else nodeXml->setAttribute("type", "IO");
        } else {
            // Check if this is a system tool (non-plugin processor)
            juce::String toolName;
            if (dynamic_cast<SimpleConnectorProcessor*>(proc))        toolName = "SimpleConnector";
            else if (dynamic_cast<StereoMeterProcessor*>(proc))       toolName = "StereoMeter";
            else if (dynamic_cast<RecorderProcessor*>(proc))          toolName = "Recorder";
            else if (dynamic_cast<TransientSplitterProcessor*>(proc)) toolName = "TransientSplitter";
            else if (dynamic_cast<ContainerProcessor*>(proc))         toolName = "Container";
            
            if (toolName.isNotEmpty()) {
                nodeXml->setAttribute("type", "SystemTool");
                nodeXml->setAttribute("toolName", toolName);
                
                // Save system tool state
                juce::MemoryBlock mb;
                proc->getStateInformation(mb);
                if (mb.getSize() > 0) {
                    auto* state = nodeXml->createNewChildElement("State");
                    state->addTextElement(mb.toBase64Encoding());
                }
            } else {
                nodeXml->setAttribute("type", "Plugin");
            }
            
            if (auto* mp = dynamic_cast<MeteringProcessor*>(proc)) {
                // FREEZE FIX: Use cached description instead of calling getPluginDescription()
                auto desc = mp->getCachedDescription();
                nodeXml->setAttribute("name", desc.name);
                nodeXml->setAttribute("identifier", desc.identifier);
                nodeXml->setAttribute("format", desc.format);
                nodeXml->setAttribute("uid", desc.uniqueId);
                nodeXml->setAttribute("midiChannelMask", mp->getMidiChannelMask());
                nodeXml->setAttribute("passThrough", mp->isPassThrough());

                juce::MemoryBlock mb;
                mp->getInnerPlugin()->getStateInformation(mb);
                auto* state = nodeXml->createNewChildElement("State");
                state->addTextElement(mb.toBase64Encoding());
            }
        }
    }

    auto* conns = root.createNewChildElement("Connections");
    for (auto& conn : mainGraph->getConnections()) {
        auto* cXml = conns->createNewChildElement("Connection");
        cXml->setAttribute("srcNode", (int)conn.source.nodeID.uid);
        cXml->setAttribute("srcCh", conn.source.channelIndex);
        cXml->setAttribute("srcMidi", conn.source.isMIDI());
        cXml->setAttribute("dstNode", (int)conn.destination.nodeID.uid);
        cXml->setAttribute("dstCh", conn.destination.channelIndex);
        cXml->setAttribute("dstMidi", conn.destination.isMIDI());
    }

    root.writeTo(file);
}

// =============================================================================
// =============================================================================
// Graph Serialization — used by save/load presets
// =============================================================================
juce::String SubterraneumAudioProcessor::serializeGraphToXml() const
{
    juce::XmlElement root("OnStagePatch");
    
    root.setAttribute("multiMode", instrumentSelectorMultiMode);
    root.setAttribute("audioInputEnabled", audioInputEnabled.load());
    root.setAttribute("audioOutputEnabled", audioOutputEnabled.load());
    root.setAttribute("midiInputEnabled", midiInputEnabled.load());
    
    root.setAttribute("rackZoomLevel", (double)rackZoomLevel);
    auto* nodes = root.createNewChildElement("Nodes");
    for (auto* node : mainGraph->getNodes())
    {
        auto* nodeXml = nodes->createNewChildElement("Node");
        auto* proc = node->getProcessor();
        bool isIO = (dynamic_cast<juce::AudioProcessorGraph::AudioGraphIOProcessor*>(proc) != nullptr);
        
        nodeXml->setAttribute("id", (int)node->nodeID.uid);
        nodeXml->setAttribute("x", (double)node->properties["x"]);
        nodeXml->setAttribute("y", (double)node->properties["y"]);
        nodeXml->setAttribute("bypassed", node->isBypassed());
        
        if (isIO) {
            if (node == audioInputNode.get()) nodeXml->setAttribute("type", "AudioInput");
            else if (node == audioOutputNode.get()) nodeXml->setAttribute("type", "AudioOutput");
            else nodeXml->setAttribute("type", "IO");
        } else {
            // Check if this is a system tool (non-plugin processor)
            juce::String toolName;
            if (dynamic_cast<SimpleConnectorProcessor*>(proc))   toolName = "SimpleConnector";
            else if (dynamic_cast<StereoMeterProcessor*>(proc))  toolName = "StereoMeter";
            else if (dynamic_cast<RecorderProcessor*>(proc))     toolName = "Recorder";
            else if (dynamic_cast<TransientSplitterProcessor*>(proc)) toolName = "TransientSplitter";
            else if (dynamic_cast<ContainerProcessor*>(proc))         toolName = "Container";
            
            if (toolName.isNotEmpty()) {
                nodeXml->setAttribute("type", "SystemTool");
                nodeXml->setAttribute("toolName", toolName);
                
                // Save system tool state
                juce::MemoryBlock mb;
                proc->getStateInformation(mb);
                if (mb.getSize() > 0) {
                    auto* state = nodeXml->createNewChildElement("State");
                    state->addTextElement(mb.toBase64Encoding());
                }
            } else {
                nodeXml->setAttribute("type", "Plugin");
            }
            
            if (auto* mp = dynamic_cast<MeteringProcessor*>(proc)) {
                auto desc = mp->getCachedDescription();
                nodeXml->setAttribute("name", desc.name);
                nodeXml->setAttribute("identifier", desc.identifier);
                nodeXml->setAttribute("format", desc.format);
                nodeXml->setAttribute("uid", desc.uniqueId);
                nodeXml->setAttribute("midiChannelMask", mp->getMidiChannelMask());
                nodeXml->setAttribute("passThrough", mp->isPassThrough());
                
                juce::MemoryBlock mb;
                mp->getInnerPlugin()->getStateInformation(mb);
                auto* state = nodeXml->createNewChildElement("State");
                state->addTextElement(mb.toBase64Encoding());
            }
        }
    }
    
    auto* conns = root.createNewChildElement("Connections");
    for (auto& conn : mainGraph->getConnections())
    {
        auto* cXml = conns->createNewChildElement("Connection");
        cXml->setAttribute("srcNode", (int)conn.source.nodeID.uid);
        cXml->setAttribute("srcCh", conn.source.channelIndex);
        cXml->setAttribute("srcMidi", conn.source.isMIDI());
        cXml->setAttribute("dstNode", (int)conn.destination.nodeID.uid);
        cXml->setAttribute("dstCh", conn.destination.channelIndex);
        cXml->setAttribute("dstMidi", conn.destination.isMIDI());
    }
    
    return root.toString();
}

void SubterraneumAudioProcessor::restoreGraphFromXml(const juce::String& xmlStr, bool skipClear)
{
    auto xml = juce::parseXML(xmlStr);
    if (!xml) return;
    
    std::map<int, juce::AudioProcessorGraph::NodeID> nodeIdMap;
    
    instrumentSelectorMultiMode = xml->getBoolAttribute("multiMode", false);
    audioInputEnabled.store(xml->getBoolAttribute("audioInputEnabled", true));
    audioOutputEnabled.store(xml->getBoolAttribute("audioOutputEnabled", true));
    midiInputEnabled.store(xml->getBoolAttribute("midiInputEnabled", true));
    
    rackZoomLevel = juce::jlimit(0.25f, 1.0f, (float)xml->getDoubleAttribute("rackZoomLevel", 1.0));
    // Remove all non-IO nodes (skipped when caller already cleared)
    if (!skipClear)
    {
        std::vector<juce::AudioProcessorGraph::NodeID> toRemove;
        for (auto* node : mainGraph->getNodes()) {
            if (node != audioInputNode.get() &&
                node != audioOutputNode.get() &&
                node != playbackNode.get()) {

                toRemove.push_back(node->nodeID);
            }
        }
        for (auto nid : toRemove)
            mainGraph->removeNode(nid);
    }
    
    // Map IO nodes
    if (auto* nodesXml = xml->getChildByName("Nodes")) {
        for (auto* nodeXml : nodesXml->getChildIterator()) {
            if (nodeXml->hasTagName("Node")) {
                juce::String type = nodeXml->getStringAttribute("type");
                int oldId = nodeXml->getIntAttribute("id");
                double x = nodeXml->getDoubleAttribute("x");
                double y = nodeXml->getDoubleAttribute("y");
                
                if (type == "AudioInput" && audioInputNode) {
                    nodeIdMap[oldId] = audioInputNode->nodeID;
                    audioInputNode->properties.set("x", x);
                    audioInputNode->properties.set("y", y);
                } else if (type == "AudioOutput" && audioOutputNode) {
                    nodeIdMap[oldId] = audioOutputNode->nodeID;
                    audioOutputNode->properties.set("x", x);
                    audioOutputNode->properties.set("y", y);
                }
            }
        }
    }
    
    // Restore plugin nodes
    if (auto* nodesXml = xml->getChildByName("Nodes")) {
        for (auto* nodeXml : nodesXml->getChildIterator()) {
            if (nodeXml->hasTagName("Node")) {
                juce::String type = nodeXml->getStringAttribute("type");
                int oldId = nodeXml->getIntAttribute("id");
                double x = nodeXml->getDoubleAttribute("x");
                double y = nodeXml->getDoubleAttribute("y");
                
                // === System Tools ===
                if (type == "SystemTool") {
                    juce::String toolName = nodeXml->getStringAttribute("toolName");
                    std::unique_ptr<juce::AudioProcessor> toolProc;
                    
                    if (toolName == "SimpleConnector")       toolProc = std::make_unique<SimpleConnectorProcessor>();
                    else if (toolName == "StereoMeter")      toolProc = std::make_unique<StereoMeterProcessor>();
                    else if (toolName == "Recorder")          toolProc = std::make_unique<RecorderProcessor>();
                    else if (toolName == "TransientSplitter") toolProc = std::make_unique<TransientSplitterProcessor>();
                    else if (toolName == "Container")           toolProc = std::make_unique<ContainerProcessor>();
                    
                    if (toolProc) {
                        // Set parentProcessor for containers so they can load plugins inside
                        if (auto* container = dynamic_cast<ContainerProcessor*>(toolProc.get()))
                            container->setParentProcessor(this);
                        // Restore state before adding to graph
                        if (auto* stateXml = nodeXml->getChildByName("State")) {
                            juce::MemoryBlock mb;
                            mb.fromBase64Encoding(stateXml->getAllSubText());
                            if (mb.getSize() > 0) {
                                try {
                                    toolProc->setStateInformation(mb.getData(), (int)mb.getSize());
                                } catch (...) {}
                            }
                        }
                        
                        auto nodePtr = mainGraph->addNode(std::move(toolProc));
                        if (nodePtr) {
                            nodeIdMap[oldId] = nodePtr->nodeID;
                            nodePtr->properties.set("x", x);
                            nodePtr->properties.set("y", y);
                        }
                    }
                    continue;
                }
                
                if (type != "Plugin") continue;
                
                juce::PluginDescription desc;
                desc.fileOrIdentifier = nodeXml->getStringAttribute("identifier");
                desc.uniqueId = nodeXml->getIntAttribute("uid");
                desc.name = nodeXml->getStringAttribute("name");
                desc.pluginFormatName = nodeXml->getStringAttribute("format");
                
                juce::String msg;
                std::unique_ptr<juce::AudioPluginInstance> instance;
                try {
                    instance = formatManager.createPluginInstance(desc, getSampleRate(), getBlockSize(), msg);
                } catch (...) {}
                
                // Fallback: try alternate UID if first attempt failed
                if (!instance) {
                    for (auto& alt : knownPluginList.getTypes()) {
                        if (alt.name == desc.name && alt.pluginFormatName == desc.pluginFormatName && alt.uniqueId != desc.uniqueId) {
                            try {
                                juce::String altMsg;
                                instance = formatManager.createPluginInstance(alt, getSampleRate(), getBlockSize(), altMsg);
                                if (instance) break;
                            } catch (...) {}
                        }
                    }
                }
                
                
                // Nuclear fallback: re-scan .vst3 in-process for correct UIDs
                if (!instance && desc.pluginFormatName == "VST3") {
                    for (int fi = 0; fi < formatManager.getNumFormats(); ++fi) {
                        auto* format = formatManager.getFormat(fi);
                        if (format->getName() != "VST3") continue;
                        juce::OwnedArray<juce::PluginDescription> freshDescs;
                        format->findAllTypesForFile(freshDescs, desc.fileOrIdentifier);
                        for (auto* fresh : freshDescs) {
                            if (fresh->name == desc.name) {
                                try {
                                    juce::String freshMsg;
                                    instance = formatManager.createPluginInstance(*fresh, getSampleRate(), getBlockSize(), freshMsg);
                                    if (instance) { knownPluginList.addType(*fresh); break; }
                                } catch (...) {}
                            }
                        }
                        break;
                    }
                }
                if (!instance) continue;
                
                if (instance) {
                    auto nodePtr = mainGraph->addNode(std::make_unique<MeteringProcessor>(std::move(instance)));
                    auto* node = nodePtr.get();
                    if (node) {
                        nodeIdMap[oldId] = node->nodeID;
                        node->properties.set("x", nodeXml->getDoubleAttribute("x"));
                        node->properties.set("y", nodeXml->getDoubleAttribute("y"));
                        
                        bool isBypassed = nodeXml->getBoolAttribute("bypassed", false);
                        node->setBypassed(isBypassed);
                        
                        if (auto* mp = dynamic_cast<MeteringProcessor*>(node->getProcessor())) {
                            mp->setMidiChannelMask(nodeXml->getIntAttribute("midiChannelMask", 0x0001));
                            mp->setPassThrough(nodeXml->getBoolAttribute("passThrough", false));
                            mp->setFrozen(isBypassed);
                            
                            if (auto* stateXml = nodeXml->getChildByName("State")) {
                                juce::MemoryBlock mb;
                                mb.fromBase64Encoding(stateXml->getAllSubText());
                                try {
                                    mp->getInnerPlugin()->setStateInformation(mb.getData(), (int)mb.getSize());
                                } catch (...) {}
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Restore connections
    if (auto* conns = xml->getChildByName("Connections")) {
        for (auto* cXml : conns->getChildIterator()) {
            if (cXml->hasTagName("Connection")) {
                int srcOldId = cXml->getIntAttribute("srcNode");
                int dstOldId = cXml->getIntAttribute("dstNode");
                int srcCh = cXml->getIntAttribute("srcCh");
                int dstCh = cXml->getIntAttribute("dstCh");
                bool isMidi = cXml->getBoolAttribute("srcMidi");
                
                auto srcIt = nodeIdMap.find(srcOldId);
                auto dstIt = nodeIdMap.find(dstOldId);
                
                if (srcIt != nodeIdMap.end() && dstIt != nodeIdMap.end()) {
                    juce::AudioProcessorGraph::Connection conn;
                    conn.source.nodeID = srcIt->second;
                    conn.destination.nodeID = dstIt->second;
                    if (isMidi) {
                        conn.source.channelIndex = juce::AudioProcessorGraph::midiChannelIndex;
                        conn.destination.channelIndex = juce::AudioProcessorGraph::midiChannelIndex;
                    } else {
                        conn.source.channelIndex = srcCh;
                        conn.destination.channelIndex = dstCh;
                    }
                    mainGraph->addConnection(conn);
                }
            }
        }
    }
    
    // Detect and enable sidechain after all connections loaded
    for (auto* node : mainGraph->getNodes()) {
        if (auto* meteringProc = dynamic_cast<MeteringProcessor*>(node->getProcessor())) {
            if (meteringProc->hasSidechain()) {
                bool hasSidechainConnection = false;
                for (auto& conn : mainGraph->getConnections()) {
                    if (conn.destination.nodeID == node->nodeID && 
                        !conn.destination.isMIDI() &&
                        conn.destination.channelIndex >= 2) {
                        hasSidechainConnection = true;
                        break;
                    }
                }
                if (hasSidechainConnection)
                    meteringProc->enableSidechain();
                else
                    meteringProc->disableSidechain();
            }
        }
    }
}

// =============================================================================
// Plugin Filter Entry Point
// =============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new SubterraneumAudioProcessor();
}

