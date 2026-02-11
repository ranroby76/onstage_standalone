// ==============================================================================
//  GuitarReverbProcessor.h
//  OnStage - Guitar Reverb (Freeverb algorithm)
//
//  Schroeder/Moorer reverb: 8 parallel comb filters → 4 series all-pass.
//  Tuned for guitar-friendly reverb (spring/room/hall).
//
//  Parameters:
//  - Size: Room size (0..1)
//  - Damping: High frequency decay (0..1)
//  - Mix: Dry/wet (0..1)
//  - Width: Stereo width (0..1)
// ==============================================================================

#pragma once
#include <juce_dsp/juce_dsp.h>
#include <vector>

class GuitarReverbProcessor
{
public:
    struct Params
    {
        float size    = 0.5f;   // 0..1
        float damping = 0.5f;   // 0..1
        float mix     = 0.3f;   // 0..1
        float width   = 0.8f;   // 0..1
        float predelayMs = 0.0f; // 0..100

        bool operator== (const Params& o) const
        { return size == o.size && damping == o.damping && mix == o.mix
              && width == o.width && predelayMs == o.predelayMs; }
        bool operator!= (const Params& o) const { return !(*this == o); }
    };

    GuitarReverbProcessor() = default;

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;

        // Freeverb comb filter lengths (at 44100 Hz, scaled for SR)
        const int combLengths[numCombs] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
        const int apLengths[numAllpass]  = { 556, 441, 341, 225 };
        const int stereoSpread = 23;

        double srRatio = sampleRate / 44100.0;

        for (int i = 0; i < numCombs; ++i)
        {
            int lenL = (int)(combLengths[i] * srRatio);
            int lenR = (int)((combLengths[i] + stereoSpread) * srRatio);
            combL[i].resize (lenL, 0.0f);
            combR[i].resize (lenR, 0.0f);
            combIndexL[i] = combIndexR[i] = 0;
            combFilterL[i] = combFilterR[i] = 0.0f;
        }

        for (int i = 0; i < numAllpass; ++i)
        {
            int lenL = (int)(apLengths[i] * srRatio);
            int lenR = (int)((apLengths[i] + stereoSpread) * srRatio);
            apL[i].resize (lenL, 0.0f);
            apR[i].resize (lenR, 0.0f);
            apIndexL[i] = apIndexR[i] = 0;
        }

        applyParams();
        isPrepared = true;
    }

    void reset()
    {
        for (int i = 0; i < numCombs; ++i)
        {
            std::fill (combL[i].begin(), combL[i].end(), 0.0f);
            std::fill (combR[i].begin(), combR[i].end(), 0.0f);
            combFilterL[i] = combFilterR[i] = 0.0f;
        }
        for (int i = 0; i < numAllpass; ++i)
        {
            std::fill (apL[i].begin(), apL[i].end(), 0.0f);
            std::fill (apR[i].begin(), apR[i].end(), 0.0f);
        }
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        if (bypassed || !isPrepared) return;

        const int numSamples  = buffer.getNumSamples();
        const int numChannels = juce::jmin (2, buffer.getNumChannels());
        const float wet = params.mix;
        const float dry = 1.0f - params.mix;
        const float wet1 = wet * (params.width * 0.5f + 0.5f);
        const float wet2 = wet * ((1.0f - params.width) * 0.5f);

        for (int i = 0; i < numSamples; ++i)
        {
            float inL = buffer.getSample (0, i);
            float inR = numChannels > 1 ? buffer.getSample (1, i) : inL;
            float input = (inL + inR) * 0.5f * 0.015f;  // scale down for reverb

            // Parallel comb filters
            float outL = 0.0f, outR = 0.0f;
            for (int c = 0; c < numCombs; ++c)
            {
                // Left
                float combOutL = combL[c][(size_t)combIndexL[c]];
                combFilterL[c] = combOutL * (1.0f - damp) + combFilterL[c] * damp;
                combL[c][(size_t)combIndexL[c]] = input + combFilterL[c] * feedback;
                combIndexL[c] = (combIndexL[c] + 1) % (int)combL[c].size();
                outL += combOutL;

                // Right
                float combOutR = combR[c][(size_t)combIndexR[c]];
                combFilterR[c] = combOutR * (1.0f - damp) + combFilterR[c] * damp;
                combR[c][(size_t)combIndexR[c]] = input + combFilterR[c] * feedback;
                combIndexR[c] = (combIndexR[c] + 1) % (int)combR[c].size();
                outR += combOutR;
            }

            // Series all-pass filters
            for (int a = 0; a < numAllpass; ++a)
            {
                // Left
                float bufOutL = apL[a][(size_t)apIndexL[a]];
                apL[a][(size_t)apIndexL[a]] = outL + bufOutL * 0.5f;
                outL = bufOutL - outL;
                apIndexL[a] = (apIndexL[a] + 1) % (int)apL[a].size();

                // Right
                float bufOutR = apR[a][(size_t)apIndexR[a]];
                apR[a][(size_t)apIndexR[a]] = outR + bufOutR * 0.5f;
                outR = bufOutR - outR;
                apIndexR[a] = (apIndexR[a] + 1) % (int)apR[a].size();
            }

            // Mix output
            float* dataL = buffer.getWritePointer (0);
            dataL[i] = inL * dry + outL * wet1 + outR * wet2;

            if (numChannels > 1)
            {
                float* dataR = buffer.getWritePointer (1);
                dataR[i] = inR * dry + outR * wet1 + outL * wet2;
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
        feedback = params.size * 0.28f + 0.7f;   // 0.7..0.98
        feedback = juce::jlimit (0.0f, 0.98f, feedback);
        damp = params.damping;
    }

    static constexpr int numCombs = 8;
    static constexpr int numAllpass = 4;

    Params params;
    double sampleRate = 44100.0;
    bool bypassed = false;
    bool isPrepared = false;

    float feedback = 0.84f;
    float damp = 0.5f;

    // Comb filters
    std::vector<float> combL[numCombs], combR[numCombs];
    int combIndexL[numCombs] = {}, combIndexR[numCombs] = {};
    float combFilterL[numCombs] = {}, combFilterR[numCombs] = {};

    // All-pass filters
    std::vector<float> apL[numAllpass], apR[numAllpass];
    int apIndexL[numAllpass] = {}, apIndexR[numAllpass] = {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GuitarReverbProcessor)
};
