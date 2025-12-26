#pragma once
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>

class ReverbProcessor
{
public:
    struct Params {
        float wetGain = 0.5f; float lowCutHz = 20.0f; float highCutHz = 20000.0f; juce::String irFilePath = "";
        bool operator==(const Params& other) const { return wetGain == other.wetGain && lowCutHz == other.lowCutHz && highCutHz == other.highCutHz && irFilePath == other.irFilePath; }
        bool operator!=(const Params& other) const { return !(*this == other); }
    };

    ReverbProcessor();
    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();
    void process(juce::AudioBuffer<float>& buffer);
    void setParams(const Params& newParams);
    Params getParams() const { return params; }
    void setBypassed(bool shouldBypass) { bypassed = shouldBypass; }
    bool isBypassed() const { return bypassed; }
    juce::String getCurrentIrName() const;

private:
    void updateFilters(); void loadEmbeddedIR(); void loadExternalIR(const juce::File& file);
    Params params; bool bypassed = false; double sampleRate = 44100.0; juce::String currentIrName = "Default (Internal)";
    juce::dsp::Convolution convolution;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> lowCutFilter, highCutFilter;
    juce::AudioBuffer<float> dryBuffer;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReverbProcessor)
};