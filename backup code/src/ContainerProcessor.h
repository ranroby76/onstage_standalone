// #D:\Workspace\Subterraneum_plugins_daw\src\ContainerProcessor.h
// ContainerProcessor — A sub-graph node that hosts an entire plugin chain
// Each container wraps its own AudioProcessorGraph with configurable I/O buses.
// Appears as a single node on the main graph. Double-click to dive in.
// Containers are instruments in InstrumentSelector and support save/load presets.
// NEW: Full per-container transport override (custom tempo/time-sig, like MeteringProcessor)

#pragma once

#include <JuceHeader.h>

class SubterraneumAudioProcessor;

// =============================================================================
// ContainerProcessor — Sub-graph node
// =============================================================================
class ContainerProcessor : public juce::AudioProcessor {
public:
    // =========================================================================
    // Bus configuration — user-definable I/O
    // =========================================================================
    struct BusConfig {
        juce::String name;
        int numChannels = 2;  // stereo by default
    };

    // =========================================================================
    // Per-Container Transport PlayHead
    // Mirrors MeteringProcessor::PluginTransportPlayHead
    // =========================================================================
    class ContainerTransportPlayHead : public juce::AudioPlayHead {
    public:
        ContainerTransportPlayHead(ContainerProcessor& ownerRef) : owner(ownerRef) {}

        juce::Optional<PositionInfo> getPosition() const override
        {
            if (owner.transportSyncedToMaster.load())
            {
                if (auto* parentHead = owner.parentPlayHead)
                    return parentHead->getPosition();

                PositionInfo info;
                info.setBpm(120.0);
                info.setTimeSignature(juce::AudioPlayHead::TimeSignature { 4, 4 });
                info.setIsPlaying(false);
                return info;
            }

            PositionInfo info;
            if (auto* parentHead = owner.parentPlayHead)
            {
                auto parentPos = parentHead->getPosition();
                if (parentPos.hasValue())
                    info = *parentPos;
            }

            info.setBpm(owner.customTempo.load());
            info.setTimeSignature(juce::AudioPlayHead::TimeSignature {
                owner.customTimeSigNum.load(),
                owner.customTimeSigDen.load() });

            return info;
        }

    private:
        ContainerProcessor& owner;
    };

    friend class ContainerTransportPlayHead;

    ContainerProcessor();
    ~ContainerProcessor() override;

    // =========================================================================
    // AudioProcessor overrides
    // =========================================================================
    const juce::String getName() const override { return containerName; }

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    double getTailLengthSeconds() const override { return 0.0; }

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    // CRITICAL: These must return true so addBus()/removeBus() work at runtime
    bool canAddBus(bool /*isInput*/) const override { return true; }
    bool canRemoveBus(bool /*isInput*/) const override { return true; }

    // =========================================================================
    // Container identity
    // =========================================================================
    void setContainerName(const juce::String& name) { containerName = name; }
    const juce::String& getContainerName() const { return containerName; }

    static constexpr const char* getIdentifier() { return "Container"; }

    // =========================================================================
    // Inner graph access — used by GraphCanvas for dive-in rendering
    // =========================================================================
    juce::AudioProcessorGraph& getInnerGraph() { return *innerGraph; }
    const juce::AudioProcessorGraph& getInnerGraph() const { return *innerGraph; }

    // Inner I/O node references (for canvas rendering inside container)
    juce::AudioProcessorGraph::Node::Ptr getInnerAudioInput() const { return innerAudioInput; }
    juce::AudioProcessorGraph::Node::Ptr getInnerAudioOutput() const { return innerAudioOutput; }
    juce::AudioProcessorGraph::Node::Ptr getInnerMidiInput() const { return innerMidiInput; }
    juce::AudioProcessorGraph::Node::Ptr getInnerMidiOutput() const { return innerMidiOutput; }

    // Aliases used by GraphCanvas navigation
    juce::AudioProcessorGraph::Node::Ptr getInnerAudioInputNode() const { return innerAudioInput; }
    juce::AudioProcessorGraph::Node::Ptr getInnerAudioOutputNode() const { return innerAudioOutput; }
    juce::AudioProcessorGraph::Node::Ptr getInnerMidiInputNode() const { return innerMidiInput; }
    juce::AudioProcessorGraph::Node::Ptr getInnerMidiOutputNode() const { return innerMidiOutput; }

    // Pointer accessor for getActiveGraph()
    juce::AudioProcessorGraph* getInnerGraphPtr() { return innerGraph.get(); }

    // =========================================================================
    // I/O bus configuration
    // Default: 1 stereo input + 1 stereo output
    // User can add/remove buses and rename them
    // =========================================================================
    int getNumContainerInputBuses() const { return static_cast<int>(inputBusConfigs.size()); }
    int getNumContainerOutputBuses() const { return static_cast<int>(outputBusConfigs.size()); }

    const BusConfig& getInputBusConfig(int index) const { return inputBusConfigs[static_cast<size_t>(index)]; }
    const BusConfig& getOutputBusConfig(int index) const { return outputBusConfigs[static_cast<size_t>(index)]; }

    void setInputBusName(int index, const juce::String& name);
    void setOutputBusName(int index, const juce::String& name);

    // Add/remove buses (rebuilds inner graph I/O nodes)
    void addInputBus(const juce::String& name = "Input", int numChannels = 2);
    void addOutputBus(const juce::String& name = "Output", int numChannels = 2);
    void removeInputBus(int index);
    void removeOutputBus(int index);

    // =========================================================================
    // Serialization — for container preset save/load files
    // =========================================================================
    juce::String serializeToXml() const;
    bool restoreFromXml(const juce::String& xmlStr);

    // =========================================================================
    // Container preset folder (global, like recording/sampler folders)
    // Default: C:\Users\{user}\Documents\Colosseum\containers
    // =========================================================================
    static juce::File getGlobalDefaultFolder();
    static void setGlobalDefaultFolder(const juce::File& folder);
    static juce::File getEffectiveDefaultFolder();

    // Save/load container preset files (.container)
    bool savePreset(const juce::File& file) const;
    bool loadPreset(const juce::File& file);

    // =========================================================================
    // Reference to parent processor (for format manager access when loading
    // plugins inside the container)
    // =========================================================================
    void setParentProcessor(SubterraneumAudioProcessor* parent) { parentProcessor = parent; }
    SubterraneumAudioProcessor* getParentProcessor() const { return parentProcessor; }

    // =========================================================================
    // Volume / Mute (same pattern as SimpleConnectorProcessor)
    // =========================================================================
    void setVolume(float normalizedValue) { volumeNormalized.store(juce::jlimit(0.0f, 1.0f, normalizedValue)); }
    float getVolume() const { return volumeNormalized.load(); }
    float getVolumeDb() const;

    void setMuted(bool shouldMute) { muted.store(shouldMute); }
    bool isMuted() const { return muted.load(); }
    void toggleMute() { muted.store(!muted.load()); }

    // Container is always treated as an instrument for InstrumentSelector
    bool isContainerInstrument() const { return true; }

    // =========================================================================
    // Per-Container Transport Override API
    // Mirrors MeteringProcessor transport API exactly
    // =========================================================================
    bool isTransportSynced() const { return transportSyncedToMaster.load(); }
    void setTransportSynced(bool synced) { transportSyncedToMaster.store(synced); }

    double getCustomTempo() const { return customTempo.load(); }
    void setCustomTempo(double bpm) { customTempo.store(juce::jlimit(20.0, 999.0, bpm)); }

    int getCustomTimeSigNumerator() const { return customTimeSigNum.load(); }
    int getCustomTimeSigDenominator() const { return customTimeSigDen.load(); }
    void setCustomTimeSignature(int num, int den) {
        customTimeSigNum.store(num);
        customTimeSigDen.store(den);
    }

    // Called by the host graph to give us the parent playhead reference
    void setParentPlayHead(juce::AudioPlayHead* head) { parentPlayHead = head; }

    // =========================================================================
    // Dynamic I/O configuration — reconfigure audio buses at runtime
    // Returns true if layout changed (caller should rebuild graph connections)
    // =========================================================================
    bool setAudioIOConfiguration(int numInputChannels, int numOutputChannels);

private:
    // =========================================================================
    // Inner graph
    // =========================================================================
    std::unique_ptr<juce::AudioProcessorGraph> innerGraph;

    juce::AudioProcessorGraph::Node::Ptr innerAudioInput;
    juce::AudioProcessorGraph::Node::Ptr innerAudioOutput;
    juce::AudioProcessorGraph::Node::Ptr innerMidiInput;
    juce::AudioProcessorGraph::Node::Ptr innerMidiOutput;

    // =========================================================================
    // Configuration
    // =========================================================================
    juce::String containerName { "Container" };
    std::vector<BusConfig> inputBusConfigs;
    std::vector<BusConfig> outputBusConfigs;

    // =========================================================================
    // Volume/mute
    // =========================================================================
    std::atomic<float> volumeNormalized { 0.5f };  // 0.5 = unity gain
    std::atomic<bool> muted { false };

    // =========================================================================
    // Per-Container Transport Override state
    // =========================================================================
    std::atomic<bool> transportSyncedToMaster { true };
    std::atomic<double> customTempo { 120.0 };
    std::atomic<int> customTimeSigNum { 4 };
    std::atomic<int> customTimeSigDen { 4 };
    juce::AudioPlayHead* parentPlayHead = nullptr;
    ContainerTransportPlayHead containerTransportPlayHead { *this };

    float normalizedToGain(float normalized) const;

    // =========================================================================
    // Parent processor reference
    // =========================================================================
    SubterraneumAudioProcessor* parentProcessor = nullptr;

    // =========================================================================
    // Global container folder
    // =========================================================================
    static juce::File globalDefaultFolder;

    // =========================================================================
    // Internal helpers
    // =========================================================================
    void rebuildBusLayout();
    void createInnerIONodes();
    void updateInnerIONodes();  // FIX: Update I/O nodes when bus configuration changes

    // Build BusesProperties from current bus configs
    static BusesProperties makeBusesProperties(const std::vector<BusConfig>& inputs,
                                                const std::vector<BusConfig>& outputs);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ContainerProcessor)
};
