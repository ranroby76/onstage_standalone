/*
  ==============================================================================

    SculptProcessor.h
    OnStage

    A "Smart" Vocal Channel Strip.
    Combines Saturation (Tube/Tape/Hybrid), Dynamic Resonance Suppression, and Air.

  ==============================================================================
*/

#pragma once
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>

class SculptProcessor
{
public:
    enum class SaturationMode
    {
        Tube = 0,   // Odd harmonics - bright, present
        Tape = 1,   // Even harmonics - warm, smooth
        Hybrid = 2  // Balanced blend
    };

    struct Params
    {
        float drive = 0.0f;     // 0-100% Saturation
        float mudCut = 0.0f;    // 0-100% Low-Mid suppression
        float harshCut = 0.0f;  // 0-100% High-Mid suppression
        float air = 0.0f;       // 0-10dB High Shelf Boost
        SaturationMode mode = SaturationMode::Hybrid;  // Saturation mode
        
        bool operator==(const Params& other) const {
            return drive == other.drive && mudCut == other.mudCut && 
                   harshCut == other.harshCut && air == other.air && mode == other.mode;
        }
        bool operator!=(const Params& other) const { return !(*this == other); }
    };

    SculptProcessor();
    
    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();
    void process(juce::dsp::ProcessContextReplacing<float>& context);
    
    void setParams(const Params& newParams);
    Params getParams() const { return params; }
    
    void setBypassed(bool shouldBypass) { bypassed = shouldBypass; }
    bool isBypassed() const { return bypassed; }

private:
    void updateFilters();

    Params params;
    bool bypassed = false;
    double sampleRate = 44100.0;

    // --- DSP BLOCKS ---
    
    // 1. Saturation (Waveshaper) - removed, now using inline processing
    
    // 2. Filters (Using ProcessorDuplicator for safe state access)
    using FilterType = juce::dsp::IIR::Filter<float>;
    using CoeffType = juce::dsp::IIR::Coefficients<float>;

    juce::dsp::ProcessorDuplicator<FilterType, CoeffType> mudFilter;
    juce::dsp::ProcessorDuplicator<FilterType, CoeffType> harshFilter;
    juce::dsp::ProcessorDuplicator<FilterType, CoeffType> airFilter;

    // Constants
    const float MUD_FREQ = 300.0f;
    const float HARSH_FREQ = 3500.0f;
    const float AIR_FREQ = 12000.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SculptProcessor)
};