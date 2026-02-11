// ==============================================================================
//  GuitarPhaserProcessor.h
//  OnStage - Guitar Phaser
//
//  Based on the textbook phaser from:
//  "Digital Audio Effects: Theory, Implementation and Application"
//  by Joshua D. Reiss and Andrew P. McPherson
//  Code reference: getdunne/audio-effects (GPL-3)
//
//  Classic phaser using cascaded first-order allpass filters with LFO
//  modulation, feedback, stereo offset, and dry/wet mix.
//
//  Parameters:
//  - BaseFreq:   Sweep base frequency (50..1000 Hz)
//  - SweepWidth: How wide the LFO sweeps (50..5000 Hz)
//  - Rate:       LFO speed (0.05..2.0 Hz)
//  - Depth:      Effect intensity (0..1)
//  - Feedback:   Resonance (0..0.99)
//  - Stereo:     LFO phase offset between L/R (0=off, 1=on, 90 deg)
//  - Waveform:   LFO shape (0=Sine, 1=Tri, 2=Square, 3=Saw)
//  - Stages:     Number of allpass filters (2..10, even)
//  - Mix:        Dry/Wet (0..1)
// ==============================================================================

#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>

class GuitarPhaserProcessor
{
public:
    struct Params
    {
        float baseFreq   = 200.0f;  // Hz  (50..1000)
        float sweepWidth = 2000.0f; // Hz  (50..5000)
        float rate       = 0.5f;    // Hz  (0.05..2.0)
        float depth      = 1.0f;    // 0..1
        float feedback   = 0.0f;    // 0..0.99
        float stereo     = 0.0f;    // 0 or 1
        int   waveform   = 0;       // 0..3
        int   stages     = 4;       // 2..10 (even)
        float mix        = 0.5f;    // 0..1

        bool operator==(const Params& o) const
        {
            return baseFreq == o.baseFreq && sweepWidth == o.sweepWidth
                && rate == o.rate && depth == o.depth && feedback == o.feedback
                && stereo == o.stereo && waveform == o.waveform
                && stages == o.stages && mix == o.mix;
        }
        bool operator!=(const Params& o) const { return !(*this == o); }
    };

    GuitarPhaserProcessor() = default;

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        inverseSampleRate = 1.0 / sampleRate;
        reset();
        isPrepared = true;
    }

    void reset()
    {
        lfoPhase = 0.0f;
        for (int ch = 0; ch < 2; ++ch)
        {
            for (int s = 0; s < maxStages; ++s)
            {
                apX1[ch][s] = 0.0f;
                apY1[ch][s] = 0.0f;
            }
            lastFilterOutput[ch] = 0.0f;
        }
    }

    void process(juce::AudioBuffer<float>& buffer)
    {
        if (bypassed || !isPrepared) return;

        const int numSamples  = buffer.getNumSamples();
        const int numChannels = juce::jmin(2, buffer.getNumChannels());

        const float baseFreq   = juce::jlimit(50.0f, 1000.0f, params.baseFreq);
        const float sweepWidth = juce::jlimit(50.0f, 5000.0f, params.sweepWidth);
        const float rate       = juce::jlimit(0.05f, 2.0f, params.rate);
        const float depth      = juce::jlimit(0.0f, 1.0f, params.depth);
        const float feedback   = juce::jlimit(0.0f, 0.99f, params.feedback);
        const bool  stereoMode = (params.stereo >= 0.5f);
        const int   waveform   = juce::jlimit(0, 3, params.waveform);
        const int   stages     = juce::jlimit(2, maxStages, params.stages & ~1); // force even
        const float mixWet     = juce::jlimit(0.0f, 1.0f, params.mix);
        const float mixDry     = 1.0f - mixWet;

        const float lfoInc = (float)(rate * inverseSampleRate);

        // Update interval for filter coefficients (every 8 samples — cheaper)
        constexpr int updateInterval = 8;

        float ph = lfoPhase;
        float channel0EndPhase = ph;
        unsigned int sc = sampleCount;

        for (int channel = 0; channel < numChannels; ++channel)
        {
            float* channelData = buffer.getWritePointer(channel);
            ph = lfoPhase;
            sc = sampleCount;

            // Stereo: offset right channel LFO by 90 degrees
            if (stereoMode && channel != 0)
                ph = std::fmod(ph + 0.25f, 1.0f);

            for (int i = 0; i < numSamples; ++i)
            {
                float dry = channelData[i];
                float out = dry;

                // Add feedback from last sample
                if (feedback > 0.0f)
                    out += feedback * lastFilterOutput[channel];

                // Update allpass filter coefficients periodically
                if (sc % updateInterval == 0)
                {
                    float lfoVal = getLfoSample(ph, waveform); // output [0..1]
                    double centreFreq = (double)baseFreq + (double)sweepWidth * (double)lfoVal;
                    centreFreq = juce::jlimit(20.0, sampleRate * 0.45, centreFreq);

                    // Compute allpass coefficient using bilinear transform
                    // a = (1 - tan(w0/2)) / (1 + tan(w0/2))
                    double w0 = juce::jmin(centreFreq * inverseSampleRate, 0.99 * pi);
                    double tanHalf = std::tan(0.5 * w0);
                    float coeff = (float)((1.0 - tanHalf) / (1.0 + tanHalf));

                    // Store coefficient for all stages on this channel
                    for (int s = 0; s < stages; ++s)
                        apCoeff[channel][s] = coeff;
                }

                // Process cascade of first-order allpass filters
                // Each allpass: y[n] = a*x[n] + (-1)*x[n-1] + a*y[n-1]
                //   equivalent: y[n] = a * (x[n] - y[n-1]) + x[n-1]
                //   where a = (1 - tan(w0/2)) / (1 + tan(w0/2))
                for (int s = 0; s < stages; ++s)
                {
                    float a  = apCoeff[channel][s];
                    float x  = out;
                    float y  = a * x + apX1[channel][s] * (-1.0f) + a * apY1[channel][s];
                    // Simplified form matching textbook:
                    // y = a * (x - y1) + x1
                    // But using the expanded direct form for clarity:
                    // y = a*x - x1 + a*y1  ... wait, let's use the textbook form exactly:
                    // b0 = a, b1 = -1, a1 = a  (from OnePoleAllpassFilter.cpp)
                    // y[n] = b0*x[n] + b1*x[n-1] + a1*y[n-1]
                    //      = a*x[n] + (-1)*x[n-1] + a*y[n-1]
                    apY1[channel][s] = y;
                    apX1[channel][s] = x;
                    out = y;
                }

                // Store for feedback
                lastFilterOutput[channel] = out;

                // Mix: depth controls how much of filtered signal mixes with dry
                // depth=0 => input only, depth=1 => evenly balanced
                float dfrac = 0.5f * depth;
                float mixed = (1.0f - dfrac) * dry + dfrac * out;

                // Final dry/wet mix
                channelData[i] = dry * mixDry + mixed * mixWet;

                // Advance LFO
                ph += lfoInc;
                while (ph >= 1.0f) ph -= 1.0f;

                sc++;
            }

            if (channel == 0) channel0EndPhase = ph;
        }

        lfoPhase = channel0EndPhase;
        sampleCount = sc;
    }

    void setParams(const Params& p) { params = p; }
    Params getParams() const { return params; }
    void setBypassed(bool b) { bypassed = b; }
    bool isBypassed() const { return bypassed; }

private:
    static constexpr int maxStages = 10;
    static constexpr double pi = 3.14159265358979323846;

    // LFO generator — biased output [0..1], matching textbook
    float getLfoSample(float phase, int waveform) const
    {
        constexpr float twoPi = 6.283185307179586f;
        switch (waveform)
        {
            default:
            case 0: // Sine
                return 0.5f + 0.5f * std::sin(twoPi * phase);
            case 1: // Triangle
            {
                if (phase < 0.25f)       return 0.5f + 2.0f * phase;
                else if (phase < 0.75f)  return 1.0f - 2.0f * (phase - 0.25f);
                else                     return 2.0f * (phase - 0.75f);
            }
            case 2: // Square
                return (phase < 0.5f) ? 1.0f : 0.0f;
            case 3: // Sawtooth
            {
                if (phase < 0.5f) return 0.5f + phase;
                else              return phase - 0.5f;
            }
        }
    }

    Params params;
    double sampleRate = 44100.0;
    double inverseSampleRate = 1.0 / 44100.0;
    bool bypassed = false;
    bool isPrepared = false;

    float lfoPhase = 0.0f;
    unsigned int sampleCount = 0;

    // Allpass filter state: [channel][stage]
    float apX1[2][maxStages] = {};    // previous input
    float apY1[2][maxStages] = {};    // previous output
    float apCoeff[2][maxStages] = {}; // current coefficient

    // Feedback storage per channel
    float lastFilterOutput[2] = {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GuitarPhaserProcessor)
};
