// ==============================================================================
//  GuitarTremoloProcessor.h
//  OnStage - Guitar Tremolo effect
//
//  Classic amplitude modulation effect modeled after Fender amp tremolo:
//  - Bias-vary style: signal × (1 - depth × lfo), never fully mutes
//  - Multiple LFO shapes for different tremolo characters
//  - Stereo mode with adjustable phase offset between L/R
//
//  Waveform characters:
//    Sine     — Smooth, classic Fender blackface tremolo
//    Triangle — Slightly more "present" pulsing, Vox-like
//    Square   — Choppy, stutter/gate effect (TremoSquare style)
//    Ramp Up  — Asymmetric swell (softer attack, sharp drop)
//    Ramp Down— Asymmetric chop (sharp attack, soft release)
//    S&H      — Random stepped volume for lo-fi/experimental
//
//  Parameters (6):
//    Rate     — LFO speed (0.5..15 Hz, classic range 3-8 Hz)
//    Depth    — Modulation amount (0..1, 0=off, 1=full mute on trough)
//    Wave     — LFO shape (0..5: Sine,Tri,Square,RampUp,RampDn,S&H)
//    Stereo   — L/R phase offset (0..1, 0=mono, 0.5=opposite/panning)
//    Bias     — Shifts modulation center up (0..1, higher=less dip)
//    Mix      — Dry/wet (0..1, typically 1.0 for tremolo)
// ==============================================================================

#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <cstdlib>

class GuitarTremoloProcessor
{
public:
    struct Params
    {
        float rate    = 5.0f;   // Hz (0.5..15)
        float depth   = 0.6f;   // 0..1
        int   wave    = 0;      // 0..5
        float stereo  = 0.0f;   // 0..1 (L/R phase offset)
        float bias    = 0.0f;   // 0..1 (shifts mod center up)
        float mix     = 1.0f;   // 0..1

        bool operator==(const Params& o) const
        {
            return rate == o.rate && depth == o.depth && wave == o.wave
                && stereo == o.stereo && bias == o.bias && mix == o.mix;
        }
        bool operator!=(const Params& o) const { return !(*this == o); }
    };

    GuitarTremoloProcessor() = default;

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        reset();
        isPrepared = true;
    }

    void reset()
    {
        lfoPhase = 0.0f;
        shValue = 0.5f;
        prevSign = false;
    }

    void process(juce::AudioBuffer<float>& buffer)
    {
        if (bypassed || !isPrepared) return;

        const int numSamples  = buffer.getNumSamples();
        const int numChannels = juce::jmin(2, buffer.getNumChannels());

        const float rateHz  = juce::jlimit(0.5f, 15.0f, params.rate);
        const float depth   = juce::jlimit(0.0f, 1.0f, params.depth);
        const float stereoO = juce::jlimit(0.0f, 1.0f, params.stereo);
        const float biasV   = juce::jlimit(0.0f, 1.0f, params.bias);
        const float mixW    = juce::jlimit(0.0f, 1.0f, params.mix);
        const float mixD    = 1.0f - mixW;
        const float lfoInc  = rateHz / (float)sampleRate;

        for (int i = 0; i < numSamples; ++i)
        {
            for (int ch = 0; ch < numChannels; ++ch)
            {
                // Phase for this channel (R offset by stereo amount)
                float phase = lfoPhase;
                if (ch == 1)
                {
                    phase += stereoO;
                    if (phase >= 1.0f) phase -= 1.0f;
                }

                // Generate LFO value in range 0..1 (unipolar for amplitude mod)
                float lfo = generateLFO(phase, params.wave);

                // Tremolo gain:
                // Classic bias-vary: gain = 1 - depth * lfo + bias * depth
                // At lfo=0: gain = 1 (full volume)
                // At lfo=1: gain = 1 - depth + bias*depth = 1 - depth*(1-bias)
                // bias=0: full depth range. bias=1: no modulation.
                float effectiveDepth = depth * (1.0f - biasV);
                float gain = 1.0f - effectiveDepth * lfo;

                float* data = buffer.getWritePointer(ch);
                float dry = data[i];
                data[i] = dry * mixD + (dry * gain) * mixW;
            }

            // Advance LFO (shared phase, stereo offset applied per-channel)
            lfoPhase += lfoInc;
            if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
        }
    }

    void setParams(const Params& p) { params = p; }
    Params getParams() const { return params; }
    void setBypassed(bool b) { bypassed = b; }
    bool isBypassed() const { return bypassed; }

    static const char* getWaveName(int w)
    {
        static const char* names[] = { "Sine", "Tri", "Square", "RampUp", "RampDn", "S&H" };
        if (w >= 0 && w < 6) return names[w];
        return "Sine";
    }

private:
    // LFO output: 0..1 unipolar (0=no attenuation, 1=max attenuation)
    float generateLFO(float phase, int wave)
    {
        switch (wave)
        {
            default:
            case 0: // Sine (0..1 unipolar)
                return 0.5f - 0.5f * std::cos(phase * 6.283185307f);

            case 1: // Triangle
            {
                if (phase < 0.5f)
                    return phase * 2.0f;
                else
                    return 2.0f - phase * 2.0f;
            }

            case 2: // Square (with slight smoothing to reduce clicks)
            {
                if (phase < 0.48f) return 0.0f;
                if (phase < 0.50f) return (phase - 0.48f) / 0.02f;
                if (phase < 0.98f) return 1.0f;
                return 1.0f - (phase - 0.98f) / 0.02f;
            }

            case 3: // Ramp up (swell then drop)
                return phase;

            case 4: // Ramp down (drop then swell)
                return 1.0f - phase;

            case 5: // Sample & Hold
            {
                bool positive = (phase < 0.5f);
                if (positive && !prevSign)
                    shValue = (float)(rand()) / (float)RAND_MAX;
                prevSign = positive;
                return shValue;
            }
        }
    }

    Params params;
    double sampleRate = 44100.0;
    bool bypassed = false;
    bool isPrepared = false;

    float lfoPhase = 0.0f;
    float shValue = 0.5f;
    bool prevSign = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GuitarTremoloProcessor)
};
