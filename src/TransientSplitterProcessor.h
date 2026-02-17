// #D:\Workspace\Subterraneum_plugins_daw\src\TransientSplitterProcessor.h
// TRANSIENT SPLITTER - Splits audio into transient (attack) and sustain (tonal) components
// 2-in, 4-out: Transient L/R (ch 0-1), Sustain L/R (ch 2-3)
// Zero latency, real-time envelope follower based detection

#pragma once

#include <JuceHeader.h>
#include <atomic>

class TransientSplitterProcessor : public juce::AudioProcessor {
public:
    TransientSplitterProcessor();
    ~TransientSplitterProcessor() override = default;

    const juce::String getName() const override { return "Transient Splitter"; }

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

    static constexpr const char* getIdentifier() { return "TransientSplitter"; }

    // =========================================================================
    // Parameters (all atomic for thread safety)
    // =========================================================================
    
    // Detection
    std::atomic<float> sensitivity { 0.5f };      // 0.0–1.0, how aggressively transients are detected
    std::atomic<float> decay { 50.0f };            // ms, how long signal stays classified as transient
    std::atomic<float> holdTime { 10.0f };         // ms, minimum transient gate open time
    std::atomic<float> smoothing { 2.0f };         // ms, crossfade smoothness at split boundary
    
    // Frequency focus (detection sidechain filter)
    std::atomic<float> focusHPFreq { 20.0f };      // Hz, high-pass on detection (20 = off)
    std::atomic<float> focusLPFreq { 20000.0f };   // Hz, low-pass on detection (20k = off)
    
    // Output
    std::atomic<float> transientGainDb { 0.0f };   // dB, -inf to +12
    std::atomic<float> sustainGainDb { 0.0f };     // dB, -inf to +12
    std::atomic<float> balance { 0.0f };           // -1.0 (all transient) to +1.0 (all sustain), 0 = clean split
    
    // Modes
    std::atomic<bool> stereoLinked { true };        // true = mono detection, false = independent L/R
    std::atomic<bool> gateMode { false };           // true = hard gate, false = proportional split
    std::atomic<bool> invertMode { false };         // true = swap transient/sustain outputs
    
    // Metering (read-only, for UI)
    std::atomic<float> transientRmsL { 0.0f };
    std::atomic<float> transientRmsR { 0.0f };
    std::atomic<float> sustainRmsL { 0.0f };
    std::atomic<float> sustainRmsR { 0.0f };
    std::atomic<float> transientActivity { 0.0f };  // 0.0–1.0, current transient detection level

private:
    double currentSampleRate = 44100.0;
    
    // Envelope followers (per channel)
    float fastEnvL = 0.0f, fastEnvR = 0.0f;   // Fast attack envelope
    float slowEnvL = 0.0f, slowEnvR = 0.0f;   // Slow reference envelope
    float gateL = 0.0f, gateR = 0.0f;          // Current transient gate value (0–1)
    float smoothGateL = 0.0f, smoothGateR = 0.0f; // Smoothed gate for output
    
    // Hold counters (samples remaining)
    int holdCounterL = 0, holdCounterR = 0;
    
    // Detection sidechain filters (per channel)
    juce::dsp::IIR::Filter<float> detHPFilterL, detHPFilterR;
    juce::dsp::IIR::Filter<float> detLPFilterL, detLPFilterR;
    
    // Cached filter frequencies to detect changes
    float lastHPFreq = 20.0f;
    float lastLPFreq = 20000.0f;
    
    void updateDetectionFilters();
    
    // One-pole coefficient from time constant in ms
    float msToCoeff(float ms) const {
        if (ms <= 0.0f) return 0.0f;
        return std::exp(-1.0f / (float)(currentSampleRate * ms * 0.001));
    }
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransientSplitterProcessor)
};
