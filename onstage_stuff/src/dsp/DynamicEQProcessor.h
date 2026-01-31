#pragma once
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>

class DynamicEQProcessor
{
public:
    struct Params
    {
        float duckBandHz = 1000.0f;
        float q = 2.0f;
        float shape = 0.5f;
        float threshold = -30.0f;
        float ratio = 4.0f;
        float attack = 10.0f;
        float release = 150.0f;
    };

    DynamicEQProcessor() = default;

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        
        for (int ch = 0; ch < 2; ++ch)
        {
            duckFilter[ch].prepare(spec);
            duckFilter[ch].reset();
        }
        
        updateFilterCoefficients();
        
        envelopeLevel = 0.0f;
        attackCoeff = 0.0f;
        releaseCoeff = 0.0f;
        updateEnvelopeCoefficients();
    }

    void reset()
    {
        for (int ch = 0; ch < 2; ++ch)
            duckFilter[ch].reset();
        envelopeLevel = 0.0f;
    }

    void process(juce::AudioBuffer<float>& backingTracks, 
                 const juce::AudioBuffer<float>& vocalSidechain)
    {
        if (bypassed)
            return;

        const int numSamples = backingTracks.getNumSamples();
        const int numChannels = juce::jmin(2, backingTracks.getNumChannels());

        float vocalEnergy = 0.0f;
        for (int ch = 0; ch < vocalSidechain.getNumChannels(); ++ch)
        {
            const float* data = vocalSidechain.getReadPointer(ch);
            for (int i = 0; i < numSamples; ++i)
                vocalEnergy += data[i] * data[i];
        }
        vocalEnergy = std::sqrt(vocalEnergy / (numSamples * vocalSidechain.getNumChannels()));
        float vocalDb = juce::Decibels::gainToDecibels(vocalEnergy + 1e-6f);

        float gainReductionDb = 0.0f;
        if (vocalDb > params.threshold)
        {
            float overThresholdDb = vocalDb - params.threshold;
            gainReductionDb = overThresholdDb * (1.0f - (1.0f / params.ratio));
        }

        float targetGainReduction = juce::Decibels::decibelsToGain(-gainReductionDb);
        
        for (int i = 0; i < numSamples; ++i)
        {
            if (targetGainReduction < envelopeLevel)
                envelopeLevel += (targetGainReduction - envelopeLevel) * attackCoeff;
            else
                envelopeLevel += (targetGainReduction - envelopeLevel) * releaseCoeff;

            float freqGain = calculateFrequencyGain();
            float finalGain = 1.0f - ((1.0f - envelopeLevel) * freqGain);

            for (int ch = 0; ch < numChannels; ++ch)
            {
                float* channelData = backingTracks.getWritePointer(ch);
                channelData[i] *= finalGain;
            }
        }
    }

    void setParams(const Params& newParams)
    {
        params = newParams;
        updateFilterCoefficients();
        updateEnvelopeCoefficients();
    }

    Params getParams() const { return params; }
    void setBypassed(bool shouldBypass) { bypassed = shouldBypass; }
    bool isBypassed() const { return bypassed; }

private:
    void updateFilterCoefficients()
    {
        auto coeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
            sampleRate, params.duckBandHz, params.q, 1.0f);

        for (int ch = 0; ch < 2; ++ch)
            duckFilter[ch].coefficients = coeffs;
    }

    void updateEnvelopeCoefficients()
    {
        attackCoeff = 1.0f - std::exp(-1.0f / (params.attack * 0.001f * sampleRate));
        releaseCoeff = 1.0f - std::exp(-1.0f / (params.release * 0.001f * sampleRate));
    }

    float calculateFrequencyGain()
    {
        float shapeAmount = juce::jmap(params.shape, 0.0f, 1.0f, 0.3f, 1.0f);
        return shapeAmount;
    }

    Params params;
    bool bypassed = false;
    double sampleRate = 44100.0;

    juce::dsp::IIR::Filter<float> duckFilter[2];

    float envelopeLevel = 0.0f;
    float attackCoeff = 0.0f;
    float releaseCoeff = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DynamicEQProcessor)
};
