// ==============================================================================
//  ToneStackProcessor.h
//  OnStage - Guitar Tone Stack
//
//  Classic amp tone stack emulation using cascaded biquad filters.
//  Models: Fender Bassman, Marshall JCM800, Baxandall (flat response)
//
//  Parameters:
//  - Model: 0=Fender, 1=Marshall, 2=Baxandall
//  - Bass: Low frequency control (0..1, 0.5 = flat)
//  - Mid: Midrange control (0..1, 0.5 = flat)
//  - Treble: High frequency control (0..1, 0.5 = flat)
//  - Gain: Post tone stack gain (0..2)
// ==============================================================================

#pragma once
#include <juce_dsp/juce_dsp.h>

class ToneStackProcessor
{
public:
    enum Model { Fender = 0, Marshall, Baxandall };

    struct Params
    {
        int   model  = Fender;
        float bass   = 0.5f;   // 0..1
        float mid    = 0.5f;   // 0..1
        float treble = 0.5f;   // 0..1
        float gain   = 1.0f;   // 0..2

        bool operator== (const Params& o) const
        { return model == o.model && bass == o.bass && mid == o.mid
              && treble == o.treble && gain == o.gain; }
        bool operator!= (const Params& o) const { return !(*this == o); }
    };

    ToneStackProcessor() = default;

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        for (int ch = 0; ch < 2; ++ch)
        {
            bassFilter[ch].prepare (spec);
            midFilter[ch].prepare (spec);
            trebleFilter[ch].prepare (spec);
        }
        applyParams();
        isPrepared = true;
    }

    void reset()
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            bassFilter[ch].reset();
            midFilter[ch].reset();
            trebleFilter[ch].reset();
        }
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        if (bypassed || !isPrepared) return;

        const int numSamples  = buffer.getNumSamples();
        const int numChannels = juce::jmin (2, buffer.getNumChannels());

        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* data = buffer.getWritePointer (ch);
            for (int i = 0; i < numSamples; ++i)
            {
                float x = data[i];
                x = bassFilter[ch].processSample (x);
                x = midFilter[ch].processSample (x);
                x = trebleFilter[ch].processSample (x);
                data[i] = x * params.gain;
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

        // Model-specific center frequencies and Q values
        float bassFreq, midFreq, trebleFreq;
        float bassQ, midQ, trebleQ;

        switch (params.model)
        {
            case Marshall:
                bassFreq = 100.0f;   bassQ = 0.8f;
                midFreq  = 800.0f;   midQ  = 1.2f;
                trebleFreq = 3200.0f; trebleQ = 0.7f;
                break;
            case Baxandall:
                bassFreq = 150.0f;   bassQ = 0.5f;
                midFreq  = 1000.0f;  midQ  = 0.5f;
                trebleFreq = 4000.0f; trebleQ = 0.5f;
                break;
            default: // Fender
                bassFreq = 80.0f;    bassQ = 0.6f;
                midFreq  = 650.0f;   midQ  = 1.5f;
                trebleFreq = 2500.0f; trebleQ = 0.7f;
                break;
        }

        // Convert 0..1 knob to -12..+12 dB
        float bassDb   = (params.bass   - 0.5f) * 24.0f;
        float midDb    = (params.mid    - 0.5f) * 24.0f;
        float trebleDb = (params.treble - 0.5f) * 24.0f;

        float bassGain   = juce::Decibels::decibelsToGain (bassDb);
        float midGain    = juce::Decibels::decibelsToGain (midDb);
        float trebleGain = juce::Decibels::decibelsToGain (trebleDb);

        auto bassCoeffs   = juce::dsp::IIR::Coefficients<float>::makeLowShelf (sampleRate, bassFreq, bassQ, bassGain);
        auto midCoeffs    = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sampleRate, midFreq, midQ, midGain);
        auto trebleCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighShelf (sampleRate, trebleFreq, trebleQ, trebleGain);

        for (int ch = 0; ch < 2; ++ch)
        {
            bassFilter[ch].coefficients = bassCoeffs;
            midFilter[ch].coefficients = midCoeffs;
            trebleFilter[ch].coefficients = trebleCoeffs;
        }
    }

    Params params;
    double sampleRate = 44100.0;
    bool bypassed = false;
    bool isPrepared = false;

    juce::dsp::IIR::Filter<float> bassFilter[2];
    juce::dsp::IIR::Filter<float> midFilter[2];
    juce::dsp::IIR::Filter<float> trebleFilter[2];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ToneStackProcessor)
};
