#pragma once

#include <juce_dsp/juce_dsp.h>

// ============================================================================
// EQProcessor - 3-band EQ with Low/Mid/High controls
// ============================================================================
class EQProcessor
{
public:
    // NEW: Params struct for PresetManager
    struct Params
    {
        float lowFreq = 100.0f;
        float midFreq = 1000.0f;
        float highFreq = 10000.0f;
        
        float lowGain = 0.0f;
        float midGain = 0.0f;
        float highGain = 0.0f;
        
        float lowQ = 0.707f;
        float midQ = 0.707f;
        float highQ = 0.707f;

        bool operator==(const Params& other) const
        {
            return lowFreq == other.lowFreq && midFreq == other.midFreq && highFreq == other.highFreq &&
                   lowGain == other.lowGain && midGain == other.midGain && highGain == other.highGain &&
                   lowQ == other.lowQ && midQ == other.midQ && highQ == other.highQ;
        }
        bool operator!=(const Params& other) const { return !(*this == other); }
    };

    EQProcessor();
    void prepare(const juce::dsp::ProcessSpec& spec);
    void process(juce::dsp::ProcessContextReplacing<float>& context);
    void reset();
    
    // Param management (This was missing in the cpp)
    void setParams(const Params& params);
    Params getParams() const;

    // Bypass control
    void setBypassed(bool shouldBeBypassed);
    bool isBypassed() const;
    
    // Frequency setters
    void setLowFrequency(float freq);
    void setMidFrequency(float freq);
    void setHighFrequency(float freq);
    // Gain setters (in dB)
    void setLowGain(float gain);
    void setMidGain(float gain);
    void setHighGain(float gain);
    // Q factor setters
    void setLowQ(float q);
    void setMidQ(float q);
    void setHighQ(float q);
    // Getters
    float getLowFrequency() const;
    float getMidFrequency() const;
    float getHighFrequency() const;
    float getLowGain() const;
    float getMidGain() const;
    float getHighGain() const;
    float getLowQ() const;
    float getMidQ() const;
    float getHighQ() const;

private:
    // Filter parameters
    float lowFreq, midFreq, highFreq;
    float lowGain, midGain, highGain;
    float lowQ, midQ, highQ;
    double sampleRate;
    bool bypassed;
    
    // Filters
    juce::dsp::IIR::Filter<float> lowShelf[2];
    juce::dsp::IIR::Filter<float> midPeak[2];
    juce::dsp::IIR::Filter<float> highShelf[2];
    
    void updateFilters();
};