// ==============================================================================
//  DistortionProcessor.h
//  OnStage - Guitar Distortion (Boss DS-1 / RAT inspired)
//
//  Hard-clipping distortion with pre/post filtering.
//  Signal chain: HP → Drive → Hard Clip → Tone → Level
//
//  Parameters:
//  - Drive: Distortion amount (0 to 10)
//  - Tone: Brightness (0..1)
//  - Level: Output volume (0..1)
//  - Mix: Dry/wet (0..1)
// ==============================================================================

#pragma once
#include <juce_dsp/juce_dsp.h>

class DistortionProcessor
{
public:
    struct Params
    {
        float drive = 5.0f;
        float tone  = 0.5f;
        float level = 0.5f;
        float mix   = 1.0f;

        bool operator== (const Params& o) const
        { return drive == o.drive && tone == o.tone && level == o.level && mix == o.mix; }
        bool operator!= (const Params& o) const { return !(*this == o); }
    };

    DistortionProcessor() = default;

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        for (int ch = 0; ch < 2; ++ch)
        {
            inputHP[ch].prepare (spec);
            toneFilter[ch].prepare (spec);
            outputLP[ch].prepare (spec);
        }
        applyParams();
        isPrepared = true;
    }

    void reset()
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            inputHP[ch].reset();
            toneFilter[ch].reset();
            outputLP[ch].reset();
        }
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        if (bypassed || !isPrepared) return;

        const int numSamples  = buffer.getNumSamples();
        const int numChannels = juce::jmin (2, buffer.getNumChannels());

        const float driveGain = 1.0f + params.drive * 8.0f;
        const float comp = 1.0f / (1.0f + params.drive * 0.4f);
        const float outGain = params.level;
        const float wet = params.mix;
        const float dry = 1.0f - params.mix;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* data = buffer.getWritePointer (ch);
            for (int i = 0; i < numSamples; ++i)
            {
                float in = data[i];
                float x = in;

                x = inputHP[ch].processSample (x);
                x *= driveGain;

                // Hard clipping with slight knee
                x = juce::jlimit (-1.0f, 1.0f, x);
                // Add a tiny bit of cubic shaping for texture
                x = x - (x * x * x) * 0.166f;

                x *= comp;
                x = toneFilter[ch].processSample (x);
                x = outputLP[ch].processSample (x);

                data[i] = (x * outGain * wet) + (in * dry);
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

        auto hpCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, 100.0f, 0.707f);
        float toneFreq = 600.0f * std::pow (10.0f, params.tone);
        auto toneCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass (sampleRate, toneFreq, 0.707f);
        auto lpCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass (sampleRate, 12000.0f, 0.707f);

        for (int ch = 0; ch < 2; ++ch)
        {
            inputHP[ch].coefficients = hpCoeffs;
            toneFilter[ch].coefficients = toneCoeffs;
            outputLP[ch].coefficients = lpCoeffs;
        }
    }

    Params params;
    double sampleRate = 44100.0;
    bool bypassed = false;
    bool isPrepared = false;

    juce::dsp::IIR::Filter<float> inputHP[2];
    juce::dsp::IIR::Filter<float> toneFilter[2];
    juce::dsp::IIR::Filter<float> outputLP[2];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DistortionProcessor)
};
