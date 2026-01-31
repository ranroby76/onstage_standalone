// #D:\Workspace\Subterraneum_plugins_daw\src\PluginProcessor.h
// CRITICAL FIX: NEVER call getPluginDescription() after construction!
// Some plugins (like SOLO by Taqs.im) freeze when getPluginDescription() is called.
// Solution: Query ONCE at construction using static helper, cache forever.
// NEW: Added VST2 support with format identification helpers

#pragma once

#include <JuceHeader.h>
#include "RegistrationManager.h"

class SubterraneumAudioProcessorEditor;

#if SUBTERRANEUM_CLAP_HOSTING
#endif

// ==============================================================================
// Metering Processor - COMPLETE FREEZE FIX: Cache ALL plugin info in constructor
// ==============================================================================
class MeteringProcessor : public juce::AudioProcessor {
public:
    struct BusInfo {
        int busIndex;
        juce::String name;
        int numChannels;
        bool isEnabled;
        bool isSidechain;
    };
    
    // Cached plugin description for preset saving
    struct CachedPluginDescription {
        juce::String name;
        juce::String identifier;
        juce::String format;
        int uniqueId = 0;
    };
    
    // =========================================================================
    // CRITICAL FIX: Static helper queries isInstrument ONCE before construction
    // This is called in the initializer list, before any member initialization
    // =========================================================================
    static bool queryIsInstrument(juce::AudioPluginInstance* plugin) {
        if (!plugin) return false;
        return plugin->getPluginDescription().isInstrument;
    }
    
    static juce::AudioProcessor::BusesProperties createBusesProperties(bool isInstrument) {
        if (isInstrument) {
            return BusesProperties()
                .withOutput("Output", juce::AudioChannelSet::stereo(), true);
        } else {
            return BusesProperties()
                .withInput("Input", juce::AudioChannelSet::stereo(), true)
                .withInput("Sidechain", juce::AudioChannelSet::stereo(), false)
                .withOutput("Output", juce::AudioChannelSet::stereo(), true);
        }
    }
    
    MeteringProcessor(std::unique_ptr<juce::AudioPluginInstance> pluginToWrap)
        : AudioProcessor(createBusesProperties(queryIsInstrument(pluginToWrap.get()))),
          cachedIsInstrument(queryIsInstrument(pluginToWrap.get())),
          innerPlugin(std::move(pluginToWrap)) 
    {
        // =========================================================================
        // FREEZE FIX: Cache ALL plugin information ONCE during construction
        // NEVER call these methods again after construction - use cached values!
        // =========================================================================
        if (innerPlugin) {
            try {
                // Cache plugin description (for preset save)
                auto desc = innerPlugin->getPluginDescription();
                cachedDesc.name = desc.name;
                cachedDesc.identifier = desc.fileOrIdentifier;
                cachedDesc.format = desc.pluginFormatName;
                cachedDesc.uniqueId = desc.uniqueId;
                
                // Cache plugin name (for UI display)
                cachedPluginName = innerPlugin->getName();
                if (cachedPluginName.length() > 20) {
                    cachedPluginName = cachedPluginName.substring(0, 18) + "..";
                }
                
                // Cache editor capability
                cachedHasEditor = innerPlugin->hasEditor();
            }
            catch (...) {
                // If any caching fails, use safe defaults
                cachedDesc.name = "Unknown";
                cachedDesc.identifier = "unknown";
                cachedDesc.format = "Unknown";
                cachedDesc.uniqueId = 0;
                cachedPluginName = "Plugin";
                cachedHasEditor = false;
            }
            
            detectSidechainCapability();
            buildWrapperBusLayout();
        }
        else {
            // No plugin - set safe defaults
            cachedDesc.name = "Unknown";
            cachedDesc.identifier = "unknown";
            cachedDesc.format = "Unknown";
            cachedDesc.uniqueId = 0;
            cachedPluginName = "Unknown";
            cachedHasEditor = false;
        }
    }

    ~MeteringProcessor() override = default;

    // =========================================================================
    // CRITICAL: Public getters for cached values - use these EVERYWHERE
    // =========================================================================
    bool isInstrument() const { return cachedIsInstrument; }
    const juce::String& getCachedName() const { return cachedPluginName; }
    const CachedPluginDescription& getCachedDescription() const { return cachedDesc; }

    // FREEZE FIX: Return cached name instead of calling getName() on plugin
    const juce::String getName() const override { 
        return cachedPluginName;
    }
    
    int getNumInputBuses() const { return static_cast<int>(inputBusInfo.size()); }
    int getNumOutputBuses() const { return static_cast<int>(outputBusInfo.size()); }
    
    const BusInfo* getInputBusInfo(int busIndex) const {
        if (busIndex >= 0 && busIndex < static_cast<int>(inputBusInfo.size()))
            return &inputBusInfo[busIndex];
        return nullptr;
    }
    
    const BusInfo* getOutputBusInfo(int busIndex) const {
        if (busIndex >= 0 && busIndex < static_cast<int>(outputBusInfo.size()))
            return &outputBusInfo[busIndex];
        return nullptr;
    }
    
    // CRITICAL FIX: Use cached value
    int getMainInputChannels() const { 
        return cachedIsInstrument ? 0 : 2; 
    }
    int getSidechainInputChannels() const { return hasSidechainBus ? 2 : 0; }
    bool hasSidechain() const { return hasSidechainBus; }
    bool isSidechainActive() const { return sidechainActive.load(); }
    
    bool enableSidechain() {
        if (!hasSidechainBus) return false;
        sidechainActive.store(true);
        return true;
    }
    
    void disableSidechain() {
        sidechainActive.store(false);
    }
    
    struct ChannelMapping {
        int busIndex;
        int channelInBus;
        bool isSidechain;
    };
    
    ChannelMapping mapInputChannel(int flatChannelIndex) const {
        if (flatChannelIndex < 2) {
            return { 0, flatChannelIndex, false };
        } else if (hasSidechainBus && flatChannelIndex < 4) {
            return { 1, flatChannelIndex - 2, true };
        }
        return { -1, -1, false };
    }
    
    ChannelMapping mapOutputChannel(int flatChannelIndex) const {
        if (flatChannelIndex < 2) {
            return { 0, flatChannelIndex, false };
        }
        return { -1, -1, false };
    }
    
    int getInputChannelIndex(int busIndex, int channelInBus) const {
        if (busIndex == 0) return channelInBus;
        if (busIndex == 1 && hasSidechainBus) return 2 + channelInBus;
        return -1;
    }
    
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    // FREEZE FIX: Use cached value instead of calling hasEditor() on plugin
    bool hasEditor() const override { 
        return cachedHasEditor;
    }
    
    juce::AudioProcessorEditor* createEditor() override { 
        return innerPlugin ? innerPlugin->createEditor() : nullptr; 
    }
    
    // CRITICAL FIX: Use cached value instead of getPluginDescription()
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override { 
        if (cachedIsInstrument) {
            if (layouts.inputBuses.size() > 0) return false;
            if (layouts.outputBuses.size() != 1) return false;
            return true;
        }
        
        if (layouts.inputBuses.size() > 2) return false;
        if (layouts.outputBuses.size() != 1) return false;
        
        if (hasSidechainBus && layouts.inputBuses.size() == 2) {
            if (layouts.inputBuses[0] != juce::AudioChannelSet::stereo()) return false;
            if (layouts.inputBuses[1] != juce::AudioChannelSet::stereo()) return false;
        }
        
        return true;
    }
    
    double getTailLengthSeconds() const override { 
        return innerPlugin ? innerPlugin->getTailLengthSeconds() : 0.0; 
    }
    bool acceptsMidi() const override { return innerPlugin ? innerPlugin->acceptsMidi() : false; }
    bool producesMidi() const override { return innerPlugin ? innerPlugin->producesMidi() : false; }
    bool isMidiEffect() const override { return innerPlugin ? innerPlugin->isMidiEffect() : false; }
    
    int getNumPrograms() override { return innerPlugin ? innerPlugin->getNumPrograms() : 1; }
    int getCurrentProgram() override { return innerPlugin ? innerPlugin->getCurrentProgram() : 0; }
    void setCurrentProgram(int index) override { if (innerPlugin) innerPlugin->setCurrentProgram(index); }
    const juce::String getProgramName(int index) override { 
        return innerPlugin ? innerPlugin->getProgramName(index) : juce::String(); 
    }
    void changeProgramName(int index, const juce::String& newName) override { 
        if (innerPlugin) innerPlugin->changeProgramName(index, newName); 
    }
    
    void getStateInformation(juce::MemoryBlock& destData) override { 
        if (innerPlugin) innerPlugin->getStateInformation(destData); 
    }
    void setStateInformation(const void* data, int sizeInBytes) override { 
        if (innerPlugin) innerPlugin->setStateInformation(data, sizeInBytes); 
    }

    juce::AudioPluginInstance* getInnerPlugin() { return innerPlugin.get(); }
    
    void sendAllNotesOffToPlugin();
    
    float getInputRms(int) const { return 0.0f; }
    float getOutputRms(int) const { return 0.0f; }
    int getInputRmsSize() const { return hasSidechainBus ? 4 : 2; }
    int getOutputRmsSize() const { return 2; }
    
    bool isMidiInActive() const { return midiInActiveAtomic.load(); }
    bool isMidiOutActive() const { return midiOutActiveAtomic.load(); }
    
    void setMidiChannelMask(int mask) { midiChannelMask.store(mask); }
    int getMidiChannelMask() const { return midiChannelMask.load(); }
    
    void setGain(float g) { gain.store(g); }
    float getGain() const { return gain.load(); }
    
    bool hasPluginCrashed() const { return pluginCrashed; }
    void resetCrashState() { pluginCrashed = false; }
    
    void setPassThrough(bool enabled) { passThrough.store(enabled); }
    bool isPassThrough() const { return passThrough.load(); }
    void togglePassThrough() { passThrough.store(!passThrough.load()); }
    
    void setFrozen(bool freeze) { 
        frozen.store(freeze); 
        if (innerPlugin) {
            innerPlugin->suspendProcessing(freeze);
        }
    }
    bool isFrozen() const { return frozen.load(); }

private:
    // CRITICAL FIX: Use cached value instead of getPluginDescription()
    void detectSidechainCapability() {
        if (!innerPlugin) return;
        if (cachedIsInstrument) {
            hasSidechainBus = false;
            return;
        }
        
        int numInputBuses = innerPlugin->getBusCount(true);
        if (numInputBuses < 2) {
            hasSidechainBus = false;
            return;
        }
        
        auto* bus0 = innerPlugin->getBus(true, 0);
        auto* bus1 = innerPlugin->getBus(true, 1);
        
        if (!bus0 || !bus1) {
            hasSidechainBus = false;
            return;
        }
        
        bool bus1IsAuxiliary = !bus1->isMain();
        
        juce::String bus1Name = bus1->getName().toLowerCase();
        bool hasScName = bus1Name.contains("sidechain") ||
                         bus1Name.contains("side chain") ||
                         bus1Name.contains("side-chain") ||
                         bus1Name.contains("key input") ||
                         bus1Name.contains("key") ||
                         bus1Name.contains("external") ||
                         bus1Name.contains("detector") ||
                         bus1Name.contains("aux") ||
                         bus1Name == "sc" ||
                         bus1Name.startsWith("sc ") ||
                         bus1Name.endsWith(" sc");
        
        if (bus1IsAuxiliary && hasScName) {
            hasSidechainBus = true;
        }
    }
    
    // CRITICAL FIX: Use cached value instead of getPluginDescription()
    void buildWrapperBusLayout() {
        if (!innerPlugin) return;
        
        if (!cachedIsInstrument) {
            if (auto* scBus = getBus(true, 1)) {
                scBus->enable(hasSidechainBus);
            }
        }
        
        inputBusInfo.clear();
        
        if (!cachedIsInstrument) {
            BusInfo mainInfo;
            mainInfo.busIndex = 0;
            mainInfo.name = "Main Input";
            mainInfo.numChannels = 2;
            mainInfo.isEnabled = true;
            mainInfo.isSidechain = false;
            inputBusInfo.push_back(mainInfo);
            
            if (hasSidechainBus) {
                BusInfo scInfo;
                scInfo.busIndex = 1;
                scInfo.name = "Sidechain";
                scInfo.numChannels = 2;
                scInfo.isEnabled = true;
                scInfo.isSidechain = true;
                inputBusInfo.push_back(scInfo);
            }
        }
        
        outputBusInfo.clear();
        BusInfo outInfo;
        outInfo.busIndex = 0;
        outInfo.name = "Output";
        outInfo.numChannels = 2;
        outInfo.isEnabled = true;
        outInfo.isSidechain = false;
        outputBusInfo.push_back(outInfo);
    }

    // =========================================================================
    // FREEZE FIX: Cached plugin information - set ONCE in constructor
    // =========================================================================
    bool cachedIsInstrument = false;
    juce::String cachedPluginName;
    bool cachedHasEditor = false;
    CachedPluginDescription cachedDesc;
    
    std::unique_ptr<juce::AudioPluginInstance> innerPlugin;
    std::vector<BusInfo> inputBusInfo;
    std::vector<BusInfo> outputBusInfo;
    
    std::atomic<bool> midiInActiveAtomic { false };
    std::atomic<bool> midiOutActiveAtomic { false };
    float midiInDecay = 0.0f;
    float midiOutDecay = 0.0f;
    
    // =========================================================================
    // CRITICAL FIX: Default to channel 1 ONLY (was 0xFFFF)
    // =========================================================================
    std::atomic<int> midiChannelMask { 0x0001 };
    
    // =========================================================================
    // CRITICAL FIX: Per-instance tracking variables (NOT static!)
    // =========================================================================
    int previousMidiChannelMask = 0x0001;
    juce::Array<int> lastReceivedNotes;
    bool sentPanicWhenFrozen = false;
    
    bool pluginCrashed = false;
    bool hasSidechainBus = false;
    
    std::atomic<bool> sidechainActive { false };
    
    std::atomic<float> gain { 1.0f };
    std::atomic<bool> passThrough { false };
    std::atomic<bool> frozen { false };
    juce::AudioBuffer<float> internalBuffer;
};

// ==============================================================================
// Main Processor
// ==============================================================================
class SubterraneumAudioProcessor : public juce::AudioProcessor, public juce::ChangeListener {
public:
    SubterraneumAudioProcessor();
    ~SubterraneumAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    void resetGraph();
    void loadUserPreset(const juce::File& file);
    void saveUserPreset(const juce::File& file);
    void removeNode(juce::AudioProcessorGraph::NodeID nodeID);
    void toggleBypass(juce::AudioProcessorGraph::NodeID nodeID);

    void storeMultiStates();
    void restoreMultiStates();
    void resetBlacklist();
    
    juce::StringArray getSupportedFormatNames() const;
    
    // =========================================================================
    // NEW: Format identification helpers for VST2 support
    // =========================================================================
    static juce::String getShortFormatName(const juce::String& fullFormatName);
    static juce::Colour getFormatColor(const juce::String& formatName);

    std::unique_ptr<juce::AudioProcessorGraph> mainGraph;
    juce::AudioProcessorGraph::Node::Ptr audioInputNode, audioOutputNode, midiInputNode, midiOutputNode;
    juce::KnownPluginList knownPluginList;
    juce::AudioPluginFormatManager formatManager;
    juce::ApplicationProperties appProperties;
    juce::PropertiesFile::Options options;

    void saveAudioSettings();
    void loadAudioSettings();
    juce::String getSavedAudioDeviceName() const { return savedAudioDeviceName; }
    
    void updateIOChannelCount();
    juce::StringArray inputChannelNames, outputChannelNames;
    juce::String getDeviceInputChannelName(int i) const { 
        return (i >= 0 && i < inputChannelNames.size()) ? inputChannelNames[i] : juce::String(i+1); 
    }
    juce::String getDeviceOutputChannelName(int i) const { 
        return (i >= 0 && i < outputChannelNames.size()) ? outputChannelNames[i] : juce::String(i+1); 
    }
    
    std::atomic<bool> audioInputEnabled { true };
    std::atomic<bool> audioOutputEnabled { true };
    std::atomic<bool> midiInputEnabled { true };
    
    std::atomic<double> masterTempo { 120.0 };
    std::atomic<int> masterTimeSigNumerator { 4 };
    std::atomic<int> masterTimeSigDenominator { 4 };
    
    std::atomic<float> mainInputRms[2] = { {0.0f}, {0.0f} };
    std::atomic<float> mainOutputRms[2] = { {0.0f}, {0.0f} };
    std::atomic<bool> mainMidiInFlash { false };
    std::atomic<bool> mainMidiOutFlash { false };
    int mainMidiInNoteCount = 0;
    
    static constexpr int maxMixerChannels = 64;
    std::atomic<float> inputGains[maxMixerChannels];
    std::atomic<float> outputGains[maxMixerChannels];
    
    void setInputGain(int channel, float gain) { 
        if (channel >= 0 && channel < maxMixerChannels) inputGains[channel].store(gain); 
    }
    void setOutputGain(int channel, float gain) { 
        if (channel >= 0 && channel < maxMixerChannels) outputGains[channel].store(gain); 
    }
    float getInputGain(int channel) const { 
        return (channel >= 0 && channel < maxMixerChannels) ? inputGains[channel].load() : 1.0f; 
    }
    float getOutputGain(int channel) const { 
        return (channel >= 0 && channel < maxMixerChannels) ? outputGains[channel].load() : 1.0f; 
    }
    
    void updateActiveChannels();
    
    bool startRecording();
    void stopRecording();
    bool isCurrentlyRecording() const { return isRecording.load(); }
    juce::File getLastRecordingFile() const { return lastRecordingFile; }
    
    bool sortPluginsByVendor = true;
    bool instrumentSelectorMultiMode = true;  // FIX #2: Multi-mode is now default
    
    static juce::AudioDeviceManager* standaloneDeviceManager;
    
    juce::MidiKeyboardState keyboardState;
    
    void updateHardwareMidiChannelMasks();
    void applyHardwareMidiChannelFiltering(juce::MidiBuffer& midiMessages);
    void sendMidiPanicToAllInstruments();

private:
    void updateGraph();
    void initializePluginFormats();
    std::map<juce::AudioProcessorGraph::NodeID, bool> multiModeBypassStates;
    juce::String savedAudioDeviceName;
    std::atomic<bool> isRecording { false };
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> backgroundWriter;
    juce::TimeSliceThread writerThread { "Audio Recorder Thread" };
    juce::File lastRecordingFile;
    
    int meterUpdateCounter = 0;
    static constexpr int meterUpdateInterval = 8;
    
    std::map<juce::String, int> hardwareMidiChannelMasks;
    
    // =========================================================================
    // CRITICAL FIX: Per-instance tracking (NOT static!)
    // =========================================================================
    int lastHardwareMidiMask = 0x0001;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SubterraneumAudioProcessor)
};

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();
