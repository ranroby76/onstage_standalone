#pragma once

#include <JuceHeader.h>

// =============================================================================
// SimpleConnectorProcessor - A bus/summing module for routing audio with amp
// Purple colored module with volume slider, mute and delete buttons
// Stereo input to stereo output
// Volume: 0 = silence, middle = unity (0dB), max = +35dB
// =============================================================================
class SimpleConnectorProcessor : public juce::AudioProcessor {
public:
    SimpleConnectorProcessor();
    ~SimpleConnectorProcessor() override = default;

    const juce::String getName() const override { return "Connector/Amp"; }
    
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
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
    
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    
    // Volume: 0.0 = silence, 0.5 = unity (0dB), 1.0 = +35dB
    // 100 steps resolution (0.01 increments)
    void setVolume(float normalizedValue);
    float getVolume() const { return volumeNormalized.load(); }
    float getVolumeDb() const;
    
    void setMuted(bool shouldMute) { muted.store(shouldMute); }
    bool isMuted() const { return muted.load(); }
    void toggleMute() { muted.store(!muted.load()); }
    
    static constexpr const char* getIdentifier() { return "SimpleConnector"; }
    
private:
    std::atomic<float> volumeNormalized { 0.5f };  // Default = unity gain
    std::atomic<bool> muted { false };
    
    std::vector<float> inputRms;
    std::vector<float> outputRms;
    
    float normalizedToGain(float normalized) const;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SimpleConnectorProcessor)
};
