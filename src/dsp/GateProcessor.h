// ==============================================================================
//  GateProcessor.h
//  OnStage - Noise Gate DSP for clean vocals
//
//  Parameters:
//  - Threshold: Level below which gate closes (-80 to 0 dB)
//  - Attack: How fast gate opens (0.1 to 50 ms)
//  - Hold: Time to keep gate open after signal drops (0 to 500 ms)
//  - Release: How fast gate closes (10 to 1000 ms)
//  - Range: Amount of reduction when closed (0 to -80 dB)
// ==============================================================================

#pragma once
#include <juce_dsp/juce_dsp.h>

class GateProcessor
{
public:
    struct Params
    {
        float thresholdDb = -40.0f;   // Gate opens above this level
        float attackMs    = 1.0f;     // Fast attack for vocals
        float holdMs      = 50.0f;    // Hold time before release
        float releaseMs   = 100.0f;   // Smooth release
        float rangeDb     = -80.0f;   // Full closure by default
    };

    GateProcessor() = default;

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        updateCoefficients();
        
        envelope = 0.0f;
        holdCounter = 0;
        gateGain = 0.0f;
        
        // RMS detection filter
        for (int ch = 0; ch < 2; ++ch)
        {
            rmsFilters[ch].prepare(spec);
            rmsFilters[ch].coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, 50.0f);
        }
    }

    void reset()
    {
        envelope = 0.0f;
        holdCounter = 0;
        gateGain = 0.0f;
        for (int ch = 0; ch < 2; ++ch)
            rmsFilters[ch].reset();
    }

    void process(juce::AudioBuffer<float>& buffer)
    {
        if (bypassed)
        {
            currentGainReductionDb = 0.0f;
            return;
        }

        const int numSamples = buffer.getNumSamples();
        const int numChannels = juce::jmin(2, buffer.getNumChannels());

        float thresholdLinear = juce::Decibels::decibelsToGain(params.thresholdDb);
        float rangeLinear = juce::Decibels::decibelsToGain(params.rangeDb);

        for (int i = 0; i < numSamples; ++i)
        {
            // Detect input level (peak across channels)
            float inputLevel = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
            {
                float sample = buffer.getSample(ch, i);
                inputLevel = juce::jmax(inputLevel, std::abs(sample));
            }

            // Smooth envelope follower
            if (inputLevel > envelope)
                envelope += attackCoeff * (inputLevel - envelope);
            else
                envelope += releaseCoeff * (inputLevel - envelope);

            // Gate state machine
            float targetGain;
            if (envelope > thresholdLinear)
            {
                // Gate open
                targetGain = 1.0f;
                holdCounter = holdSamples;
            }
            else if (holdCounter > 0)
            {
                // Hold phase
                targetGain = 1.0f;
                holdCounter--;
            }
            else
            {
                // Gate closed
                targetGain = rangeLinear;
            }

            // Smooth gain transition
            if (targetGain > gateGain)
                gateGain += attackCoeff * (targetGain - gateGain);
            else
                gateGain += releaseCoeff * (targetGain - gateGain);

            // Apply gain
            for (int ch = 0; ch < numChannels; ++ch)
            {
                float* data = buffer.getWritePointer(ch);
                data[i] *= gateGain;
            }
        }

        // Store current reduction for metering
        currentGainReductionDb = juce::Decibels::gainToDecibels(gateGain);
    }

    void setParams(const Params& p)
    {
        params = p;
        updateCoefficients();
    }

    Params getParams() const { return params; }

    void setBypassed(bool b) { bypassed = b; }
    bool isBypassed() const { return bypassed; }

    float getCurrentGainReductionDb() const { return currentGainReductionDb; }
    float getGateState() const { return gateGain; }  // 0 = closed, 1 = open

private:
    void updateCoefficients()
    {
        if (sampleRate <= 0.0) return;

        // Time constants
        float attackSec = params.attackMs / 1000.0f;
        float releaseSec = params.releaseMs / 1000.0f;

        attackCoeff = 1.0f - std::exp(-1.0f / (float)(sampleRate * attackSec));
        releaseCoeff = 1.0f - std::exp(-1.0f / (float)(sampleRate * releaseSec));

        holdSamples = (int)(params.holdMs / 1000.0f * sampleRate);
    }

    Params params;
    double sampleRate = 44100.0;
    bool bypassed = false;

    float envelope = 0.0f;
    float gateGain = 0.0f;
    int holdCounter = 0;
    int holdSamples = 0;

    float attackCoeff = 0.0f;
    float releaseCoeff = 0.0f;

    float currentGainReductionDb = 0.0f;

    juce::dsp::IIR::Filter<float> rmsFilters[2];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GateProcessor)
};
