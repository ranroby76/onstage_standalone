// #D:\Workspace\Subterraneum_plugins_daw\src\ContainerProcessor.cpp
// ContainerProcessor — Sub-graph node implementation
// Wraps an AudioProcessorGraph with configurable I/O buses.
// Supports preset save/load, volume/mute, and serialization for workspace saving.

#include "ContainerProcessor.h"
#include "PluginProcessor.h"
#include "SimpleConnectorProcessor.h"
#include "StereoMeterProcessor.h"
#include "RecorderProcessor.h"
#include "TransientSplitterProcessor.h"

// =============================================================================
// Static member
// =============================================================================
juce::File ContainerProcessor::globalDefaultFolder;

// =============================================================================
// Constructor
// =============================================================================
ContainerProcessor::ContainerProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    // Default stereo I/O so the container works correctly in the graph
    inputBusConfigs.push_back({ "Input 1", 2 });
    outputBusConfigs.push_back({ "Output 1", 2 });
    
    // Create inner graph
    innerGraph = std::make_unique<juce::AudioProcessorGraph>();
    createInnerIONodes();
}

// =============================================================================
// Inner graph I/O nodes
// =============================================================================
void ContainerProcessor::createInnerIONodes()
{
    innerGraph->clear();
    
    // Create Audio I/O nodes (stereo by default)
    innerAudioInput = innerGraph->addNode(
        std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor>(
            juce::AudioProcessorGraph::AudioGraphIOProcessor::audioInputNode));
    
    innerAudioOutput = innerGraph->addNode(
        std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor>(
            juce::AudioProcessorGraph::AudioGraphIOProcessor::audioOutputNode));
    
    // OnStage: No MIDI I/O nodes in containers (effects-only mode)
    innerMidiInput = nullptr;
    innerMidiOutput = nullptr;
    
    // FIX 4: Add ioNodeName properties for tooltip display
    if (innerAudioInput)  innerAudioInput->properties.set("ioNodeName", "Container Audio In");
    if (innerAudioOutput) innerAudioOutput->properties.set("ioNodeName", "Container Audio Out");
    
    // Position I/O nodes on grid (gridSize = 40px):
    if (innerAudioInput)  { innerAudioInput->properties.set("x",  160.0); innerAudioInput->properties.set("y",  120.0); }
    if (innerAudioOutput) { innerAudioOutput->properties.set("x", 160.0); innerAudioOutput->properties.set("y",  440.0); }
}

// =============================================================================
// FIX: Update I/O nodes when bus configuration changes WITHOUT clearing graph
// =============================================================================
void ContainerProcessor::updateInnerIONodes()
{
    if (!innerGraph) return;
    
    // Store all connections (we'll restore user-to-user connections)
    struct SavedConnection {
        juce::AudioProcessorGraph::NodeID sourceNode;
        int sourceChannel;
        bool sourceMidi;
        juce::AudioProcessorGraph::NodeID destNode;
        int destChannel;
        bool destMidi;
    };
    juce::Array<SavedConnection> savedConnections;
    
    for (const auto& conn : innerGraph->getConnections())
    {
        savedConnections.add({
            conn.source.nodeID,
            conn.source.channelIndex,
            conn.source.isMIDI(),
            conn.destination.nodeID,
            conn.destination.channelIndex,
            conn.destination.isMIDI()
        });
    }
    
    // Remove old I/O nodes
    if (innerAudioInput)  innerGraph->removeNode(innerAudioInput->nodeID);
    if (innerAudioOutput) innerGraph->removeNode(innerAudioOutput->nodeID);
    
    // Recreate I/O nodes with new configuration
    innerAudioInput = innerGraph->addNode(
        std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor>(
            juce::AudioProcessorGraph::AudioGraphIOProcessor::audioInputNode));
    
    innerAudioOutput = innerGraph->addNode(
        std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor>(
            juce::AudioProcessorGraph::AudioGraphIOProcessor::audioOutputNode));
    
    // OnStage: No MIDI I/O nodes in containers
    innerMidiInput = nullptr;
    innerMidiOutput = nullptr;
    
    // FIX 4: Add ioNodeName properties for tooltip display
    if (innerAudioInput)  innerAudioInput->properties.set("ioNodeName", "Container Audio In");
    if (innerAudioOutput) innerAudioOutput->properties.set("ioNodeName", "Container Audio Out");
    
    // Position I/O nodes
    if (innerAudioInput)  { innerAudioInput->properties.set("x",  160.0); innerAudioInput->properties.set("y",  120.0); }
    if (innerAudioOutput) { innerAudioOutput->properties.set("x", 160.0); innerAudioOutput->properties.set("y",  440.0); }
    
    // Restore user-to-user connections only
    for (const auto& saved : savedConnections)
    {
        // Skip connections involving old I/O nodes
        bool sourceIsIO = false;
        bool destIsIO = false;
        
        // Check if source/dest are among the current I/O nodes (they won't match old IDs)
        // We only restore connections between user nodes
        
        if (innerGraph->getNodeForId(saved.sourceNode) && 
            innerGraph->getNodeForId(saved.destNode))
        {
            // Both nodes still exist - restore connection
            if (saved.sourceMidi)
            {
                innerGraph->addConnection({
                    { saved.sourceNode, juce::AudioProcessorGraph::midiChannelIndex },
                    { saved.destNode, juce::AudioProcessorGraph::midiChannelIndex }
                });
            }
            else
            {
                innerGraph->addConnection({
                    { saved.sourceNode, saved.sourceChannel },
                    { saved.destNode, saved.destChannel }
                });
            }
        }
    }
}

// =============================================================================
// Destructor
// =============================================================================
ContainerProcessor::~ContainerProcessor()
{
    if (innerGraph)
    {
        innerGraph->releaseResources();
        innerGraph->clear();
    }
}

// =============================================================================
// Prepare / Process / Release
// =============================================================================
void ContainerProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    innerGraph->setPlayConfigDetails(
        getTotalNumInputChannels(),
        getTotalNumOutputChannels(),
        sampleRate,
        samplesPerBlock);

    // Wire our custom transport playhead into the inner graph
    innerGraph->setPlayHead(&containerTransportPlayHead);

    innerGraph->prepareToPlay(sampleRate, samplesPerBlock);
}

void ContainerProcessor::releaseResources()
{
    innerGraph->releaseResources();
}

void ContainerProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    if (muted.load()) {
        buffer.clear();
        midiMessages.clear();
        return;
    }

    // Update parent playhead reference each block (host may change it)
    parentPlayHead = getPlayHead();

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

    // Process through inner graph — our ContainerTransportPlayHead is already
    // set as the inner graph's playhead via prepareToPlay / setPlayHead
    innerGraph->processBlock(buffer, midiMessages);

    // Apply volume
    float gain = normalizedToGain(volumeNormalized.load());
    if (gain != 1.0f) {
        buffer.applyGain(gain);
    }
}


// =============================================================================
// Bus layout support
// =============================================================================
bool ContainerProcessor::isBusesLayoutSupported(const BusesLayout& /*layouts*/) const
{
    // Container supports any bus configuration — I/O is user-defined
    return true;
}

// =============================================================================
// Bus configuration methods
// =============================================================================
void ContainerProcessor::setInputBusName(int index, const juce::String& name)
{
    if (index >= 0 && index < static_cast<int>(inputBusConfigs.size()))
        inputBusConfigs[static_cast<size_t>(index)].name = name;
}

void ContainerProcessor::setOutputBusName(int index, const juce::String& name)
{
    if (index >= 0 && index < static_cast<int>(outputBusConfigs.size()))
        outputBusConfigs[static_cast<size_t>(index)].name = name;
}

void ContainerProcessor::addInputBus(const juce::String& name, int numChannels)
{
    juce::String busName = name.isEmpty()
        ? ("Input " + juce::String(inputBusConfigs.size() + 1))
        : name;
    inputBusConfigs.push_back({ busName, numChannels });
    rebuildBusLayout();
    updateInnerIONodes();  // FIX: Update I/O nodes immediately
}

void ContainerProcessor::addOutputBus(const juce::String& name, int numChannels)
{
    juce::String busName = name.isEmpty()
        ? ("Output " + juce::String(outputBusConfigs.size() + 1))
        : name;
    outputBusConfigs.push_back({ busName, numChannels });
    rebuildBusLayout();
    updateInnerIONodes();  // FIX: Update I/O nodes immediately
}

void ContainerProcessor::removeInputBus(int index)
{
    if (index >= 0 && index < static_cast<int>(inputBusConfigs.size()))
    {
        inputBusConfigs.erase(inputBusConfigs.begin() + index);
        rebuildBusLayout();
        updateInnerIONodes();  // FIX: Update I/O nodes immediately
    }
}

void ContainerProcessor::removeOutputBus(int index)
{
    if (index >= 0 && index < static_cast<int>(outputBusConfigs.size()))
    {
        outputBusConfigs.erase(outputBusConfigs.begin() + index);
        rebuildBusLayout();
        updateInnerIONodes();  // FIX: Update I/O nodes immediately
    }
}

void ContainerProcessor::rebuildBusLayout()
{
    // =========================================================================
    // CRITICAL: setBusesLayout() CANNOT add/remove buses — it only changes the
    // active channel layout of already-existing buses.
    // We must use addBus() / removeBus() to sync the actual bus topology first.
    // canAddBus() and canRemoveLastBus() return true (see header).
    // =========================================================================

    // --- Sync INPUT buses ---
    int currentIns  = getBusCount(true);
    int desiredIns  = static_cast<int>(inputBusConfigs.size());

    while (currentIns < desiredIns)
    {
        addBus(true);
        ++currentIns;
    }
    while (currentIns > desiredIns && currentIns > 0)
    {
        removeBus(true);
        --currentIns;
    }

    // --- Sync OUTPUT buses ---
    int currentOuts = getBusCount(false);
    int desiredOuts = static_cast<int>(outputBusConfigs.size());

    while (currentOuts < desiredOuts)
    {
        addBus(false);
        ++currentOuts;
    }
    while (currentOuts > desiredOuts && currentOuts > 0)
    {
        removeBus(false);
        --currentOuts;
    }

    // --- Now set the channel layout for each bus ---
    BusesLayout layout;
    for (const auto& bus : inputBusConfigs)
        layout.inputBuses.add(juce::AudioChannelSet::canonicalChannelSet(bus.numChannels));
    for (const auto& bus : outputBusConfigs)
        layout.outputBuses.add(juce::AudioChannelSet::canonicalChannelSet(bus.numChannels));

    setBusesLayout(layout);

    // --- Update inner graph channel count to match ---
    if (innerGraph)
    {
        int totalIns  = getTotalNumInputChannels();
        int totalOuts = getTotalNumOutputChannels();
        double sr = getSampleRate() > 0.0 ? getSampleRate() : 44100.0;
        int bs    = getBlockSize()   > 0   ? getBlockSize()   : 512;
        innerGraph->setPlayConfigDetails(totalIns, totalOuts, sr, bs);
    }
}

// =============================================================================
// Dynamic I/O configuration
// =============================================================================
bool ContainerProcessor::setAudioIOConfiguration(int numInputChannels, int numOutputChannels)
{
    inputBusConfigs.clear();
    outputBusConfigs.clear();
    
    if (numInputChannels > 0)
    {
        // Create stereo pairs, or mono for odd remainder
        int stereoIns = numInputChannels / 2;
        int monoIns = numInputChannels % 2;
        
        for (int i = 0; i < stereoIns; ++i)
            inputBusConfigs.push_back({ "Input " + juce::String(i + 1), 2 });
        if (monoIns > 0)
            inputBusConfigs.push_back({ "Input " + juce::String(stereoIns + 1), 1 });
    }
    
    if (numOutputChannels > 0)
    {
        int stereoOuts = numOutputChannels / 2;
        int monoOuts = numOutputChannels % 2;
        
        for (int i = 0; i < stereoOuts; ++i)
            outputBusConfigs.push_back({ "Output " + juce::String(i + 1), 2 });
        if (monoOuts > 0)
            outputBusConfigs.push_back({ "Output " + juce::String(stereoOuts + 1), 1 });
    }
    
    rebuildBusLayout();
    updateInnerIONodes();  // FIX: Update I/O nodes immediately
    
    return true;
}

juce::AudioProcessor::BusesProperties ContainerProcessor::makeBusesProperties(
    const std::vector<BusConfig>& inputs,
    const std::vector<BusConfig>& outputs)
{
    BusesProperties props;
    for (const auto& bus : inputs) {
        props = props.withInput(bus.name, 
            juce::AudioChannelSet::canonicalChannelSet(bus.numChannels), true);
    }
    for (const auto& bus : outputs) {
        props = props.withOutput(bus.name, 
            juce::AudioChannelSet::canonicalChannelSet(bus.numChannels), true);
    }
    return props;
}

// =============================================================================
// Volume helpers (same curve as SimpleConnectorProcessor)
// =============================================================================
float ContainerProcessor::getVolumeDb() const
{
    float normalized = volumeNormalized.load();
    
    if (normalized <= 0.0f)
        return -100.0f;
    
    if (normalized <= 0.5f) {
        float t = normalized / 0.5f;
        return juce::jmap(t, 0.0f, 1.0f, -60.0f, 0.0f);
    } else {
        float t = (normalized - 0.5f) / 0.5f;
        return juce::jmap(t, 0.0f, 1.0f, 0.0f, 25.0f);
    }
}

float ContainerProcessor::normalizedToGain(float normalized) const
{
    if (normalized <= 0.0f)
        return 0.0f;
    
    float db = getVolumeDb();
    return juce::Decibels::decibelsToGain(db);
}

// =============================================================================
// State serialization (for workspace save/load via main processor)
// =============================================================================
void ContainerProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::String xmlStr = serializeToXml();
    juce::MemoryOutputStream stream(destData, false);
    stream.writeString(xmlStr);
}

void ContainerProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    juce::MemoryInputStream stream(data, static_cast<size_t>(sizeInBytes), false);
    juce::String xmlStr = stream.readString();
    restoreFromXml(xmlStr);
}

// =============================================================================
// Full XML serialization — used by both state and preset files
// =============================================================================
juce::String ContainerProcessor::serializeToXml() const
{
    juce::XmlElement root("OnStageContainerPreset");
    
    root.setAttribute("name", containerName);
    root.setAttribute("version", 1);
    root.setAttribute("volume", (double)volumeNormalized.load());
    root.setAttribute("muted", muted.load());
    root.setAttribute("transportSynced", transportSyncedToMaster.load());
    root.setAttribute("customTempo", customTempo.load());
    root.setAttribute("customTimeSigNum", customTimeSigNum.load());
    root.setAttribute("customTimeSigDen", customTimeSigDen.load());
    
    // Serialize bus configuration
    auto* busesXml = root.createNewChildElement("Buses");
    
    auto* inputsXml = busesXml->createNewChildElement("Inputs");
    for (const auto& bus : inputBusConfigs) {
        auto* busXml = inputsXml->createNewChildElement("Bus");
        busXml->setAttribute("name", bus.name);
        busXml->setAttribute("channels", bus.numChannels);
    }
    
    auto* outputsXml = busesXml->createNewChildElement("Outputs");
    for (const auto& bus : outputBusConfigs) {
        auto* busXml = outputsXml->createNewChildElement("Bus");
        busXml->setAttribute("name", bus.name);
        busXml->setAttribute("channels", bus.numChannels);
    }
    
    // Serialize inner graph nodes
    auto* nodesXml = root.createNewChildElement("Nodes");
    for (auto* node : innerGraph->getNodes())
    {
        auto* nodeXml = nodesXml->createNewChildElement("Node");
        auto* proc = node->getProcessor();
        bool isIO = (dynamic_cast<juce::AudioProcessorGraph::AudioGraphIOProcessor*>(proc) != nullptr);
        
        nodeXml->setAttribute("id", (int)node->nodeID.uid);
        nodeXml->setAttribute("x", (double)node->properties["x"]);
        nodeXml->setAttribute("y", (double)node->properties["y"]);
        nodeXml->setAttribute("bypassed", node->isBypassed());
        
        if (isIO) {
            if (node == innerAudioInput.get())       nodeXml->setAttribute("type", "AudioInput");
            else if (node == innerAudioOutput.get()) nodeXml->setAttribute("type", "AudioOutput");
            else nodeXml->setAttribute("type", "IO");
        } else {
            // Check system tools
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
            } else if (auto* mp = dynamic_cast<MeteringProcessor*>(proc)) {
                nodeXml->setAttribute("type", "Plugin");
                
                auto desc = mp->getCachedDescription();
                nodeXml->setAttribute("name", desc.name);
                nodeXml->setAttribute("identifier", desc.identifier);
                nodeXml->setAttribute("format", desc.format);
                nodeXml->setAttribute("uid", desc.uniqueId);
                nodeXml->setAttribute("midiChannelMask", mp->getMidiChannelMask());
                nodeXml->setAttribute("passThrough", mp->isPassThrough());
                
                if (mp->getInnerPlugin()) {
                    juce::MemoryBlock mb;
                    mp->getInnerPlugin()->getStateInformation(mb);
                    auto* state = nodeXml->createNewChildElement("State");
                    state->addTextElement(mb.toBase64Encoding());
                }
            }
        }
    }
    
    // Serialize inner graph connections
    auto* connsXml = root.createNewChildElement("Connections");
    for (auto& conn : innerGraph->getConnections())
    {
        auto* cXml = connsXml->createNewChildElement("Connection");
        cXml->setAttribute("srcNode", (int)conn.source.nodeID.uid);
        cXml->setAttribute("srcCh", conn.source.channelIndex);
        cXml->setAttribute("srcMidi", conn.source.isMIDI());
        cXml->setAttribute("dstNode", (int)conn.destination.nodeID.uid);
        cXml->setAttribute("dstCh", conn.destination.channelIndex);
        cXml->setAttribute("dstMidi", conn.destination.isMIDI());
    }
    
    return root.toString();
}

// =============================================================================
// Restore from XML
// =============================================================================
bool ContainerProcessor::restoreFromXml(const juce::String& xmlStr)
{
    auto xml = juce::parseXML(xmlStr);
    if (!xml || !xml->hasTagName("OnStageContainerPreset"))
        return false;
    
    containerName = xml->getStringAttribute("name", "Container");
    volumeNormalized.store((float)xml->getDoubleAttribute("volume", 0.5));
    muted.store(xml->getBoolAttribute("muted", false));
    transportSyncedToMaster.store(xml->getBoolAttribute("transportSynced", true));
    customTempo.store(xml->getDoubleAttribute("customTempo", 120.0));
    customTimeSigNum.store(xml->getIntAttribute("customTimeSigNum", 4));
    customTimeSigDen.store(xml->getIntAttribute("customTimeSigDen", 4));
    
    // Restore bus configuration
    inputBusConfigs.clear();
    outputBusConfigs.clear();
    
    if (auto* busesXml = xml->getChildByName("Buses")) {
        if (auto* inputsXml = busesXml->getChildByName("Inputs")) {
            for (auto* busXml : inputsXml->getChildIterator()) {
                if (busXml->hasTagName("Bus")) {
                    BusConfig cfg;
                    cfg.name = busXml->getStringAttribute("name", "Input");
                    cfg.numChannels = busXml->getIntAttribute("channels", 2);
                    inputBusConfigs.push_back(cfg);
                }
            }
        }
        if (auto* outputsXml = busesXml->getChildByName("Outputs")) {
            for (auto* busXml : outputsXml->getChildIterator()) {
                if (busXml->hasTagName("Bus")) {
                    BusConfig cfg;
                    cfg.name = busXml->getStringAttribute("name", "Output");
                    cfg.numChannels = busXml->getIntAttribute("channels", 2);
                    outputBusConfigs.push_back(cfg);
                }
            }
        }
    }
    
    // Rebuild inner graph — create I/O nodes based on restored bus config
    innerGraph->clear();
    
    // Create audio I/O nodes only if audio buses are configured
    innerAudioInput = nullptr;
    innerAudioOutput = nullptr;
    
    if (!inputBusConfigs.empty())
    {
        innerAudioInput = innerGraph->addNode(
            std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor>(
                juce::AudioProcessorGraph::AudioGraphIOProcessor::audioInputNode));
    }
    if (!outputBusConfigs.empty())
    {
        innerAudioOutput = innerGraph->addNode(
            std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor>(
                juce::AudioProcessorGraph::AudioGraphIOProcessor::audioOutputNode));
    }
    
    // OnStage: No MIDI I/O nodes in containers (effects-only mode)
    innerMidiInput = nullptr;
    innerMidiOutput = nullptr;
    
    // Rebuild bus layout to match restored config
    rebuildBusLayout();
    
    std::map<int, juce::AudioProcessorGraph::NodeID> nodeIdMap;
    
    // Map I/O nodes
    if (auto* nodesXml = xml->getChildByName("Nodes")) {
        for (auto* nodeXml : nodesXml->getChildIterator()) {
            if (nodeXml->hasTagName("Node")) {
                juce::String type = nodeXml->getStringAttribute("type");
                int oldId = nodeXml->getIntAttribute("id");
                double x = nodeXml->getDoubleAttribute("x");
                double y = nodeXml->getDoubleAttribute("y");
                
                if (type == "AudioInput" && innerAudioInput) {
                    nodeIdMap[oldId] = innerAudioInput->nodeID;
                    innerAudioInput->properties.set("x", x);
                    innerAudioInput->properties.set("y", y);
                } else if (type == "AudioOutput" && innerAudioOutput) {
                    nodeIdMap[oldId] = innerAudioOutput->nodeID;
                    innerAudioOutput->properties.set("x", x);
                    innerAudioOutput->properties.set("y", y);
                }
                // OnStage: MidiInput/MidiOutput nodes are ignored (effects-only)
            }
        }
    }
    
    // Restore non-IO nodes
    if (auto* nodesXml = xml->getChildByName("Nodes")) {
        for (auto* nodeXml : nodesXml->getChildIterator()) {
            if (!nodeXml->hasTagName("Node")) continue;
            
            juce::String type = nodeXml->getStringAttribute("type");
            int oldId = nodeXml->getIntAttribute("id");
            double x = nodeXml->getDoubleAttribute("x");
            double y = nodeXml->getDoubleAttribute("y");
            
            // === System Tools ===
            if (type == "SystemTool") {
                juce::String toolName = nodeXml->getStringAttribute("toolName");
                std::unique_ptr<juce::AudioProcessor> toolProc;
                
                if (toolName == "SimpleConnector")        toolProc = std::make_unique<SimpleConnectorProcessor>();
                else if (toolName == "StereoMeter")       toolProc = std::make_unique<StereoMeterProcessor>();
                else if (toolName == "Recorder")          toolProc = std::make_unique<RecorderProcessor>();
                else if (toolName == "TransientSplitter") toolProc = std::make_unique<TransientSplitterProcessor>();
                else if (toolName == "Container") {
                    auto container = std::make_unique<ContainerProcessor>();
                    container->setParentProcessor(parentProcessor);
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
                    
                    auto nodePtr = innerGraph->addNode(std::move(toolProc));
                    if (nodePtr) {
                        nodeIdMap[oldId] = nodePtr->nodeID;
                        nodePtr->properties.set("x", x);
                        nodePtr->properties.set("y", y);
                    }
                }
                continue;
            }
            
            if (type != "Plugin") continue;
            
            // === Plugins ===
            if (!parentProcessor) continue;
            
            juce::PluginDescription desc;
            desc.fileOrIdentifier = nodeXml->getStringAttribute("identifier");
            desc.uniqueId = nodeXml->getIntAttribute("uid");
            desc.name = nodeXml->getStringAttribute("name");
            desc.pluginFormatName = nodeXml->getStringAttribute("format");
            
            juce::String msg;
            std::unique_ptr<juce::AudioPluginInstance> instance;
            try {
                instance = parentProcessor->formatManager.createPluginInstance(
                    desc, getSampleRate() > 0 ? getSampleRate() : 44100.0,
                    getBlockSize() > 0 ? getBlockSize() : 512, msg);
            } catch (...) { continue; }
            
            if (instance) {
                auto nodePtr = innerGraph->addNode(std::make_unique<MeteringProcessor>(std::move(instance)));
                auto* node = nodePtr.get();
                if (node) {
                    nodeIdMap[oldId] = node->nodeID;
                    node->properties.set("x", x);
                    node->properties.set("y", y);
                    
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
    
    // Restore connections
    if (auto* connsXml = xml->getChildByName("Connections")) {
        for (auto* cXml : connsXml->getChildIterator()) {
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
                    innerGraph->addConnection(conn);
                }
            }
        }
    }
    
    return true;
}

// =============================================================================
// Container preset folder
// =============================================================================
juce::File ContainerProcessor::getGlobalDefaultFolder()
{
    return globalDefaultFolder;
}

void ContainerProcessor::setGlobalDefaultFolder(const juce::File& folder)
{
    globalDefaultFolder = folder;
}

juce::File ContainerProcessor::getEffectiveDefaultFolder()
{
    if (globalDefaultFolder.exists())
        return globalDefaultFolder;
    
    // Default: Documents/Colosseum/containers
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("Colosseum")
        .getChildFile("containers");
}

// =============================================================================
// Preset file save/load
// =============================================================================
bool ContainerProcessor::savePreset(const juce::File& file) const
{
    // Ensure parent directory exists
    file.getParentDirectory().createDirectory();
    
    juce::String xmlStr = serializeToXml();
    return file.replaceWithText(xmlStr);
}

bool ContainerProcessor::loadPreset(const juce::File& file)
{
    if (!file.existsAsFile())
        return false;
    
    juce::String xmlStr = file.loadFileAsString();
    return restoreFromXml(xmlStr);
}

