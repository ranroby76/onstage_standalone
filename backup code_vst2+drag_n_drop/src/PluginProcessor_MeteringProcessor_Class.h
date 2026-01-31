// D:\Workspace\Subterraneum_plugins_daw\src\PluginProcessor_MeteringProcessor_Class.h
// PARTIAL FILE - MeteringProcessor class only
// This replaces the MeteringProcessor class definition in PluginProcessor.h
// CRITICAL FIX: Cache ALL plugin information to prevent freeze bugs
// Some plugins (like SOLO by Taqs.im) freeze when getName() or getPluginDescription() is called repeatedly

// ==============================================================================
// Metering Processor - COMPLETE FREEZE FIX: Cache everything in constructor
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
    
    MeteringProcessor(std::unique_ptr<juce::AudioPluginInstance> pluginToWrap)
        : AudioProcessor(pluginToWrap && pluginToWrap->getPluginDescription().isInstrument
            ? BusesProperties()
                .withOutput("Output", juce::AudioChannelSet::stereo(), true)
            : BusesProperties()
                .withInput("Input", juce::AudioChannelSet::stereo(), true)
                .withInput("Sidechain", juce::AudioChannelSet::stereo(), false)
                .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
          innerPlugin(std::move(pluginToWrap)) 
    {
        if (innerPlugin) {
            // =========================================================================
            // FREEZE FIX: Cache ALL plugin information ONCE during construction
            // NEVER call these methods again - use cached values!
            // =========================================================================
            try {
                // Cache plugin description (for preset save)
                auto desc = innerPlugin->getPluginDescription();
                cachedDesc.name = desc.name;
                cachedDesc.identifier = desc.fileOrIdentifier;
                cachedDesc.format = desc.pluginFormatName;
                cachedDesc.uniqueId = desc.uniqueId;
                cachedIsInstrument = desc.isInstrument;
                
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
                cachedIsInstrument = false;
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
            cachedIsInstrument = false;
            cachedPluginName = "Unknown";
            cachedHasEditor = false;
        }
    }

    ~MeteringProcessor() override = default;

    // =========================================================================
    // FREEZE FIX: Return cached values - NO plugin method calls!
    // =========================================================================
    const juce::String getName() const override { 
        return cachedPluginName;  // Use cached name!
    }
    
    // Public getters for cached information
    const juce::String& getCachedName() const { return cachedPluginName; }
    const CachedPluginDescription& getCachedDescription() const { return cachedDesc; }
    bool isInstrument() const { return cachedIsInstrument; }
    
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
    
    int getMainInputChannels() const { 
        return cachedIsInstrument ? 0 : 2;  // Use cached value!
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

    bool hasEditor() const override { 
        return cachedHasEditor;  // Use cached value!
    }
    
    juce::AudioProcessorEditor* createEditor() override { 
        return innerPlugin ? innerPlugin->createEditor() : nullptr; 
    }
    
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override { 
        if (cachedIsInstrument) {  // Use cached value!
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
    void detectSidechainCapability() {
        if (!innerPlugin) return;
        if (cachedIsInstrument) {  // Use cached value!
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
    
    void buildWrapperBusLayout() {
        if (!innerPlugin) return;
        
        if (!cachedIsInstrument) {  // Use cached value!
            if (auto* scBus = getBus(true, 1)) {
                scBus->enable(hasSidechainBus);
            }
        }
        
        inputBusInfo.clear();
        
        if (!cachedIsInstrument) {  // Use cached value!
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
    // FREEZE FIX: Cached plugin information
    // =========================================================================
    juce::String cachedPluginName;
    bool cachedHasEditor = false;
    bool cachedIsInstrument = false;
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
    // These MUST be member variables, not static locals in processBlock!
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