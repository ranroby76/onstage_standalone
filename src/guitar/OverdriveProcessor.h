// ==============================================================================
//  OverdriveProcessor.h
//  OnStage - Guitar Overdrive (Tube Screamer inspired)
//
//  Soft-clipping overdrive with asymmetric waveshaping for tube warmth.
//  Signal chain: Input Gain → HP filter → Waveshaper → Tone → Output Level
//
//  Parameters:
//  - Drive: Amount of overdrive (0 to 10)
//  - Tone: Brightness control (dark to bright)
//  - Level: Output volume (0.0 to 1.0)
//  - Mix: Dry/wet blend (0.0 to 1.0)
// ==============================================================================

#pragma once
#include <juce_dsp/juce_dsp.h>

class OverdriveProcessor
{
public:
    struct Params
    {
        float drive = 5.0f;       // 0..10
        float tone  = 0.5f;       // 0..1 (dark to bright)
        float level = 0.5f;       // 0..1
        float mix   = 1.0f;       // 0..1 dry/wet

        bool operator== (const Params& o) const
        {
            return drive == o.drive && tone == o.tone
                && level == o.level && mix == o.mix;
        }
        bool operator!= (const Params& o) const { return !(*this == o); }
    };

    OverdriveProcessor() = default;

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;

        // Input high-pass to remove DC and sub-bass rumble
        for (int ch = 0; ch < 2; ++ch)
        {
            inputHP[ch].prepare (spec);
            toneFilter[ch].prepare (spec);
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
        }
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        if (bypassed || !isPrepared) return;

        const int numSamples  = buffer.getNumSamples();
        const int numChannels = juce::jmin (2, buffer.getNumChannels());

        const float driveGain = 1.0f + params.drive * 4.0f;   // 1x to 41x
        const float comp = 1.0f / (1.0f + params.drive * 0.3f); // compensation
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

                // High-pass to remove DC
                x = inputHP[ch].processSample (x);

                // Apply drive gain
                x *= driveGain;

                // Asymmetric soft clipping (tube-like)
                // Positive half: tanh (smooth)
                // Negative half: slightly harder clip (adds even harmonics)
                if (x >= 0.0f)
                    x = std::tanh (x);
                else
                    x = std::tanh (x * 1.2f) * 0.9f;

                // Compensation gain
                x *= comp;

                // Tone filter (variable LP)
                x = toneFilter[ch].processSample (x);

                // Output gain + dry/wet mix
                data[i] = (x * outGain * wet) + (in * dry);
            }
        }
    }

    void setParams (const Params& p)
    {
        params = p;
        if (isPrepared) applyParams();
    }

    Params getParams() const { return params; }
    void setBypassed (bool b) { bypassed = b; }
    bool isBypassed() const { return bypassed; }

private:
    void applyParams()
    {
        if (sampleRate <= 0.0) return;

        // Input HP at 720 Hz (Tube Screamer voicing - removes low end before clipping)
        auto hpCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, 720.0f, 0.707f);
        for (int ch = 0; ch < 2; ++ch)
            inputHP[ch].coefficients = hpCoeffs;

        // Tone: LP sweep from 800 Hz (dark) to 8 kHz (bright)
        float toneFreq = 800.0f * std::pow (10.0f, params.tone);
        auto toneCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass (sampleRate, toneFreq, 0.707f);
        for (int ch = 0; ch < 2; ++ch)
            toneFilter[ch].coefficients = toneCoeffs;
    }

    Params params;
    double sampleRate = 44100.0;
    bool bypassed = false;
    bool isPrepared = false;

    juce::dsp::IIR::Filter<float> inputHP[2];
    juce::dsp::IIR::Filter<float> toneFilter[2];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OverdriveProcessor)
};
