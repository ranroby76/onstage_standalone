#pragma once

#include <juce_dsp/juce_dsp.h>

class EQProcessor
{
public:
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
    
    void setParams(const Params& params);
    Params getParams() const;

    void setBypassed(bool shouldBeBypassed);
    bool isBypassed() const;
    
    void setLowFrequency(float freq);
    void setMidFrequency(float freq);
    void setHighFrequency(float freq);
    void setLowGain(float gain);
    void setMidGain(float gain);
    void setHighGain(float gain);
    void setLowQ(float q);
    void setMidQ(float q);
    void setHighQ(float q);
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
    float lowFreq, midFreq, highFreq;
    float lowGain, midGain, highGain;
    float lowQ, midQ, highQ;
    double sampleRate;
    bool bypassed;
    
    juce::dsp::IIR::Filter<float> lowShelf[2];
    juce::dsp::IIR::Filter<float> midPeak[2];
    juce::dsp::IIR::Filter<float> highShelf[2];
    
    void updateFilters();
};