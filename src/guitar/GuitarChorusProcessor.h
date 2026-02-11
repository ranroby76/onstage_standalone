// ==============================================================================
//  GuitarChorusProcessor.h
//  OnStage - Guitar Chorus (Boss CE-2 inspired)
//
//  LFO-modulated delay line chorus with stereo spread.
//  Signal chain: Input → Modulated Delay → Mix with dry → Output
//
//  Parameters:
//  - Rate: LFO speed in Hz (0.1..10)
//  - Depth: Modulation depth (0..1)
//  - Mix: Dry/wet (0..1)
//  - Width: Stereo spread (0..1)
// ==============================================================================

#pragma once
#include <juce_dsp/juce_dsp.h>

class GuitarChorusProcessor
{
public:
    struct Params
    {
        float rate  = 1.0f;     // Hz
        float depth = 0.5f;     // 0..1
        float mix   = 0.5f;     // 0..1
        float width = 0.7f;     // 0..1 stereo spread

        bool operator== (const Params& o) const
        { return rate == o.rate && depth == o.depth && mix == o.mix && width == o.width; }
        bool operator!= (const Params& o) const { return !(*this == o); }
    };

    GuitarChorusProcessor() = default;

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;

        // Max delay: ~25 ms for deep chorus
        int maxDelay = (int)(0.025 * sampleRate) + 1;
        for (int ch = 0; ch < 2; ++ch)
        {
            delayLine[ch].reset (new juce::dsp::DelayLine<float,
                juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> (maxDelay));
            delayLine[ch]->prepare (spec);
        }

        lfoPhase = 0.0f;
        applyParams();
        isPrepared = true;
    }

    void reset()
    {
        for (int ch = 0; ch < 2; ++ch)
            if (delayLine[ch]) delayLine[ch]->reset();
        lfoPhase = 0.0f;
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        if (bypassed || !isPrepared) return;

        const int numSamples  = buffer.getNumSamples();
        const int numChannels = juce::jmin (2, buffer.getNumChannels());
        const float lfoInc = params.rate / (float)sampleRate;

        // Base delay 7ms, modulation range 0..5ms
        const float baseDelayMs  = 7.0f;
        const float modRangeMs   = 5.0f * params.depth;
        const float wet = params.mix;
        const float dry = 1.0f - params.mix;

        for (int i = 0; i < numSamples; ++i)
        {
            // LFO (sine)
            float lfoL = std::sin (lfoPhase * juce::MathConstants<float>::twoPi);
            // Right channel offset by 90° for stereo width
            float lfoR = std::sin ((lfoPhase + 0.25f * params.width)
                                   * juce::MathConstants<float>::twoPi);

            float delayMsL = baseDelayMs + modRangeMs * lfoL;
            float delayMsR = baseDelayMs + modRangeMs * lfoR;
            float delaySamplesL = delayMsL * (float)sampleRate / 1000.0f;
            float delaySamplesR = delayMsR * (float)sampleRate / 1000.0f;

            for (int ch = 0; ch < numChannels; ++ch)
            {
                float* data = buffer.getWritePointer (ch);
                float in = data[i];
                float delaySamples = (ch == 0) ? delaySamplesL : delaySamplesR;

                float delayed = delayLine[ch]->popSample (0, delaySamples);
                delayLine[ch]->pushSample (0, in);

                data[i] = in * dry + delayed * wet;
            }

            lfoPhase += lfoInc;
            if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
        }
    }

    void setParams (const Params& p) { params = p; if (isPrepared) applyParams(); }
    Params getParams() const { return params; }
    void setBypassed (bool b) { bypassed = b; }
    bool isBypassed() const { return bypassed; }

private:
    void applyParams() {}

    Params params;
    double sampleRate = 44100.0;
    bool bypassed = false;
    bool isPrepared = false;

    std::unique_ptr<juce::dsp::DelayLine<float,
        juce::dsp::DelayLineInterpolationTypes::Lagrange3rd>> delayLine[2];
    float lfoPhase = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GuitarChorusProcessor)
};
