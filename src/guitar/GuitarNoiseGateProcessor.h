// ==============================================================================
//  GuitarNoiseGateProcessor.h
//  OnStage - Guitar Noise Gate
//
//  High-gain guitar-optimized noise gate with fast tracking.
//
//  Parameters:
//  - Threshold: Gate open level (-80..0 dB)
//  - Attack: Gate open speed (0.1..20 ms)
//  - Hold: Hold open time (0..500 ms)
//  - Release: Gate close speed (5..500 ms)
//  - Range: Reduction when closed (0..-80 dB)
// ==============================================================================

#pragma once
#include <juce_dsp/juce_dsp.h>

class GuitarNoiseGateProcessor
{
public:
    struct Params
    {
        float thresholdDb = -50.0f;
        float attackMs    = 0.5f;
        float holdMs      = 30.0f;
        float releaseMs   = 50.0f;
        float rangeDb     = -80.0f;

        bool operator== (const Params& o) const
        { return thresholdDb == o.thresholdDb && attackMs == o.attackMs
              && holdMs == o.holdMs && releaseMs == o.releaseMs && rangeDb == o.rangeDb; }
        bool operator!= (const Params& o) const { return !(*this == o); }
    };

    GuitarNoiseGateProcessor() = default;

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        updateCoefficients();
        envelope = 0.0f;
        holdCounter = 0;
        gateGain = 0.0f;
        isPrepared = true;
    }

    void reset()
    {
        envelope = 0.0f;
        holdCounter = 0;
        gateGain = 0.0f;
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        if (bypassed || !isPrepared)
        {
            currentGainReductionDb = 0.0f;
            return;
        }

        const int numSamples  = buffer.getNumSamples();
        const int numChannels = juce::jmin (2, buffer.getNumChannels());
        float threshLin = juce::Decibels::decibelsToGain (params.thresholdDb);
        float rangeLin  = juce::Decibels::decibelsToGain (params.rangeDb);

        for (int i = 0; i < numSamples; ++i)
        {
            float inputLevel = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                inputLevel = juce::jmax (inputLevel, std::abs (buffer.getSample (ch, i)));

            if (inputLevel > envelope)
                envelope += attackCoeff * (inputLevel - envelope);
            else
                envelope += releaseCoeff * (inputLevel - envelope);

            float targetGain;
            if (envelope > threshLin)
            {
                targetGain = 1.0f;
                holdCounter = holdSamples;
            }
            else if (holdCounter > 0)
            {
                targetGain = 1.0f;
                --holdCounter;
            }
            else
            {
                targetGain = rangeLin;
            }

            if (targetGain > gateGain)
                gateGain += attackCoeff * (targetGain - gateGain);
            else
                gateGain += releaseCoeff * (targetGain - gateGain);

            for (int ch = 0; ch < numChannels; ++ch)
                buffer.getWritePointer (ch)[i] *= gateGain;
        }

        currentGainReductionDb = juce::Decibels::gainToDecibels (gateGain);
    }

    void setParams (const Params& p) { params = p; if (isPrepared) updateCoefficients(); }
    Params getParams() const { return params; }
    void setBypassed (bool b) { bypassed = b; }
    bool isBypassed() const { return bypassed; }
    float getCurrentGainReductionDb() const { return currentGainReductionDb; }
    float getGateState() const { return gateGain; }

private:
    void updateCoefficients()
    {
        if (sampleRate <= 0.0) return;
        attackCoeff  = 1.0f - std::exp (-1.0f / (float)(sampleRate * params.attackMs * 0.001f));
        releaseCoeff = 1.0f - std::exp (-1.0f / (float)(sampleRate * params.releaseMs * 0.001f));
        holdSamples  = (int)(params.holdMs * 0.001f * sampleRate);
    }

    Params params;
    double sampleRate = 44100.0;
    bool bypassed = false;
    bool isPrepared = false;

    float envelope = 0.0f;
    float gateGain = 0.0f;
    int holdCounter = 0;
    int holdSamples = 0;
    float attackCoeff = 0.0f;
    float releaseCoeff = 0.0f;
    float currentGainReductionDb = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GuitarNoiseGateProcessor)
};
