// ==============================================================================
//  GuitarFlangerProcessor.h
//  OnStage - Guitar Flanger (Boss BF-2 inspired)
//
//  Short modulated delay with feedback for jet/metallic sweep.
//  Signal chain: Input → Short Delay (LFO mod) + Feedback → Mix → Output
//
//  Parameters:
//  - Rate: LFO speed (0.05..5 Hz)
//  - Depth: Modulation depth (0..1)
//  - Feedback: Resonance / intensity (0..0.95)
//  - Mix: Dry/wet (0..1)
// ==============================================================================

#pragma once
#include <juce_dsp/juce_dsp.h>

class GuitarFlangerProcessor
{
public:
    struct Params
    {
        float rate     = 0.3f;   // Hz
        float depth    = 0.7f;   // 0..1
        float feedback = 0.5f;   // 0..0.95
        float mix      = 0.5f;   // 0..1

        bool operator== (const Params& o) const
        { return rate == o.rate && depth == o.depth && feedback == o.feedback && mix == o.mix; }
        bool operator!= (const Params& o) const { return !(*this == o); }
    };

    GuitarFlangerProcessor() = default;

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;

        // Max delay ~10ms for flanging
        int maxDelay = (int)(0.010 * sampleRate) + 1;
        for (int ch = 0; ch < 2; ++ch)
        {
            delayLine[ch].reset (new juce::dsp::DelayLine<float,
                juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> (maxDelay));
            delayLine[ch]->prepare (spec);
        }

        lfoPhase = 0.0f;
        feedbackState[0] = feedbackState[1] = 0.0f;
        isPrepared = true;
    }

    void reset()
    {
        for (int ch = 0; ch < 2; ++ch)
            if (delayLine[ch]) delayLine[ch]->reset();
        lfoPhase = 0.0f;
        feedbackState[0] = feedbackState[1] = 0.0f;
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        if (bypassed || !isPrepared) return;

        const int numSamples  = buffer.getNumSamples();
        const int numChannels = juce::jmin (2, buffer.getNumChannels());
        const float lfoInc = params.rate / (float)sampleRate;
        const float fb = juce::jlimit (0.0f, 0.95f, params.feedback);

        // Flanger: 0.5ms to 7ms sweep
        const float minDelayMs = 0.5f;
        const float maxDelayMs = 7.0f;
        const float wet = params.mix;
        const float dry = 1.0f - params.mix;

        for (int i = 0; i < numSamples; ++i)
        {
            // Triangle LFO for smoother flanging sweep
            float lfoVal = 2.0f * std::abs (2.0f * (lfoPhase - std::floor (lfoPhase + 0.5f))) - 1.0f;

            float delayMs = minDelayMs + (maxDelayMs - minDelayMs) * 0.5f * (1.0f + lfoVal * params.depth);
            float delaySamples = delayMs * (float)sampleRate / 1000.0f;

            for (int ch = 0; ch < numChannels; ++ch)
            {
                float* data = buffer.getWritePointer (ch);
                float in = data[i];

                float delayed = delayLine[ch]->popSample (0, delaySamples);
                delayLine[ch]->pushSample (0, in + delayed * fb);

                feedbackState[ch] = delayed;
                data[i] = in * dry + delayed * wet;
            }

            lfoPhase += lfoInc;
            if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
        }
    }

    void setParams (const Params& p) { params = p; }
    Params getParams() const { return params; }
    void setBypassed (bool b) { bypassed = b; }
    bool isBypassed() const { return bypassed; }

private:
    Params params;
    double sampleRate = 44100.0;
    bool bypassed = false;
    bool isPrepared = false;

    std::unique_ptr<juce::dsp::DelayLine<float,
        juce::dsp::DelayLineInterpolationTypes::Lagrange3rd>> delayLine[2];
    float lfoPhase = 0.0f;
    float feedbackState[2] = {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GuitarFlangerProcessor)
};
