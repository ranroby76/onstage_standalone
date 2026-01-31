#pragma once
#include <juce_dsp/juce_dsp.h>

class ExciterProcessor
{
public:
    struct Params
    {
        float frequency = 3000.0f; // High-pass cutoff (1000Hz - 10000Hz)
        float amount = 0.0f;       // Drive/Harmonics (0dB - 24dB)
        float mix = 0.0f;          // Mix amount (0.0 - 1.0)

        bool operator==(const Params& other) const {
            return frequency == other.frequency && 
                   amount == other.amount && 
                   mix == other.mix;
        }
        bool operator!=(const Params& other) const { return !(*this == other); }
    };

    ExciterProcessor();
    
    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();
    void process(juce::dsp::ProcessContextReplacing<float>& context);
    
    void setParams(const Params& newParams);
    Params getParams() const { return params; }
    
    void setBypassed(bool shouldBypass) { bypassed = shouldBypass; }
    bool isBypassed() const { return bypassed; }

private:
    void updateFilter();

    Params params;
    bool bypassed = false;
    double sampleRate = 44100.0;

    // High-pass filter to isolate the "Air" frequencies
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> highPassFilter;
    
    // Drive gain for saturation
    juce::dsp::Gain<float> driveGain;
    
    // Wet signal buffer
    juce::AudioBuffer<float> wetBuffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ExciterProcessor)
};