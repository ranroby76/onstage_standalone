// ==============================================================================
//  FuzzProcessor.h
//  OnStage - Guitar Fuzz (Big Muff / Fuzz Face inspired)
//
//  Extreme clipping with octave-up harmonics and thick sustain.
//  Signal chain: Boost → Asymmetric Clip → Sustain → Tone → Level
//
//  Parameters:
//  - Fuzz: Intensity (0..10)
//  - Tone: Dark to bright (0..1)
//  - Sustain: Sustain amount via compression before clip (0..1)
//  - Level: Output (0..1)
// ==============================================================================

#pragma once
#include <juce_dsp/juce_dsp.h>

class FuzzProcessor
{
public:
    struct Params
    {
        float fuzz    = 7.0f;    // 0..10
        float tone    = 0.5f;    // 0..1
        float sustain = 0.6f;    // 0..1
        float level   = 0.4f;    // 0..1

        bool operator== (const Params& o) const
        { return fuzz == o.fuzz && tone == o.tone && sustain == o.sustain && level == o.level; }
        bool operator!= (const Params& o) const { return !(*this == o); }
    };

    FuzzProcessor() = default;

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        for (int ch = 0; ch < 2; ++ch)
        {
            inputHP[ch].prepare (spec);
            toneLP[ch].prepare (spec);
            toneHP[ch].prepare (spec);
        }
        applyParams();
        isPrepared = true;
    }

    void reset()
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            inputHP[ch].reset();
            toneLP[ch].reset();
            toneHP[ch].reset();
        }
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        if (bypassed || !isPrepared) return;

        const int numSamples  = buffer.getNumSamples();
        const int numChannels = juce::jmin (2, buffer.getNumChannels());

        const float fuzzGain = 1.0f + params.fuzz * 12.0f;
        const float comp = 1.0f / (1.0f + params.fuzz * 0.5f);
        const float sustainAmt = params.sustain;
        const float outGain = params.level;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* data = buffer.getWritePointer (ch);
            for (int i = 0; i < numSamples; ++i)
            {
                float x = data[i];
                x = inputHP[ch].processSample (x);

                // Pre-compression for sustain
                if (sustainAmt > 0.01f)
                {
                    float absX = std::abs (x);
                    float envelope = absX > 0.001f ? 1.0f / (1.0f + absX * sustainAmt * 10.0f) : 1.0f;
                    x *= (1.0f + sustainAmt * 5.0f) * envelope;
                }

                x *= fuzzGain;

                // Extreme asymmetric fuzz clipping
                // Positive: squared then clipped (octave-up harmonics)
                // Negative: hard diode-style clip
                if (x > 0.0f)
                {
                    x = x * x;  // rectification for octave-up
                    x = juce::jmin (x, 1.0f);
                }
                else
                {
                    x = juce::jmax (x, -0.6f);  // asymmetric floor
                    x *= 1.5f;
                }

                x *= comp;

                // Big Muff style tone: blend LP and HP
                float lp = toneLP[ch].processSample (x);
                float hp = toneHP[ch].processSample (x);
                x = lp * (1.0f - params.tone) + hp * params.tone;

                data[i] = x * outGain;
            }
        }
    }

    void setParams (const Params& p) { params = p; if (isPrepared) applyParams(); }
    Params getParams() const { return params; }
    void setBypassed (bool b) { bypassed = b; }
    bool isBypassed() const { return bypassed; }

private:
    void applyParams()
    {
        if (sampleRate <= 0.0) return;

        auto hpCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, 80.0f, 0.707f);
        auto lpCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass (sampleRate, 1000.0f, 0.5f);
        auto hpToneCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, 1000.0f, 0.5f);

        for (int ch = 0; ch < 2; ++ch)
        {
            inputHP[ch].coefficients = hpCoeffs;
            toneLP[ch].coefficients = lpCoeffs;
            toneHP[ch].coefficients = hpToneCoeffs;
        }
    }

    Params params;
    double sampleRate = 44100.0;
    bool bypassed = false;
    bool isPrepared = false;

    juce::dsp::IIR::Filter<float> inputHP[2];
    juce::dsp::IIR::Filter<float> toneLP[2];
    juce::dsp::IIR::Filter<float> toneHP[2];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FuzzProcessor)
};
