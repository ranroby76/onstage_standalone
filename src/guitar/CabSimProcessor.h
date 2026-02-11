// ==============================================================================
//  CabSimProcessor.h
//  OnStage - Guitar Cabinet Simulator
//
//  EQ-based speaker cabinet and microphone emulation.
//  Simulates the frequency response of common guitar cab/mic combos
//  without needing impulse response files.
//
//  Parameters:
//  - Cabinet: 0=1x12 Open, 1=2x12 Closed, 2=4x12 Closed, 3=Direct (flat)
//  - Mic: 0=SM57 (presence peak), 1=MD421 (scooped), 2=Ribbon (dark)
//  - MicPos: Distance 0..1 (close→far, changes brightness)
//  - Level: Output gain (0..2)
// ==============================================================================

#pragma once
#include <juce_dsp/juce_dsp.h>

class CabSimProcessor
{
public:
    enum Cabinet { Open1x12 = 0, Closed2x12, Closed4x12, Direct };
    enum Mic     { SM57 = 0, MD421, Ribbon };

    struct Params
    {
        int   cabinet = Open1x12;
        int   mic     = SM57;
        float micPos  = 0.3f;   // 0..1
        float level   = 1.0f;   // 0..2

        bool operator== (const Params& o) const
        { return cabinet == o.cabinet && mic == o.mic && micPos == o.micPos && level == o.level; }
        bool operator!= (const Params& o) const { return !(*this == o); }
    };

    CabSimProcessor() = default;

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        for (int ch = 0; ch < 2; ++ch)
        {
            hpFilter[ch].prepare (spec);
            lpFilter[ch].prepare (spec);
            presenceFilter[ch].prepare (spec);
            bodyFilter[ch].prepare (spec);
        }
        applyParams();
        isPrepared = true;
    }

    void reset()
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            hpFilter[ch].reset();
            lpFilter[ch].reset();
            presenceFilter[ch].reset();
            bodyFilter[ch].reset();
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
                x = hpFilter[ch].processSample (x);
                x = lpFilter[ch].processSample (x);
                x = presenceFilter[ch].processSample (x);
                x = bodyFilter[ch].processSample (x);
                data[i] = x * params.level;
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

        // --- Cabinet response ---
        float hpFreq, lpFreq, bodyFreq, bodyGainDb, bodyQ;

        switch (params.cabinet)
        {
            case Closed2x12:
                hpFreq = 80.0f;  lpFreq = 5500.0f;
                bodyFreq = 350.0f; bodyGainDb = 3.0f; bodyQ = 1.0f;
                break;
            case Closed4x12:
                hpFreq = 70.0f;  lpFreq = 5000.0f;
                bodyFreq = 250.0f; bodyGainDb = 4.0f; bodyQ = 0.8f;
                break;
            case Direct:
                hpFreq = 20.0f;  lpFreq = 20000.0f;
                bodyFreq = 1000.0f; bodyGainDb = 0.0f; bodyQ = 0.7f;
                break;
            default: // Open1x12
                hpFreq = 100.0f; lpFreq = 6000.0f;
                bodyFreq = 400.0f; bodyGainDb = 2.0f; bodyQ = 1.2f;
                break;
        }

        // --- Mic response ---
        float presFreq, presGainDb, presQ;

        switch (params.mic)
        {
            case MD421:
                presFreq = 2000.0f; presGainDb = -2.0f; presQ = 1.5f; // scooped
                break;
            case Ribbon:
                presFreq = 3500.0f; presGainDb = -5.0f; presQ = 0.8f; // dark rolloff
                lpFreq *= 0.7f;  // darker top end
                break;
            default: // SM57
                presFreq = 3500.0f; presGainDb = 4.0f; presQ = 1.5f;  // presence peak
                break;
        }

        // --- Mic position modifies brightness ---
        float posBlend = 1.0f - params.micPos;  // close = bright, far = dark
        lpFreq *= (0.7f + 0.3f * posBlend);
        presGainDb *= posBlend;

        // Apply coefficients
        auto hpCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, hpFreq, 0.707f);
        auto lpCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass (sampleRate, lpFreq, 0.707f);
        auto presCoeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter (
            sampleRate, presFreq, presQ, juce::Decibels::decibelsToGain (presGainDb));
        auto bodyCoeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter (
            sampleRate, bodyFreq, bodyQ, juce::Decibels::decibelsToGain (bodyGainDb));

        for (int ch = 0; ch < 2; ++ch)
        {
            hpFilter[ch].coefficients = hpCoeffs;
            lpFilter[ch].coefficients = lpCoeffs;
            presenceFilter[ch].coefficients = presCoeffs;
            bodyFilter[ch].coefficients = bodyCoeffs;
        }
    }

    Params params;
    double sampleRate = 44100.0;
    bool bypassed = false;
    bool isPrepared = false;

    juce::dsp::IIR::Filter<float> hpFilter[2];
    juce::dsp::IIR::Filter<float> lpFilter[2];
    juce::dsp::IIR::Filter<float> presenceFilter[2];
    juce::dsp::IIR::Filter<float> bodyFilter[2];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CabSimProcessor)
};
