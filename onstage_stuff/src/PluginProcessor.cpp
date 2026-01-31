#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "RegistrationManager.h"

juce::AudioDeviceManager* SubterraneumAudioProcessor::standaloneDeviceManager = nullptr;

// =============================================================================
// Plugin Format Initialization
// =============================================================================
void SubterraneumAudioProcessor::initializePluginFormats()
{
    // -------------------------------------------------------------------------
    // VST3 - All Platforms (Always Available)
    // -------------------------------------------------------------------------
    #if JUCE_PLUGINHOST_VST3
    formatManager.addFormat(new juce::VST3PluginFormat());
    DBG("Plugin Format: VST3 enabled");
    #endif
    
    // -------------------------------------------------------------------------
    // AudioUnit - macOS Only
    // -------------------------------------------------------------------------
    #if JUCE_PLUGINHOST_AU && JUCE_MAC
    formatManager.addFormat(new juce::AudioUnitPluginFormat());
    DBG("Plugin Format: AudioUnit enabled (macOS)");
    #endif
    
    // -------------------------------------------------------------------------
    // LADSPA - Linux Only
    // -------------------------------------------------------------------------
    #if JUCE_PLUGINHOST_LADSPA && JUCE_LINUX
    formatManager.addFormat(new juce::LADSPAPluginFormat());
    DBG("Plugin Format: LADSPA enabled (Linux)");
    #endif
    
    // -------------------------------------------------------------------------
    // CLAP - All Platforms (Optional - Requires clap-wrapper)
    // -------------------------------------------------------------------------
    #if SUBTERRANEUM_CLAP_HOSTING
    // When clap-wrapper is integrated, add CLAP format here:
    // formatManager.addFormat(new CLAPPluginFormat());
    DBG("Plugin Format: CLAP hosting enabled");
    #endif
    
    // Log summary
    DBG("Total plugin formats registered: " << formatManager.getNumFormats());
    for (int i = 0; i < formatManager.getNumFormats(); ++i) {
        DBG("  - " << formatManager.getFormat(i)->getName());
    }
}

juce::StringArray SubterraneumAudioProcessor::getSupportedFormatNames() const
{
    juce::StringArray names;
    for (int i = 0; i < formatManager.getNumFormats(); ++i) {
        names.add(formatManager.getFormat(i)->getName());
    }
    return names;
}

// =============================================================================
// Constructor / Destructor
// =============================================================================
SubterraneumAudioProcessor::SubterraneumAudioProcessor()
     : AudioProcessor(juce::AudioProcessor::BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    // 1. Initialize Plugin Formats (Platform-Specific)
    initializePluginFormats();
    
    // 2. Setup Properties / Settings
    options.applicationName = "Subterraneum";
    options.filenameSuffix = ".settings";
    options.osxLibrarySubFolder = "Application Support";
    options.folderName = "Subterraneum";
    options.commonToAllUsers = false;
    options.ignoreCaseOfKeyNames = false;
    options.storageFormat = juce::PropertiesFile::storeAsXML;
    appProperties.setStorageParameters(options);
    
    // 3. Load Known Plugins immediately
    auto* userSettings = appProperties.getUserSettings();
    if (auto xml = userSettings->getXmlValue("KnownPlugins"))
        knownPluginList.recreateFromXml(*xml);

    // 4. Load saved audio settings
    loadAudioSettings();

    // 5. Setup Graph
    mainGraph = std::make_unique<juce::AudioProcessorGraph>();
    
    // 6. Listeners
    knownPluginList.addChangeListener(this);
    
    if (standaloneDeviceManager)
        standaloneDeviceManager->addChangeListener(this);

    // 7. Initialize mixer gains to unity (1.0)
    for (int i = 0; i < maxMixerChannels; ++i) {
        inputGains[i].store(1.0f);
        outputGains[i].store(1.0f);
    }

    // 8. Initial Graph Build
    updateGraph();
}

SubterraneumAudioProcessor::~SubterraneumAudioProcessor() {
    knownPluginList.removeChangeListener(this);
    if (standaloneDeviceManager)
        standaloneDeviceManager->removeChangeListener(this);

    // Clear I/O node pointers first
    audioInputNode = nullptr;
    audioOutputNode = nullptr;
    midiInputNode = nullptr;
    midiOutputNode = nullptr;
    
    // Clear the graph (this releases all processors)
    if (mainGraph) {
        mainGraph->clear();
        mainGraph->releaseResources();
    }

    // Final save on exit
    auto* userSettings = appProperties.getUserSettings();
    if (auto xml = knownPluginList.createXml())
        userSettings->setValue("KnownPlugins", xml.get());
    userSettings->saveIfNeeded();
}

// =============================================================================
// Audio Settings Persistence
// =============================================================================
void SubterraneumAudioProcessor::saveAudioSettings() {
    auto* userSettings = appProperties.getUserSettings();
    if (!userSettings) return;
    
    juce::String deviceName = "";
    if (standaloneDeviceManager) {
        if (auto* device = standaloneDeviceManager->getCurrentAudioDevice()) {
            deviceName = device->getName();
        }
    }
    
    userSettings->setValue("AudioDeviceName", deviceName);
    userSettings->saveIfNeeded();
    
    savedAudioDeviceName = deviceName;
    DBG("Saved audio device: " << (deviceName.isEmpty() ? "OFF" : deviceName));
}

void SubterraneumAudioProcessor::loadAudioSettings() {
    auto* userSettings = appProperties.getUserSettings();
    if (!userSettings) {
        savedAudioDeviceName = "";
        return;
    }
    
    // Check if audio settings exist
    if (!userSettings->containsKey("AudioDeviceName")) {
        // First run - create default settings with audio disabled
        userSettings->setValue("AudioDeviceName", "");
        userSettings->saveIfNeeded();
        savedAudioDeviceName = "";
        DBG("Created default audio settings with audio device disabled");
    } else {
        savedAudioDeviceName = userSettings->getValue("AudioDeviceName", "");
        DBG("Loaded saved audio device: " << (savedAudioDeviceName.isEmpty() ? "OFF" : savedAudioDeviceName));
    }
}

// =============================================================================
// Change Listener
// =============================================================================
void SubterraneumAudioProcessor::changeListenerCallback(juce::ChangeBroadcaster* source) {
    // If the plugin list changes (scan success), save immediately!
    if (source == &knownPluginList) {
        if (auto* userSettings = appProperties.getUserSettings()) {
            if (auto xml = knownPluginList.createXml()) {
                userSettings->setValue("KnownPlugins", xml.get());
                userSettings->saveIfNeeded();
            }
        }
    }
    // If Audio Device changes (sample rate, block size, channel count)
    else if (source == standaloneDeviceManager) {
        juce::MessageManager::callAsync([this]() {
            updateIOChannelCount();
        });
    }
}

// =============================================================================
// I/O Channel Management
// =============================================================================
void SubterraneumAudioProcessor::updateIOChannelCount() {
    if (!mainGraph) return;
    
    int numIns = 2;
    int numOuts = 2;

    // Get ALL Available Channels from Device (including virtual/loopback)
    if (standaloneDeviceManager) {
        if (auto* device = standaloneDeviceManager->getCurrentAudioDevice()) {
            inputChannelNames = device->getInputChannelNames();
            outputChannelNames = device->getOutputChannelNames();
            numIns = inputChannelNames.size();
            numOuts = outputChannelNames.size();
            
            DBG("Device: " << device->getName());
            DBG("Total Input Channels: " << numIns);
            DBG("Total Output Channels: " << numOuts);
        }
    }
    
    // Safety minimum
    if (numIns < 2) numIns = 2;
    if (numOuts < 2) numOuts = 2;

    // Update graph configuration
    mainGraph->setPlayConfigDetails(numIns, numOuts, getSampleRate(), getBlockSize());
    
    // Store positions of existing I/O nodes
    float audioInX = 20, audioInY = 80;
    float audioOutX = 20, audioOutY = 450;
    float midiInX = 600, midiInY = 80;
    float midiOutX = 600, midiOutY = 450;
    
    if (audioInputNode) {
        audioInX = (float)audioInputNode->properties.getWithDefault("x", 50.0);
        audioInY = (float)audioInputNode->properties.getWithDefault("y", 50.0);
    }
    if (audioOutputNode) {
        audioOutX = (float)audioOutputNode->properties.getWithDefault("x", 800.0);
        audioOutY = (float)audioOutputNode->properties.getWithDefault("y", 300.0);
    }
    if (midiInputNode) {
        midiInX = (float)midiInputNode->properties.getWithDefault("x", 50.0);
        midiInY = (float)midiInputNode->properties.getWithDefault("y", 150.0);
    }
    if (midiOutputNode) {
        midiOutX = (float)midiOutputNode->properties.getWithDefault("x", 800.0);
        midiOutY = (float)midiOutputNode->properties.getWithDefault("y", 150.0);
    }
    
    // Remove old I/O nodes
    if (audioInputNode) mainGraph->removeNode(audioInputNode->nodeID);
    if (audioOutputNode) mainGraph->removeNode(audioOutputNode->nodeID);
    if (midiInputNode) mainGraph->removeNode(midiInputNode->nodeID);
    if (midiOutputNode) mainGraph->removeNode(midiOutputNode->nodeID);
    
    // Create new IO Nodes with correct channel count
    audioInputNode = mainGraph->addNode(std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor>(juce::AudioProcessorGraph::AudioGraphIOProcessor::audioInputNode));
    audioOutputNode = mainGraph->addNode(std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor>(juce::AudioProcessorGraph::AudioGraphIOProcessor::audioOutputNode));
    midiInputNode = mainGraph->addNode(std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor>(juce::AudioProcessorGraph::AudioGraphIOProcessor::midiInputNode));
    midiOutputNode = mainGraph->addNode(std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor>(juce::AudioProcessorGraph::AudioGraphIOProcessor::midiOutputNode));
    
    // Restore positions
    if (audioInputNode) { audioInputNode->properties.set("x", audioInX); audioInputNode->properties.set("y", audioInY); }
    if (audioOutputNode) { audioOutputNode->properties.set("x", audioOutX); audioOutputNode->properties.set("y", audioOutY); }
    if (midiInputNode) { midiInputNode->properties.set("x", midiInX); midiInputNode->properties.set("y", midiInY); }
    if (midiOutputNode) { midiOutputNode->properties.set("x", midiOutX); midiOutputNode->properties.set("y", midiOutY); }
    
    // Re-prepare the graph with new config
    if (standaloneDeviceManager) {
        if (auto* device = standaloneDeviceManager->getCurrentAudioDevice()) {
            mainGraph->prepareToPlay(device->getCurrentSampleRate(), device->getCurrentBufferSizeSamples());
        }
    }
    
    DBG("I/O nodes recreated with " << numIns << " inputs and " << numOuts << " outputs");
}

void SubterraneumAudioProcessor::updateActiveChannels() {
    // No-op: All channels stay enabled at device level.
}

// =============================================================================
// Graph Management
// =============================================================================
bool SubterraneumAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
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
    updateGraph();
}

// =============================================================================
// Audio Processing
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
}

void SubterraneumAudioProcessor::releaseResources() {
    if (mainGraph)
        mainGraph->releaseResources();
}

void SubterraneumAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    if (!mainGraph) {
        buffer.clear();
        return;
    }
    
    // Update demo mode timing
    RegistrationManager::getInstance().updateDemoMode();
    bool demoSilence = RegistrationManager::getInstance().isDemoSilenceActive();
    
    // If audio input is disabled OR demo silence is active, clear the input buffer before processing
    if (!audioInputEnabled.load() || demoSilence) {
        buffer.clear();
        mainInputRms[0] = 0.0f;
        mainInputRms[1] = 0.0f;
    } else {
        // Apply input gains (before graph processing)
        for (int ch = 0; ch < buffer.getNumChannels() && ch < maxMixerChannels; ++ch) {
            float gain = inputGains[ch].load();
            if (gain != 1.0f) {
                buffer.applyGain(ch, 0, buffer.getNumSamples(), gain);
            }
        }
        
        if (buffer.getNumChannels() > 0) mainInputRms[0] = buffer.getRMSLevel(0, 0, buffer.getNumSamples());
        if (buffer.getNumChannels() > 1) mainInputRms[1] = buffer.getRMSLevel(1, 0, buffer.getNumSamples());
    }
    
    keyboardState.processNextMidiBuffer(midiMessages, 0, buffer.getNumSamples(), true);
    
    if (!midiMessages.isEmpty()) {
        mainMidiInNoteCount++;
        mainMidiInFlash = true;
    } else {
        mainMidiInFlash = false;
    }

    // Process the graph with crash protection
    try {
        mainGraph->processBlock(buffer, midiMessages);
    } catch (...) {
        // Graph processing failed - clear buffer to prevent garbage audio
        buffer.clear();
    }
    
    // If audio output is disabled OR demo silence is active, clear the output buffer after processing
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

        if (buffer.getNumChannels() > 0) mainOutputRms[0] = buffer.getRMSLevel(0, 0, buffer.getNumSamples());
        if (buffer.getNumChannels() > 1) mainOutputRms[1] = buffer.getRMSLevel(1, 0, buffer.getNumSamples());
    }
    
    // Recording - capture final output using ThreadedWriter
    if (isRecording.load() && backgroundWriter != nullptr) {
        backgroundWriter->write(buffer.getArrayOfReadPointers(), buffer.getNumSamples());
    }
}

// =============================================================================
// Recording Methods (using ThreadedWriter for reliable file writing)
// =============================================================================
bool SubterraneumAudioProcessor::startRecording() {
    stopRecording();
    
    // Start the writer thread if not already running
    if (!writerThread.isThreadRunning())
        writerThread.startThread();
    
    // Get actual sample rate from device
    double sampleRate = getSampleRate();
    if (sampleRate <= 0) sampleRate = 44100.0;
    
    // Create unique filename in user's music/documents folder
    lastRecordingFile = juce::File::getSpecialLocation(juce::File::userMusicDirectory)
                            .getNonexistentChildFile("Colosseum_Recording", ".wav");
    
    auto* wavFormat = new juce::WavAudioFormat();
    auto* outputStream = new juce::FileOutputStream(lastRecordingFile);
    
    if (outputStream->failedToOpen()) {
        delete outputStream;
        delete wavFormat;
        return false;
    }
    
    // Create 24-bit stereo WAV writer
    auto* writer = wavFormat->createWriterFor(outputStream, sampleRate, 2, 24, {}, 0);
    
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
    backgroundWriter.reset();  // This flushes and closes the file
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
const juce::String SubterraneumAudioProcessor::getName() const { return "Colosseum"; }
bool SubterraneumAudioProcessor::acceptsMidi() const { return true; }
bool SubterraneumAudioProcessor::producesMidi() const { return true; }
bool SubterraneumAudioProcessor::isMidiEffect() const { return false; }
double SubterraneumAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int SubterraneumAudioProcessor::getNumPrograms() { return 1; }
int SubterraneumAudioProcessor::getCurrentProgram() { return 0; }
void SubterraneumAudioProcessor::setCurrentProgram(int index) {}
const juce::String SubterraneumAudioProcessor::getProgramName(int index) { return {}; }
void SubterraneumAudioProcessor::changeProgramName(int index, const juce::String& newName) {}

// =============================================================================
// State
// =============================================================================
void SubterraneumAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    juce::XmlElement xml("SubterraneumState");
    copyXmlToBinary(xml, destData);
}

void SubterraneumAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
}

// =============================================================================
// Preset Save/Load
// =============================================================================
void SubterraneumAudioProcessor::loadUserPreset(const juce::File& file) {
    auto xml = juce::parseXML(file);
    if (!xml || !xml->hasTagName("SubterraneumPatch")) return;

    suspendProcessing(true);
    
    audioInputNode = nullptr;
    audioOutputNode = nullptr;
    midiInputNode = nullptr;
    midiOutputNode = nullptr;
    
    mainGraph->clear();
    updateGraph(); 
    
    instrumentSelectorMultiMode = xml->getBoolAttribute("multiMode", false);
    audioInputEnabled.store(xml->getBoolAttribute("audioInputEnabled", true));
    audioOutputEnabled.store(xml->getBoolAttribute("audioOutputEnabled", true));
    
    std::map<int, juce::AudioProcessorGraph::NodeID> nodeIdMap;

    // First pass - map IO node IDs and load their positions
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
                } else if (type == "MidiInput" && midiInputNode) {
                    nodeIdMap[oldId] = midiInputNode->nodeID;
                    midiInputNode->properties.set("x", x);
                    midiInputNode->properties.set("y", y);
                } else if (type == "MidiOutput" && midiOutputNode) {
                    nodeIdMap[oldId] = midiOutputNode->nodeID;
                    midiOutputNode->properties.set("x", x);
                    midiOutputNode->properties.set("y", y);
                }
            }
        }
    }

    // Second pass - load plugin nodes
    if (auto* nodes = xml->getChildByName("Nodes")) {
        for (auto* nodeXml : nodes->getChildIterator()) {
            if (nodeXml->hasTagName("Node")) {
                juce::String type = nodeXml->getStringAttribute("type");
                int oldId = nodeXml->getIntAttribute("id");
                
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
                } catch (...) {
                    DBG("Failed to load plugin: " + desc.name + " (crash during instantiation)");
                    continue;
                }
                
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
                            mp->setMidiChannelMask(nodeXml->getIntAttribute("midiChannelMask", 0xFFFF));
                            mp->setPassThrough(nodeXml->getBoolAttribute("passThrough", false));
                            
                            // CPU Optimization: Freeze bypassed plugins on load
                            mp->setFrozen(isBypassed);
                            
                            if (auto* stateXml = nodeXml->getChildByName("State")) {
                                juce::MemoryBlock mb;
                                mb.fromBase64Encoding(stateXml->getAllSubText());
                                try {
                                    mp->getInnerPlugin()->setStateInformation(mb.getData(), (int)mb.getSize());
                                } catch (...) {
                                    DBG("Failed to restore state for: " + desc.name);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Load connections
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
    
    suspendProcessing(false);
}

void SubterraneumAudioProcessor::saveUserPreset(const juce::File& file) {
    juce::XmlElement root("SubterraneumPatch");
    
    root.setAttribute("multiMode", instrumentSelectorMultiMode);
    root.setAttribute("audioInputEnabled", audioInputEnabled.load());
    root.setAttribute("audioOutputEnabled", audioOutputEnabled.load());
    
    // Save nodes
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
            else if (node == midiInputNode.get()) nodeXml->setAttribute("type", "MidiInput");
            else if (node == midiOutputNode.get()) nodeXml->setAttribute("type", "MidiOutput");
            else nodeXml->setAttribute("type", "IO");
        } else {
            nodeXml->setAttribute("type", "Plugin");
            
            if (auto* mp = dynamic_cast<MeteringProcessor*>(proc)) {
                auto desc = mp->getInnerPlugin()->getPluginDescription();
                nodeXml->setAttribute("name", desc.name);
                nodeXml->setAttribute("identifier", desc.fileOrIdentifier);
                nodeXml->setAttribute("format", desc.pluginFormatName);
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

    // Save connections
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
// Node Management
// =============================================================================
void SubterraneumAudioProcessor::removeNode(juce::AudioProcessorGraph::NodeID nodeID) {
    suspendProcessing(true);
    mainGraph->removeNode(nodeID);
    suspendProcessing(false);
}

void SubterraneumAudioProcessor::toggleBypass(juce::AudioProcessorGraph::NodeID nodeID) {
    if (auto* node = mainGraph->getNodeForId(nodeID)) {
        bool newBypassState = !node->isBypassed();
        node->setBypassed(newBypassState);
        
        // CPU Optimization: Freeze the plugin when bypassed
        if (auto* meteringProc = dynamic_cast<MeteringProcessor*>(node->getProcessor())) {
            meteringProc->setFrozen(newBypassState);
        }
    }
}

void SubterraneumAudioProcessor::storeMultiStates() {
    multiModeBypassStates.clear();
    for(auto* node : mainGraph->getNodes()) {
        multiModeBypassStates[node->nodeID] = node->isBypassed();
    }
}

void SubterraneumAudioProcessor::restoreMultiStates() {
    for(auto const& [id, bypassed] : multiModeBypassStates) {
        if(auto* node = mainGraph->getNodeForId(id)) {
            node->setBypassed(bypassed);
            
            // CPU Optimization: Freeze/unfreeze based on bypass state
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

// =============================================================================
// Plugin Filter Entry Point
// =============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new SubterraneumAudioProcessor();
}