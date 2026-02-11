// ==============================================================================
//  GuitarPhaserProcessor.h
//  OnStage - Guitar Phaser (upgraded with SST-derived features)
//
//  Cascade of all-pass filters with multi-shape LFO, stereo offset,
//  stage spread, sharpness (Q), center frequency, and post tone filter.
//
//  Core DSP math extracted from SST Phaser (GPL-3, surge-synthesizer).
//  Original OnStage structure preserved (Params struct, prepare/process API).
//
//  Parameters:
//  - Center:    Sweep center frequency offset (-1..+1, bipolar)
//  - Rate:      LFO speed (0.05..10 Hz)
//  - Depth:     Mod depth (0..1)
//  - Feedback:  Resonance (-1..+1, bipolar, clamped to ±0.95 internally)
//  - Stages:    Number of all-pass stages (1..16, 1 = legacy 4-stage mode)
//  - Spread:    Frequency spacing between stages (0..1)
//  - Sharpness: All-pass Q (-1..+1, bipolar)
//  - Stereo:    LFO phase offset between L/R (0..1, 0.5 = quadrature)
//  - Waveform:  LFO shape (0=Sine,1=Tri,2=Ramp,3=Saw,4=Square,5=Noise,6=S&H)
//  - Tone:      Post-filter tilt (-1..+1, neg=darken, pos=brighten)
//  - Mix:       Dry/Wet (0..1)
// ==============================================================================

#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <cstdlib>

class GuitarPhaserProcessor
{
public:
    struct Params
    {
        float center    = 0.0f;    // -1..+1 (sweep center offset)
        float rate      = 0.5f;    // Hz (0.05..10)
        float depth     = 0.7f;    // 0..1
        float feedback  = 0.3f;    // -1..+1 (clamped ±0.95 internally)
        int   stages    = 4;       // 1..16 (1 = legacy 4-stage)
        float spread    = 0.5f;    // 0..1
        float sharpness = 0.0f;    // -1..+1 (bipolar, like SST)
        float stereo    = 0.0f;    // 0..1 (LFO phase offset)
        int   waveform  = 0;       // 0..6
        float tone      = 0.0f;    // -1..+1
        float mix       = 0.5f;    // 0..1

        bool operator==(const Params& o) const
        {
            return center == o.center && rate == o.rate && depth == o.depth
                && feedback == o.feedback && stages == o.stages && spread == o.spread
                && sharpness == o.sharpness && stereo == o.stereo && waveform == o.waveform
                && tone == o.tone && mix == o.mix;
        }
        bool operator!=(const Params& o) const { return !(*this == o); }
    };

    GuitarPhaserProcessor() = default;

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        reset();
        isPrepared = true;
    }

    void reset()
    {
        lfoPhaseL = 0.0f;
        for (int ch = 0; ch < 2; ++ch)
        {
            for (int s = 0; s < maxStages; ++s)
                allpassState[ch][s] = 0.0f;
            feedbackState[ch] = 0.0f;
            toneLpState[ch] = 0.0f;
            toneHpState[ch] = 0.0f;
        }
        noiseVal[0] = noiseVal[1] = 0.0f;
        shVal[0] = shVal[1] = 0.0f;
        prevLfoSign = false;
    }

    void process(juce::AudioBuffer<float>& buffer)
    {
        if (bypassed || !isPrepared) return;

        const int numSamples  = buffer.getNumSamples();
        const int numChannels = juce::jmin(2, buffer.getNumChannels());

        // Clamp params to SST ranges
        const float center    = juce::jlimit(-1.0f, 1.0f, params.center);
        const float depth     = juce::jlimit(0.0f, 1.0f, params.depth);  // SST clamps to 2.0 but we use 1.0 for guitar
        const float fb        = juce::jlimit(-0.95f, 0.95f, params.feedback);
        const float sharp     = juce::jlimit(-1.0f, 1.0f, params.sharpness);
        const float spreadAmt = juce::jlimit(0.0f, 1.0f, params.spread);
        const float stereoOff = juce::jlimit(0.0f, 1.0f, params.stereo);
        const float toneVal   = juce::jlimit(-1.0f, 1.0f, params.tone);
        const float mixWet    = juce::jlimit(0.0f, 1.0f, params.mix);
        const float mixDry    = 1.0f - mixWet;
        const float lfoInc    = (float)(juce::jlimit(0.05, 10.0, (double)params.rate) / sampleRate);

        // Determine stage count: stages=1 means legacy 4-stage mode (like SST)
        const int n_stages = juce::jlimit(1, maxStages, params.stages);
        const bool legacyMode = (n_stages < 2);
        const int actualStages = legacyMode ? 4 : n_stages;

        // SST legacy mode frequencies and spans (fixed 4-stage)
        static const float legacy_freq[4] = { 1.5f / 12.0f, 19.5f / 12.0f, 35.0f / 12.0f, 50.0f / 12.0f };
        static const float legacy_span[4] = { 2.0f, 1.5f, 1.0f, 0.5f };

        // Tone filter coefficients
        float lpCoeff = 1.0f, hpCoeff = 0.0f;
        if (toneVal < 0.0f)
        {
            float freq = 20000.0f * std::pow(0.02f, -toneVal);
            lpCoeff = 1.0f - std::exp(-twoPi * freq / (float)sampleRate);
        }
        else if (toneVal > 0.0f)
        {
            float freq = 20.0f * std::pow(100.0f, toneVal);
            hpCoeff = 1.0f - std::exp(-twoPi * freq / (float)sampleRate);
        }

        for (int i = 0; i < numSamples; ++i)
        {
            // Generate LFO values for L and R
            float lfoPhaseR = lfoPhaseL + stereoOff;
            if (lfoPhaseR >= 1.0f) lfoPhaseR -= 1.0f;

            float lfoL = generateLFO(lfoPhaseL, params.waveform, 0) * depth;
            float lfoR = generateLFO(lfoPhaseR, params.waveform, 1) * depth;

            for (int ch = 0; ch < numChannels; ++ch)
            {
                float* data = buffer.getWritePointer(ch);
                float dry = data[i];
                float lfo = (ch == 0) ? lfoL : lfoR;

                float in = dry + feedbackState[ch] * fb;
                in = juce::jlimit(-32.0f, 32.0f, in);

                // Process all-pass cascade
                float x = in;

                if (legacyMode)
                {
                    // SST legacy: 4 fixed stages with preset frequencies
                    // Base offset of 5 octaves (~262 Hz at center=0) puts sweep
                    // in the audible phaser range of ~200-5000 Hz
                    for (int s = 0; s < 4; ++s)
                    {
                        float noteVal = 5.0f + 2.0f * center + legacy_freq[s] + legacy_span[s] * lfo;
                        float freq = 8.175798f * std::pow(2.0f, noteVal);
                        freq = juce::jlimit(20.0f, (float)(sampleRate * 0.45), freq);

                        float wc = twoPi * freq / (float)sampleRate;
                        float apQ = 1.0f + 0.8f * sharp;
                        float t = std::tan(wc * 0.5f);
                        float apCoeff = (1.0f - t / apQ) / (1.0f + t / apQ);

                        float y = apCoeff * x + allpassState[ch][s];
                        allpassState[ch][s] = x - apCoeff * y;
                        x = y;
                    }
                }
                else
                {
                    // SST modern: N stages with spread
                    for (int s = 0; s < actualStages; ++s)
                    {
                        float stageCenter = std::pow(2.0f, (float)(s + 1) * 2.0f / (float)actualStages);
                        float modForStage = 2.0f / (float)(s + 1) * lfo;

                        float noteVal = 5.0f + 2.0f * center + spreadAmt * stageCenter + modForStage;
                        float freq = 8.175798f * std::pow(2.0f, noteVal);
                        freq = juce::jlimit(20.0f, (float)(sampleRate * 0.45), freq);

                        float wc = twoPi * freq / (float)sampleRate;
                        float apQ = 1.0f + 0.8f * sharp;
                        float t = std::tan(wc * 0.5f);
                        float apCoeff = (1.0f - t / apQ) / (1.0f + t / apQ);

                        float y = apCoeff * x + allpassState[ch][s];
                        allpassState[ch][s] = x - apCoeff * y;
                        x = y;
                    }
                }

                feedbackState[ch] = x;

                // Tone filter
                if (toneVal < 0.0f)
                {
                    toneLpState[ch] += lpCoeff * (x - toneLpState[ch]);
                    x = toneLpState[ch];
                }
                else if (toneVal > 0.0f)
                {
                    toneHpState[ch] += hpCoeff * (x - toneHpState[ch]);
                    x = x - toneHpState[ch];
                }

                data[i] = dry * mixDry + x * mixWet;
            }

            // Advance LFO
            lfoPhaseL += lfoInc;
            if (lfoPhaseL >= 1.0f) lfoPhaseL -= 1.0f;
        }
    }

    void setParams(const Params& p) { params = p; }
    Params getParams() const { return params; }
    void setBypassed(bool b) { bypassed = b; }
    bool isBypassed() const { return bypassed; }

private:
    static constexpr int maxStages = 16;
    static constexpr float twoPi = 6.283185307179586f;

    // LFO generator - 7 waveforms matching SST
    float generateLFO(float phase, int wave, int ch)
    {
        switch (wave)
        {
            default:
            case 0: // Sine
                return std::sin(phase * twoPi);
            case 1: // Triangle
            {
                float t = phase * 4.0f;
                if (phase < 0.25f) return t;
                if (phase < 0.75f) return 2.0f - t;
                return t - 4.0f;
            }
            case 2: // Ramp (up)
                return 2.0f * phase - 1.0f;
            case 3: // Saw (down)
                return 1.0f - 2.0f * phase;
            case 4: // Square
                return (phase < 0.5f) ? 1.0f : -1.0f;
            case 5: // Noise (smoothed random)
            {
                float r = ((float)(rand()) / (float)RAND_MAX) * 2.0f - 1.0f;
                noiseVal[ch] += 0.1f * (r - noiseVal[ch]);
                return noiseVal[ch];
            }
            case 6: // Sample & Hold
            {
                bool positive = (phase < 0.5f);
                if (ch == 0)
                {
                    if (positive && !prevLfoSign)
                        shVal[0] = ((float)(rand()) / (float)RAND_MAX) * 2.0f - 1.0f;
                    prevLfoSign = positive;
                }
                else
                {
                    if (positive && !prevLfoSign)
                        shVal[1] = ((float)(rand()) / (float)RAND_MAX) * 2.0f - 1.0f;
                }
                return shVal[ch];
            }
        }
    }

    Params params;
    double sampleRate = 44100.0;
    bool bypassed = false;
    bool isPrepared = false;

    float lfoPhaseL = 0.0f;
    float allpassState[2][maxStages] = {};
    float feedbackState[2] = {};
    float toneLpState[2] = {};
    float toneHpState[2] = {};
    float noiseVal[2] = {};
    float shVal[2] = {};
    bool prevLfoSign = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GuitarPhaserProcessor)
};