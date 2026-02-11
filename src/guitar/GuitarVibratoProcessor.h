// ==============================================================================
//  GuitarVibratoProcessor.h
//  OnStage - Guitar Vibrato effect
//
//  Classic pitch vibrato via short modulated delay line.
//  Also capable of chorus when mixing dry+wet (like Uni-Vibe territory).
//
//  How it works:
//    Input → delay line (2-12ms) → LFO modulates delay time → pitch shift
//    Shorter delay + faster rate = vibrato
//    Longer delay + dry blend = chorus
//
//  LFO shapes:
//    Sine     — Smooth, natural vibrato (classic)
//    Triangle — Slightly more linear pitch bend
//
//  Parameters (6):
//    Rate     — LFO speed (0.1..10 Hz, classic vibrato 4-7 Hz)
//    Depth    — Mod amount / pitch deviation (0..1)
//    Wave     — 0=Sine, 1=Triangle
//    Stereo   — L/R LFO phase offset (0..1, 0.5=wide stereo chorus)
//    Delay    — Base delay time bias (0..1: short=vibrato, long=chorus)
//    Mix      — Dry/wet (0=dry, 0.5=chorus, 1.0=pure vibrato)
// ==============================================================================

#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>

class GuitarVibratoProcessor
{
public:
    struct Params
    {
        float rate    = 5.0f;   // Hz (0.1..10)
        float depth   = 0.5f;   // 0..1
        int   wave    = 0;      // 0=Sine, 1=Triangle
        float stereo  = 0.0f;   // 0..1 (L/R phase offset)
        float delay   = 0.2f;   // 0..1 (base delay: 0=short/vibrato, 1=long/chorus)
        float mix     = 1.0f;   // 0..1 (1=vibrato, 0.5=chorus blend)

        bool operator==(const Params& o) const
        {
            return rate == o.rate && depth == o.depth && wave == o.wave
                && stereo == o.stereo && delay == o.delay && mix == o.mix;
        }
        bool operator!=(const Params& o) const { return !(*this == o); }
    };

    GuitarVibratoProcessor() = default;

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        // Max delay: 20ms = enough for deep chorus
        maxDelaySamples = (int)(sampleRate * 0.020) + 4;
        for (int ch = 0; ch < 2; ++ch)
        {
            delayLine[ch].assign((size_t)maxDelaySamples, 0.0f);
            writePos[ch] = 0;
        }
        reset();
        isPrepared = true;
    }

    void reset()
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            std::fill(delayLine[ch].begin(), delayLine[ch].end(), 0.0f);
            writePos[ch] = 0;
        }
        lfoPhase = 0.0f;
    }

    void process(juce::AudioBuffer<float>& buffer)
    {
        if (bypassed || !isPrepared) return;

        const int numSamples  = buffer.getNumSamples();
        const int numChannels = juce::jmin(2, buffer.getNumChannels());

        const float rateHz    = juce::jlimit(0.1f, 10.0f, params.rate);
        const float depthV    = juce::jlimit(0.0f, 1.0f, params.depth);
        const float stereoOff = juce::jlimit(0.0f, 1.0f, params.stereo);
        const float delayBias = juce::jlimit(0.0f, 1.0f, params.delay);
        const float mixW      = juce::jlimit(0.0f, 1.0f, params.mix);
        const float mixD      = 1.0f - mixW;
        const float lfoInc    = rateHz / (float)sampleRate;

        // Base delay: 1ms (vibrato) to 12ms (chorus territory)
        // depth modulates ±0.5ms (vibrato) to ±5ms (deep chorus)
        float baseDelayMs = 1.0f + delayBias * 11.0f;  // 1..12 ms
        float modDepthMs  = 0.5f + depthV * 4.5f;       // 0.5..5 ms

        float baseDelaySamples = baseDelayMs * (float)sampleRate * 0.001f;
        float modDepthSamples  = modDepthMs  * (float)sampleRate * 0.001f;

        for (int i = 0; i < numSamples; ++i)
        {
            for (int ch = 0; ch < numChannels; ++ch)
            {
                float* data = buffer.getWritePointer(ch);
                float dry = data[i];

                // Write to delay line
                delayLine[ch][(size_t)writePos[ch]] = dry;

                // LFO phase for this channel
                float phase = lfoPhase;
                if (ch == 1)
                {
                    phase += stereoOff;
                    if (phase >= 1.0f) phase -= 1.0f;
                }

                // Generate LFO (-1..+1)
                float lfo;
                if (params.wave == 1) // Triangle
                {
                    if (phase < 0.25f)      lfo = phase * 4.0f;
                    else if (phase < 0.75f) lfo = 2.0f - phase * 4.0f;
                    else                     lfo = phase * 4.0f - 4.0f;
                }
                else // Sine
                {
                    lfo = std::sin(phase * 6.283185307f);
                }

                // Modulated delay time
                float delaySmp = baseDelaySamples + lfo * modDepthSamples;
                delaySmp = juce::jlimit(1.0f, (float)(maxDelaySamples - 2), delaySmp);

                // Read from delay with cubic Hermite interpolation
                float readPos = (float)writePos[ch] - delaySmp;
                if (readPos < 0.0f) readPos += (float)maxDelaySamples;

                int idx = (int)std::floor(readPos);
                float frac = readPos - (float)idx;

                auto rd = [&](int offset) -> float {
                    int p = (idx + offset) % maxDelaySamples;
                    if (p < 0) p += maxDelaySamples;
                    return delayLine[ch][(size_t)p];
                };

                float y0 = rd(-1), y1 = rd(0), y2 = rd(1), y3 = rd(2);
                float c0 = y1;
                float c1 = 0.5f * (y2 - y0);
                float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
                float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
                float wet = ((c3 * frac + c2) * frac + c1) * frac + c0;

                data[i] = dry * mixD + wet * mixW;

                writePos[ch] = (writePos[ch] + 1) % maxDelaySamples;
            }

            // Advance LFO
            lfoPhase += lfoInc;
            if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
        }
    }

    void setParams(const Params& p) { params = p; }
    Params getParams() const { return params; }
    void setBypassed(bool b) { bypassed = b; }
    bool isBypassed() const { return bypassed; }

private:
    Params params;
    double sampleRate = 44100.0;
    bool bypassed = false;
    bool isPrepared = false;

    // Delay line per channel
    std::vector<float> delayLine[2];
    int writePos[2] = {};
    int maxDelaySamples = 882; // default ~20ms at 44.1k

    // LFO
    float lfoPhase = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GuitarVibratoProcessor)
};
