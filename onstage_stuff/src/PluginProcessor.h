#pragma once

#include <JuceHeader.h>

// Forward declaration
class SubterraneumAudioProcessorEditor;

// =============================================================================
// CLAP Host Format (Conditional - requires clap-wrapper)
// =============================================================================
#if SUBTERRANEUM_CLAP_HOSTING
// Include CLAP hosting headers when available
// #include "clap-wrapper/include/clap_juce_hosting.h"
#endif

// ==============================================================================
// Metering Processor
// ==============================================================================
class MeteringProcessor : public juce::AudioProcessor {
public:
    MeteringProcessor(std::unique_ptr<juce::AudioPluginInstance> pluginToWrap)
        : AudioProcessor(makeBusesProperties(pluginToWrap.get())),
          innerPlugin(std::move(pluginToWrap)) 
    {
        if (innerPlugin) {
            bool isInstrument = innerPlugin->getPluginDescription().isInstrument;
            int realInputs = innerPlugin->getTotalNumInputChannels();
            
            // For instruments: only use inputs if the plugin actually has them
            // For effects: always need at least stereo input
            int inChans = isInstrument ? realInputs : juce::jmax(2, realInputs);
            int outChans = juce::jmax(2, innerPlugin->getTotalNumOutputChannels());
            
            setPlayConfigDetails(inChans, outChans,
                                 innerPlugin->getSampleRate() > 0 ? innerPlugin->getSampleRate() : 44100.0,
                                 innerPlugin->getBlockSize() > 0 ? innerPlugin->getBlockSize() : 512);
        }
    }

    ~MeteringProcessor() override = default;

    const juce::String getName() const override { return innerPlugin ? innerPlugin->getName() : "Unknown"; }
    
    void prepareToPlay(double sampleRate, int samplesPerBlock) override {
        if (innerPlugin) {
            bool isInstrument = innerPlugin->getPluginDescription().isInstrument;
            int realInputs = innerPlugin->getTotalNumInputChannels();
            
            int inChans = isInstrument ? realInputs : juce::jmax(2, realInputs);
            int outChans = juce::jmax(2, innerPlugin->getTotalNumOutputChannels());
            
            innerPlugin->setPlayConfigDetails(inChans, outChans, sampleRate, samplesPerBlock);
            innerPlugin->prepareToPlay(sampleRate, samplesPerBlock);
        }
        inputRms.resize(juce::jmax(2, getTotalNumInputChannels()), 0.0f);
        outputRms.resize(juce::jmax(2, getTotalNumOutputChannels()), 0.0f);
    }
    
    void releaseResources() override { if (innerPlugin) innerPlugin->releaseResources(); }
    
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override {
        // Safety check
        if (buffer.getNumSamples() == 0) return;
        
        // If frozen (bypassed for optimization), skip all processing and clear output
        if (frozen.load()) {
            // For effects, pass audio through; for instruments, clear
            if (innerPlugin && innerPlugin->getPluginDescription().isInstrument) {
                buffer.clear();
            }
            // Clear metering
            for (auto& rms : inputRms) rms = 0.0f;
            for (auto& rms : outputRms) rms = 0.0f;
            midiInActive = false;
            midiOutActive = false;
            return;
        }
        
        // Input Metering
        int numInputChannels = juce::jmin(getTotalNumInputChannels(), buffer.getNumChannels());
        for (int i = 0; i < numInputChannels; ++i) {
            inputRms[i] = buffer.getRMSLevel(i, 0, buffer.getNumSamples());
        }
        
        // MIDI In Logic
        if (!midiMessages.isEmpty()) {
            midiInActive = true;
            midiInDecay = 1.0f;
        } else {
            midiInDecay *= 0.9f;
            if (midiInDecay < 0.01f) midiInActive = false;
        }

        // Channel Filtering (for instruments)
        if (innerPlugin && innerPlugin->getPluginDescription().isInstrument && midiChannelMask != 0xFFFF) {
            juce::MidiBuffer filtered;
            for (const auto metadata : midiMessages) {
                auto msg = metadata.getMessage();
                if (msg.isNoteOnOrOff()) {
                    int ch = msg.getChannel();
                    if ((midiChannelMask >> (ch - 1)) & 1) filtered.addEvent(msg, metadata.samplePosition);
                } else {
                    filtered.addEvent(msg, metadata.samplePosition);
                }
            }
            midiMessages.swapWith(filtered);
        }

        // Process Inner Plugin (skip if passThrough is enabled for effects)
        bool isEffect = innerPlugin && !innerPlugin->getPluginDescription().isInstrument;
        bool shouldProcess = innerPlugin && !pluginCrashed && !(isEffect && passThrough.load());
        
        if (shouldProcess) {
            try {
                innerPlugin->processBlock(buffer, midiMessages);
            } catch (...) {
                // Plugin crashed - mark it and output silence
                pluginCrashed = true;
                buffer.clear();
            }
        }
        // If passThrough is enabled for effects, audio passes through unchanged
        
        // If plugin crashed, output silence
        if (pluginCrashed) {
            buffer.clear();
        }
        
        // Apply instrument gain (for instruments only)
        if (innerPlugin && innerPlugin->getPluginDescription().isInstrument) {
            float g = gain.load();
            if (g != 1.0f) {
                buffer.applyGain(g);
            }
        }

        // Output Metering
        int numOutputChannels = juce::jmin(getTotalNumOutputChannels(), buffer.getNumChannels());
        for (int i = 0; i < numOutputChannels; ++i) {
            outputRms[i] = buffer.getRMSLevel(i, 0, buffer.getNumSamples());
        }

        // MIDI Out Logic
        if (innerPlugin && !midiMessages.isEmpty() && innerPlugin->producesMidi()) {
            midiOutActive = true;
            midiOutDecay = 1.0f;
        } else {
            midiOutDecay *= 0.9f;
            if (midiOutDecay < 0.01f) midiOutActive = false;
        }
    }

    bool hasEditor() const override { return innerPlugin && innerPlugin->hasEditor(); }
    juce::AudioProcessorEditor* createEditor() override { return innerPlugin ? innerPlugin->createEditor() : nullptr; }
    
    // Accept any bus layout for maximum compatibility
    bool isBusesLayoutSupported(const BusesLayout&) const override { return true; }
    
    double getTailLengthSeconds() const override { return innerPlugin ? innerPlugin->getTailLengthSeconds() : 0.0; }
    bool acceptsMidi() const override { return innerPlugin ? innerPlugin->acceptsMidi() : false; }
    bool producesMidi() const override { return innerPlugin ? innerPlugin->producesMidi() : false; }
    bool isMidiEffect() const override { return innerPlugin ? innerPlugin->isMidiEffect() : false; }
    
    int getNumPrograms() override { return innerPlugin ? innerPlugin->getNumPrograms() : 0; }
    int getCurrentProgram() override { return innerPlugin ? innerPlugin->getCurrentProgram() : 0; }
    void setCurrentProgram(int index) override { if (innerPlugin) innerPlugin->setCurrentProgram(index); }
    const juce::String getProgramName(int index) override { return innerPlugin ? innerPlugin->getProgramName(index) : juce::String(); }
    void changeProgramName(int index, const juce::String& newName) override { if (innerPlugin) innerPlugin->changeProgramName(index, newName); }
    
    void getStateInformation(juce::MemoryBlock& destData) override { if (innerPlugin) innerPlugin->getStateInformation(destData); }
    void setStateInformation(const void* data, int sizeInBytes) override { if (innerPlugin) innerPlugin->setStateInformation(data, sizeInBytes); }

    juce::AudioPluginInstance* getInnerPlugin() { return innerPlugin.get(); }
    float getInputRms(int channel) const { return (channel < inputRms.size()) ? inputRms[channel] : 0.0f; }
    float getOutputRms(int channel) const { return (channel < outputRms.size()) ? outputRms[channel] : 0.0f; }
    bool isMidiInActive() const { return midiInActive; }
    bool isMidiOutActive() const { return midiOutActive; }
    
    void setMidiChannelMask(int mask) { midiChannelMask = mask; }
    int getMidiChannelMask() const { return midiChannelMask; }
    
    // Instrument gain (for mixer)
    void setGain(float g) { gain.store(g); }
    float getGain() const { return gain.load(); }
    
    bool hasPluginCrashed() const { return pluginCrashed; }
    void resetCrashState() { pluginCrashed = false; }
    
    // Pass-through mode (for effects only - bypasses processing but passes audio through)
    void setPassThrough(bool enabled) { passThrough.store(enabled); }
    bool isPassThrough() const { return passThrough.load(); }
    
    // Freeze mode - completely suspends processing for CPU optimization
    // Use when plugin is bypassed/muted and you want to save CPU
    void setFrozen(bool freeze) { 
        frozen.store(freeze); 
        // Optionally suspend the inner plugin to release resources
        if (innerPlugin) {
            if (freeze) {
                innerPlugin->suspendProcessing(true);
            } else {
                innerPlugin->suspendProcessing(false);
            }
        }
    }
    bool isFrozen() const { return frozen.load(); }

private:
    // Helper to create appropriate bus properties based on plugin type
    static BusesProperties makeBusesProperties(juce::AudioPluginInstance* plugin) {
        BusesProperties props;
        
        if (plugin) {
            bool isInstrument = plugin->getPluginDescription().isInstrument;
            int realInputs = plugin->getTotalNumInputChannels();
            
            // Only add input bus if:
            // - It's an effect (always needs inputs)
            // - It's an instrument that actually has inputs (e.g., vocoder, sampler with sidechain)
            if (!isInstrument || realInputs > 0) {
                props = props.withInput("Input", juce::AudioChannelSet::stereo(), true);
            }
            
            // Always add output bus
            props = props.withOutput("Output", juce::AudioChannelSet::stereo(), true);
        } else {
            // Fallback - stereo in/out
            props = props.withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true);
        }
        
        return props;
    }

    std::unique_ptr<juce::AudioPluginInstance> innerPlugin;
    std::vector<float> inputRms;
    std::vector<float> outputRms;
    bool midiInActive = false;
    bool midiOutActive = false;
    float midiInDecay = 0.0f;
    float midiOutDecay = 0.0f;
    int midiChannelMask = 0xFFFF;
    bool pluginCrashed = false;
    std::atomic<float> gain { 1.0f };
    std::atomic<bool> passThrough { false };
    std::atomic<bool> frozen { false };
};

// ==============================================================================
// Main Processor Class
// ==============================================================================
class SubterraneumAudioProcessor : public juce::AudioProcessor, public juce::ChangeListener {
public:
    SubterraneumAudioProcessor();
    ~SubterraneumAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

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
    
    // Audio Settings Persistence
    void saveAudioSettings();
    void loadAudioSettings();
    juce::String getSavedAudioDeviceName() const { return savedAudioDeviceName; }
    
    // I/O Channel Update (called when device changes)
    void updateIOChannelCount();
    
    // Get supported plugin format names for display
    juce::StringArray getSupportedFormatNames() const;

    std::unique_ptr<juce::AudioProcessorGraph> mainGraph;
    juce::AudioPluginFormatManager formatManager;
    juce::KnownPluginList knownPluginList;
    juce::PropertiesFile::Options options;
    juce::ApplicationProperties appProperties;
    
    // Using Ptr to match juce::AudioProcessorGraph::addNode return type
    juce::AudioProcessorGraph::Node::Ptr audioInputNode;
    juce::AudioProcessorGraph::Node::Ptr audioOutputNode;
    juce::AudioProcessorGraph::Node::Ptr midiInputNode;
    juce::AudioProcessorGraph::Node::Ptr midiOutputNode;

    // Audio I/O Enable flags
    std::atomic<bool> audioInputEnabled { true };
    std::atomic<bool> audioOutputEnabled { true };

    // Master tempo and time signature for VSTi sync
    std::atomic<double> masterTempo { 120.0 };
    std::atomic<int> masterTimeSigNumerator { 4 };
    std::atomic<int> masterTimeSigDenominator { 4 };

    std::atomic<float> mainInputRms[2] { 0.0f, 0.0f };
    std::atomic<float> mainOutputRms[2] { 0.0f, 0.0f };
    std::atomic<int> mainMidiInNoteCount { 0 };
    std::atomic<int> mainMidiOutNoteCount { 0 };
    std::atomic<bool> mainMidiInFlash { false };
    std::atomic<bool> mainMidiOutFlash { false };
    
    // Channel names from audio interface
    juce::StringArray inputChannelNames;
    juce::StringArray outputChannelNames;
    
    // Get channel name for display (renamed to avoid conflict with JUCE's virtual functions)
    juce::String getDeviceInputChannelName(int index) const { 
        if (index >= 0 && index < inputChannelNames.size()) 
            return inputChannelNames[index];
        return "Input " + juce::String(index + 1); 
    }
    juce::String getDeviceOutputChannelName(int index) const { 
        if (index >= 0 && index < outputChannelNames.size()) 
            return outputChannelNames[index];
        return "Output " + juce::String(index + 1); 
    }
    
    // Mixer gain storage (up to 64 channels)
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
    
    // Update active channels based on connections
    void updateActiveChannels();
    
    // ==========================================================================
    // Recording System (using ThreadedWriter for reliable file writing)
    // ==========================================================================
    bool startRecording();
    void stopRecording();
    bool isCurrentlyRecording() const { return isRecording.load(); }
    juce::File getLastRecordingFile() const { return lastRecordingFile; }
    
    bool sortPluginsByVendor = true;
    bool instrumentSelectorMultiMode = false;
    
    static juce::AudioDeviceManager* standaloneDeviceManager;
    juce::MidiKeyboardState keyboardState;

private:
    void updateGraph();
    void initializePluginFormats();
    std::map<juce::AudioProcessorGraph::NodeID, bool> multiModeBypassStates;
    
    // Saved audio device name for persistence
    juce::String savedAudioDeviceName;
    
    // Recording state (using ThreadedWriter for reliable file writing)
    std::atomic<bool> isRecording { false };
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> backgroundWriter;
    juce::TimeSliceThread writerThread { "Audio Recorder Thread" };
    juce::File lastRecordingFile;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SubterraneumAudioProcessor)
};