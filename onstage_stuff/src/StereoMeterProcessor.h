#pragma once

#include <JuceHeader.h>

// =============================================================================
// StereoMeterProcessor - Visual stereo level meter module
// Shows L/R bars with green->yellow->red gradient
// 3x height of standard module, with clipping LED at bottom
// =============================================================================
class StereoMeterProcessor : public juce::AudioProcessor {
public:
    StereoMeterProcessor();
    ~StereoMeterProcessor() override = default;

    const juce::String getName() const override { return "Stereo Meter"; }
    
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
    
    // Get current levels (0.0 - 1.0+, can exceed 1.0 for clipping)
    float getLeftLevel() const { return leftLevel.load(); }
    float getRightLevel() const { return rightLevel.load(); }
    
    // Get peak hold levels
    float getLeftPeak() const { return leftPeak.load(); }
    float getRightPeak() const { return rightPeak.load(); }
    
    // Clipping detection
    bool isLeftClipping() const { return leftClipping.load(); }
    bool isRightClipping() const { return rightClipping.load(); }
    bool isClipping() const { return leftClipping.load() || rightClipping.load(); }
    
    // Reset clipping indicators
    void resetClipping() { 
        leftClipping.store(false); 
        rightClipping.store(false);
        leftPeak.store(0.0f);
        rightPeak.store(0.0f);
    }
    
    static constexpr const char* getIdentifier() { return "StereoMeter"; }
    
private:
    std::atomic<float> leftLevel { 0.0f };
    std::atomic<float> rightLevel { 0.0f };
    std::atomic<float> leftPeak { 0.0f };
    std::atomic<float> rightPeak { 0.0f };
    std::atomic<bool> leftClipping { false };
    std::atomic<bool> rightClipping { false };
    
    float levelDecay = 0.95f;
    int peakHoldSamples = 0;
    int leftPeakHoldCounter = 0;
    int rightPeakHoldCounter = 0;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StereoMeterProcessor)
};
