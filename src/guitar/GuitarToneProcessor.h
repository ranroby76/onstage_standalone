// ==============================================================================
//  GuitarToneProcessor.h
//  OnStage - Guitar Tone Module
//
//  2-band Baxandall shelving EQ (bass + treble) with mid peak and
//  presence/tilt control. Modeled after classic guitar amp tone stacks.
//
//  Uses RBJ (Robert Bristow-Johnson) cookbook biquad formulas — public
//  domain, no licensing concerns.
//
//  Frequency targets:
//    Bass shelf  — 250 Hz (guitar low-end body)
//    Mid peak    — 800 Hz (mid-range honk/cut)
//    Treble shelf— 3.5 kHz (guitar brightness/bite)
//    Presence    — Post tilt: dark ↔ bright overall shift
//
//  Parameters (6):
//    Bass     — Low shelf gain (-12..+12 dB)
//    Mid      — Mid peak gain (-12..+12 dB)
//    Treble   — High shelf gain (-12..+12 dB)
//    MidFreq  — Mid peak center frequency (200..3000 Hz)
//    Presence — Overall tilt / brightness (-1..+1)
//    Mix      — Dry/wet (0..1)
// ==============================================================================

#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>

class GuitarToneProcessor
{
public:
    struct Params
    {
        float bass     = 0.0f;    // dB (-12..+12)
        float mid      = 0.0f;    // dB (-12..+12)
        float treble   = 0.0f;    // dB (-12..+12)
        float midFreq  = 800.0f;  // Hz (200..3000)
        float presence = 0.0f;    // -1..+1 (tilt)
        float mix      = 1.0f;    // 0..1

        bool operator==(const Params& o) const
        {
            return bass == o.bass && mid == o.mid && treble == o.treble
                && midFreq == o.midFreq && presence == o.presence && mix == o.mix;
        }
        bool operator!=(const Params& o) const { return !(*this == o); }
    };

    GuitarToneProcessor() = default;

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        reset();
        updateCoeffs();
        isPrepared = true;
    }

    void reset()
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            bassFilter[ch].reset();
            midFilter[ch].reset();
            trebleFilter[ch].reset();
            tiltLP[ch].reset();
            tiltHP[ch].reset();
        }
    }

    void process(juce::AudioBuffer<float>& buffer)
    {
        if (bypassed || !isPrepared) return;

        // Update coefficients if params changed (cheap check)
        if (params != prevParams)
        {
            updateCoeffs();
            prevParams = params;
        }

        const int numSamples  = buffer.getNumSamples();
        const int numChannels = juce::jmin(2, buffer.getNumChannels());
        const float mixW      = juce::jlimit(0.0f, 1.0f, params.mix);
        const float mixD      = 1.0f - mixW;

        for (int i = 0; i < numSamples; ++i)
        {
            for (int ch = 0; ch < numChannels; ++ch)
            {
                float* data = buffer.getWritePointer(ch);
                float dry = data[i];
                float x = dry;

                // 3-band EQ chain: bass shelf → mid peak → treble shelf
                x = bassFilter[ch].processSample(x);
                x = midFilter[ch].processSample(x);
                x = trebleFilter[ch].processSample(x);

                // Presence tilt: blend LP darkening or HP brightening
                if (params.presence < -0.01f)
                {
                    // Darken: mix in lowpass
                    float lp = tiltLP[ch].processSample(x);
                    float amt = -params.presence; // 0..1
                    x = x * (1.0f - amt * 0.5f) + lp * amt * 0.5f;
                    tiltHP[ch].processSample(x); // keep state warm
                }
                else if (params.presence > 0.01f)
                {
                    // Brighten: mix in highpass (adds sparkle)
                    float hp = tiltHP[ch].processSample(x);
                    float amt = params.presence; // 0..1
                    x = x + hp * amt * 0.6f;
                    tiltLP[ch].processSample(x); // keep state warm
                }
                else
                {
                    // Neutral — keep filter states warm
                    tiltLP[ch].processSample(x);
                    tiltHP[ch].processSample(x);
                }

                data[i] = dry * mixD + x * mixW;
            }
        }
    }

    void setParams(const Params& p) { params = p; }
    Params getParams() const { return params; }
    void setBypassed(bool b) { bypassed = b; }
    bool isBypassed() const { return bypassed; }

private:
    // Simple biquad filter section
    struct Biquad
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        float z1 = 0, z2 = 0;

        void reset() { z1 = z2 = 0; }

        float processSample(float in)
        {
            float out = b0 * in + z1;
            z1 = b1 * in - a1 * out + z2;
            z2 = b2 * in - a2 * out;
            return out;
        }

        // RBJ low shelf
        void setLowShelf(double freq, double gainDB, double sr)
        {
            double A  = std::pow(10.0, gainDB / 40.0);
            double w0 = 2.0 * pi * freq / sr;
            double cosw = std::cos(w0);
            double sinw = std::sin(w0);
            double alpha = sinw / (2.0 * 0.707);
            double sqA = std::sqrt(A);

            double a0inv = 1.0 / ((A + 1.0) + (A - 1.0) * cosw + 2.0 * sqA * alpha);
            b0 = (float)(A * ((A + 1.0) - (A - 1.0) * cosw + 2.0 * sqA * alpha) * a0inv);
            b1 = (float)(2.0 * A * ((A - 1.0) - (A + 1.0) * cosw) * a0inv);
            b2 = (float)(A * ((A + 1.0) - (A - 1.0) * cosw - 2.0 * sqA * alpha) * a0inv);
            a1 = (float)(-2.0 * ((A - 1.0) + (A + 1.0) * cosw) * a0inv);
            a2 = (float)(((A + 1.0) + (A - 1.0) * cosw - 2.0 * sqA * alpha) * a0inv);
        }

        // RBJ high shelf
        void setHighShelf(double freq, double gainDB, double sr)
        {
            double A  = std::pow(10.0, gainDB / 40.0);
            double w0 = 2.0 * pi * freq / sr;
            double cosw = std::cos(w0);
            double sinw = std::sin(w0);
            double alpha = sinw / (2.0 * 0.707);
            double sqA = std::sqrt(A);

            double a0inv = 1.0 / ((A + 1.0) - (A - 1.0) * cosw + 2.0 * sqA * alpha);
            b0 = (float)(A * ((A + 1.0) + (A - 1.0) * cosw + 2.0 * sqA * alpha) * a0inv);
            b1 = (float)(-2.0 * A * ((A - 1.0) + (A + 1.0) * cosw) * a0inv);
            b2 = (float)(A * ((A + 1.0) + (A - 1.0) * cosw - 2.0 * sqA * alpha) * a0inv);
            a1 = (float)(2.0 * ((A - 1.0) - (A + 1.0) * cosw) * a0inv);
            a2 = (float)(((A + 1.0) - (A - 1.0) * cosw - 2.0 * sqA * alpha) * a0inv);
        }

        // RBJ peaking EQ
        void setPeak(double freq, double gainDB, double Q, double sr)
        {
            double A  = std::pow(10.0, gainDB / 40.0);
            double w0 = 2.0 * pi * freq / sr;
            double cosw = std::cos(w0);
            double sinw = std::sin(w0);
            double alpha = sinw / (2.0 * Q);

            double a0inv = 1.0 / (1.0 + alpha / A);
            b0 = (float)((1.0 + alpha * A) * a0inv);
            b1 = (float)((-2.0 * cosw) * a0inv);
            b2 = (float)((1.0 - alpha * A) * a0inv);
            a1 = (float)((-2.0 * cosw) * a0inv);
            a2 = (float)((1.0 - alpha / A) * a0inv);
        }

        // Simple 1st-order lowpass (for tilt)
        void setLP1(double freq, double sr)
        {
            double w0 = 2.0 * pi * freq / sr;
            double g = std::tan(w0 * 0.5);
            double a0inv = 1.0 / (1.0 + g);
            b0 = (float)(g * a0inv);
            b1 = (float)(g * a0inv);
            b2 = 0.0f;
            a1 = (float)((g - 1.0) * a0inv);
            a2 = 0.0f;
        }

        // Simple 1st-order highpass (for tilt)
        void setHP1(double freq, double sr)
        {
            double w0 = 2.0 * pi * freq / sr;
            double g = std::tan(w0 * 0.5);
            double a0inv = 1.0 / (1.0 + g);
            b0 = (float)(a0inv);
            b1 = (float)(-a0inv);
            b2 = 0.0f;
            a1 = (float)((g - 1.0) * a0inv);
            a2 = 0.0f;
        }
    };

    static constexpr double pi = 3.141592653589793;

    void updateCoeffs()
    {
        float bassDB   = juce::jlimit(-12.0f, 12.0f, params.bass);
        float midDB    = juce::jlimit(-12.0f, 12.0f, params.mid);
        float trebleDB = juce::jlimit(-12.0f, 12.0f, params.treble);
        float midF     = juce::jlimit(200.0f, 3000.0f, params.midFreq);

        for (int ch = 0; ch < 2; ++ch)
        {
            bassFilter[ch].setLowShelf(250.0, (double)bassDB, sampleRate);
            midFilter[ch].setPeak((double)midF, (double)midDB, 1.4, sampleRate);
            trebleFilter[ch].setHighShelf(3500.0, (double)trebleDB, sampleRate);
            tiltLP[ch].setLP1(2000.0, sampleRate);
            tiltHP[ch].setHP1(2000.0, sampleRate);
        }
    }

    Params params;
    Params prevParams;
    double sampleRate = 44100.0;
    bool bypassed = false;
    bool isPrepared = false;

    Biquad bassFilter[2];
    Biquad midFilter[2];
    Biquad trebleFilter[2];
    Biquad tiltLP[2];
    Biquad tiltHP[2];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GuitarToneProcessor)
};
