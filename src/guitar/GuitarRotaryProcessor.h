// ==============================================================================
//  GuitarRotaryProcessor.h
//  OnStage - Rotary Speaker (Leslie) effect
//
//  DSP math extracted from SST RotarySpeaker (GPL-3, surge-synthesizer).
//  Faithful reimplementation: same signal path, crossover frequencies,
//  doppler geometry, waveshaper gain compensation, rotor modulation.
//
//  Signal path:
//    Input → mono sum → [Drive/Waveshaper] → Crossover 800Hz →
//      Upper (horn): doppler delay line + tremolo amplitude → stereo
//      Lower: lowbass split 200Hz → sub (clean) + mid (rotor LFO mod)
//    → Width → Mix
//
//  Parameters (8):
//    Horn Rate  — Horn rotation speed (0.1..10 Hz)
//    Doppler    — Doppler delay depth (0..1)
//    Tremolo    — Horn amplitude modulation depth (0..1)
//    Rotor Rate — Bass rotor speed as multiplier of horn rate (0..2)
//    Drive      — Overdrive amount (0..1, 0=off/bypass)
//    Waveshape  — Drive model (0..7: Soft,Hard,Asym,Sine,Digital,OJD,Rectify,Fuzz)
//    Width      — Stereo spread (0..2, 1=normal)
//    Mix        — Dry/wet (0..1, default 0.33)
// ==============================================================================

#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <cstring>
#include <algorithm>

class GuitarRotaryProcessor
{
public:
    struct Params
    {
        float hornRate   = 1.5f;   // Hz (0.1..10)
        float doppler    = 0.3f;   // 0..1
        float tremolo    = 0.5f;   // 0..1
        float rotorRate  = 0.7f;   // 0..2 (multiplier of horn rate)
        float drive      = 0.0f;   // 0..1 (0 = drive off)
        int   waveshape  = 0;      // 0..7
        float width      = 1.0f;   // 0..2
        float mix        = 0.33f;  // 0..1

        bool operator==(const Params& o) const
        {
            return hornRate == o.hornRate && doppler == o.doppler && tremolo == o.tremolo
                && rotorRate == o.rotorRate && drive == o.drive && waveshape == o.waveshape
                && width == o.width && mix == o.mix;
        }
        bool operator!=(const Params& o) const { return !(*this == o); }
    };

    GuitarRotaryProcessor() = default;

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        blockSize = (int)spec.maximumBlockSize;
        reset();
        isPrepared = true;
    }

    void reset()
    {
        std::memset(delayBuffer, 0, sizeof(delayBuffer));
        wpos = 0;

        // Quadrature oscillators
        hornLfo.reset();
        rotorLfo.reset();

        // Biquad filters: crossover at 800 Hz, lowbass at 200 Hz
        // SST uses calc_omega(0.862496) for 800 Hz, calc_omega(-1.14) for 200 Hz
        // calc_omega converts note-like value: freq = 440 * 2^(v/12) * pi / sampleRate
        // But the actual target frequencies are 800 Hz and 200 Hz
        xoverFilter.reset();
        lowbassFilter.reset();
        updateFilterCoeffs();

        // Smoothing
        dLSmooth = 0.0f;
        dRSmooth = 0.0f;
        hornAmpL = 1.0f;
        hornAmpR = 1.0f;
        driveSmooth = 0.0f;
    }

    void process(juce::AudioBuffer<float>& buffer)
    {
        if (bypassed || !isPrepared) return;

        const int numSamples = buffer.getNumSamples();
        const int numChannels = juce::jmin(2, buffer.getNumChannels());
        if (numSamples == 0) return;

        // Clamp params
        const float hornHz   = juce::jlimit(0.1f, 10.0f, params.hornRate);
        const float dopplerV = juce::jlimit(0.0f, 1.0f, params.doppler);
        const float tremV    = juce::jlimit(0.0f, 1.0f, params.tremolo);
        const float rotorV   = juce::jlimit(0.0f, 2.0f, params.rotorRate);
        const float driveV   = juce::jlimit(0.0f, 1.0f, params.drive);
        const int   wsIdx    = juce::jlimit(0, 7, params.waveshape);
        const float widthV   = juce::jlimit(0.0f, 2.0f, params.width);
        const float mixV     = juce::jlimit(0.0f, 1.0f, params.mix);

        // Drive enabled?
        const bool driveOn = (driveV > 0.001f);

        // Waveshaper gain compensation (from SST - empirical values)
        float gainTweak = 1.0f, compensate = 4.0f, compStartsAt = 0.18f;
        bool sqDriveComp = false;
        switch (wsIdx)
        {
            case 0: // Soft (tanh) — default
                gainTweak = 1.0f; compensate = 4.0f; compStartsAt = 0.18f; break;
            case 1: // Hard — NOTE: SST has fallthrough from hard→asym, so hard gets asym values
                gainTweak = 1.15f; compensate = 9.0f; compStartsAt = 0.05f; break;
            case 2: // Asymmetric
                gainTweak = 1.15f; compensate = 9.0f; compStartsAt = 0.05f; break;
            case 3: // Sine fold
                gainTweak = 4.4f; compensate = 10.0f; compStartsAt = 0.0f; sqDriveComp = true; break;
            case 4: // Digital
                gainTweak = 1.0f; compensate = 4.0f; compStartsAt = 0.0f; break;
            case 5: // OJD
                gainTweak = 1.0f; compensate = 2.0f; compStartsAt = 0.0f; break;
            case 6: // Full-wave rectify
                gainTweak = 1.0f; compensate = 2.0f; compStartsAt = 0.0f; break;
            case 7: // Fuzz soft
                gainTweak = 1.0f; compensate = 2.0f; compStartsAt = 0.0f; break;
        }

        // Horn LFO rate (per-sample angular increment)
        // SST: lfo.set_rate(2*pi * pow(2,frate) / sampleRate * blockSize)
        // But SST frate is in log2 scale. We use Hz directly.
        float hornOmegaPerBlock = twoPi * hornHz / (float)sampleRate * (float)numSamples;
        hornLfo.setRate(hornOmegaPerBlock);

        // Rotor LFO rate: SST processes per-sample (not per-block)
        // SST: lf_lfo.set_rate(rotorRate * 2*pi * pow(2,frate) / sampleRate)
        float rotorOmegaPerSample = rotorV * twoPi * hornHz / (float)sampleRate;
        rotorLfo.setRate(rotorOmegaPerSample);

        // --- Compute doppler geometry (SST: two virtual speakers at (-2,-1) and (-2,+1)) ---
        // Horn LFO outputs: r=cos, i=sin
        float precalc0 = -2.0f - hornLfo.i;
        float precalc1 = -1.0f - hornLfo.r;
        float precalc2 =  1.0f - hornLfo.r;
        float lenL = std::sqrt(precalc0 * precalc0 + precalc1 * precalc1);
        float lenR = std::sqrt(precalc0 * precalc0 + precalc2 * precalc2);

        // Delay in samples: SST uses sampleRate * 0.0018 * doppler
        float delaySamples = (float)sampleRate * 0.0018f * dopplerV;
        float targetDL = delaySamples * lenL;
        float targetDR = delaySamples * lenR;

        // Tremolo: dot product of speaker-to-listener vector with velocity
        float dotpL = (precalc1 * hornLfo.r + precalc0 * hornLfo.i) / std::max(lenL, 0.001f);
        float dotpR = (precalc2 * hornLfo.r + precalc0 * hornLfo.i) / std::max(lenR, 0.001f);
        float a = tremV * 0.6f;
        float targetAmpL = (1.0f - a) + a * dotpL;
        float targetAmpR = (1.0f - a) + a * dotpR;

        // Advance horn LFO (block-rate, like SST)
        hornLfo.process();

        // --- Per-sample smoothing increments ---
        float dLInc = (targetDL - dLSmooth) / (float)numSamples;
        float dRInc = (targetDR - dRSmooth) / (float)numSamples;
        float ampLInc = (targetAmpL - hornAmpL) / (float)numSamples;
        float ampRInc = (targetAmpR - hornAmpR) / (float)numSamples;
        float driveInc = (driveV - driveSmooth) / (float)numSamples;

        // Temp buffers
        const int maxBlock = 4096;
        int ns = juce::jmin(numSamples, maxBlock);
        float upper[maxBlock], lower[maxBlock], lowerSub[maxBlock];
        float tbufL[maxBlock], tbufR[maxBlock];
        float wbL[maxBlock], wbR[maxBlock];

        // Get input pointers
        const float* inL = buffer.getReadPointer(0);
        const float* inR = (numChannels > 1) ? buffer.getReadPointer(1) : inL;
        float* outL = buffer.getWritePointer(0);
        float* outR = (numChannels > 1) ? buffer.getWritePointer(1) : nullptr;

        // Store dry for mix
        float dryL[maxBlock], dryR[maxBlock];
        for (int k = 0; k < ns; ++k)
        {
            dryL[k] = inL[k];
            dryR[k] = (outR) ? inR[k] : inL[k];
        }

        // --- Stage 1: Drive + mono sum ---
        for (int k = 0; k < ns; ++k)
        {
            float monoIn = 0.5f * (inL[k] + ((numChannels > 1) ? inR[k] : inL[k]));

            if (driveOn)
            {
                float driveFactor = 1.0f + (driveSmooth * driveSmooth * 15.0f);
                float shaped = applyWaveshaper(wsIdx, monoIn * driveFactor) * gainTweak;

                // Gain compensation (SST empirical)
                float gcf = 1.0f;
                if (driveSmooth >= compStartsAt)
                {
                    if (sqDriveComp)
                        gcf = 1.0f + ((driveSmooth * driveSmooth - compStartsAt) * compensate);
                    else
                        gcf = 1.0f + ((driveSmooth - compStartsAt) * compensate);
                }
                shaped /= gcf;

                upper[k] = shaped;
                driveSmooth += driveInc;
            }
            else
            {
                upper[k] = monoIn;
            }
            lower[k] = upper[k];
        }

        // --- Stage 2: Crossover filter (LP @ 800 Hz) on lower ---
        xoverFilter.processBlock(lower, ns);

        // --- Stage 3: Write upper to delay, read with doppler ---
        for (int k = 0; k < ns; ++k)
        {
            lowerSub[k] = lower[k];
            upper[k] -= lower[k]; // upper now = highpass content

            // Write to delay buffer
            int wp = (wpos + k) & (maxDelayLength - 1);
            delayBuffer[wp] = upper[k];

            // Read from delay with cubic interpolation
            float delL = std::max(1.0f, std::min(dLSmooth, (float)(maxDelayLength - 4)));
            float delR = std::max(1.0f, std::min(dRSmooth, (float)(maxDelayLength - 4)));

            tbufL[k] = readDelayCubic(wpos + k, delL);
            tbufR[k] = readDelayCubic(wpos + k, delR);

            dLSmooth += dLInc;
            dRSmooth += dRInc;
        }

        // --- Stage 4: Lowbass filter (LP @ 200 Hz) on lowerSub ---
        lowbassFilter.processBlock(lowerSub, ns);

        // --- Stage 5: Combine horn + rotor ---
        for (int k = 0; k < ns; ++k)
        {
            // lower[k] was LP@800. lowerSub[k] is now LP@200 of that.
            // mid-low = lower - lowerSub (200-800 Hz band)
            float midLow = lower[k] - lowerSub[k];

            // Bass = sub + rotor-modulated mid-low
            // SST: bass = lowerSub + midLow * (lf_lfo.r * 0.6 + 0.3)
            float bass = lowerSub[k] + midLow * (rotorLfo.r * 0.6f + 0.3f);

            wbL[k] = hornAmpL * tbufL[k] + bass;
            wbR[k] = hornAmpR * tbufR[k] + bass;

            // Advance rotor LFO per sample (like SST)
            rotorLfo.process();
            hornAmpL += ampLInc;
            hornAmpR += ampRInc;
        }

        // --- Stage 6: Width ---
        // SST width: mid/side processing
        // width=1 is normal, 0=mono, 2=extra wide
        {
            float widthS = 0.5f * widthV;  // side gain
            float widthM = 0.5f * (2.0f - widthV); // mid gain — when width=1, both are 0.5
            for (int k = 0; k < ns; ++k)
            {
                float mid  = wbL[k] + wbR[k];
                float side = wbL[k] - wbR[k];
                wbL[k] = mid * widthM + side * widthS;
                wbR[k] = mid * widthM - side * widthS;
            }
        }

        // --- Stage 7: Mix (dry/wet crossfade) ---
        for (int k = 0; k < ns; ++k)
        {
            outL[k] = dryL[k] * (1.0f - mixV) + wbL[k] * mixV;
            if (outR)
                outR[k] = dryR[k] * (1.0f - mixV) + wbR[k] * mixV;
        }

        // Advance write position
        wpos = (wpos + ns) & (maxDelayLength - 1);
    }

    void setParams(const Params& p) { params = p; }
    Params getParams() const { return params; }
    void setBypassed(bool b) { bypassed = b; }
    bool isBypassed() const { return bypassed; }

    // Waveshape model names for UI
    static const char* getWaveshapeName(int idx)
    {
        static const char* names[] = {
            "Soft", "Hard", "Asym", "Sine", "Digital", "OJD", "Rectify", "Fuzz"
        };
        if (idx >= 0 && idx < 8) return names[idx];
        return "Soft";
    }

private:
    static constexpr float twoPi = 6.283185307179586f;
    static constexpr float pi = 3.141592653589793f;
    static constexpr int maxDelayLength = 1 << 18; // 262144 samples (~5.9s @ 44.1k)

    // -------------------------------------------------------------------------
    // Quadrature oscillator (reimplements SurgeQuadrOsc)
    // Maintains cos(phase) in r, sin(phase) in i
    // process() advances by one step via complex multiplication
    // -------------------------------------------------------------------------
    struct QuadOsc
    {
        float r = 1.0f, i = 0.0f;  // current state (cos, sin)
        float dr = 1.0f, di = 0.0f; // rotation per step

        void reset() { r = 1.0f; i = 0.0f; dr = 1.0f; di = 0.0f; }

        void setRate(float omega)
        {
            dr = std::cos(omega);
            di = std::sin(omega);
        }

        void process()
        {
            // Complex multiply: (r + i*j) * (dr + di*j)
            float newR = r * dr - i * di;
            float newI = r * di + i * dr;
            r = newR;
            i = newI;

            // Renormalize periodically to prevent drift
            float mag = r * r + i * i;
            if (mag > 1.001f || mag < 0.999f)
            {
                float invMag = 1.0f / std::sqrt(mag);
                r *= invMag;
                i *= invMag;
            }
        }
    };

    // -------------------------------------------------------------------------
    // Simple 2nd-order Butterworth lowpass (reimplements SST BiquadFilter LP2B)
    // -------------------------------------------------------------------------
    struct Biquad2
    {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
        float a1 = 0.0f, a2 = 0.0f;
        float z1 = 0.0f, z2 = 0.0f;

        void reset() { z1 = z2 = 0.0f; }

        // Set LP2 Butterworth coefficients for given frequency and Q
        void setLP2(double freq, double Q, double sampleRate)
        {
            double w0 = twoPi * freq / sampleRate;
            double cosw = std::cos(w0);
            double sinw = std::sin(w0);
            double alpha = sinw / (2.0 * Q);

            double a0inv = 1.0 / (1.0 + alpha);
            b0 = (float)(((1.0 - cosw) * 0.5) * a0inv);
            b1 = (float)((1.0 - cosw) * a0inv);
            b2 = b0;
            a1 = (float)((-2.0 * cosw) * a0inv);
            a2 = (float)((1.0 - alpha) * a0inv);
        }

        void processBlock(float* data, int numSamples)
        {
            for (int i = 0; i < numSamples; ++i)
            {
                float in = data[i];
                float out = b0 * in + b1 * z1 + b2 * z2 - a1 * z1 - a2 * z2;
                // Actually transposed direct form II:
                out = b0 * in + z1;
                z1 = b1 * in - a1 * out + z2;
                z2 = b2 * in - a2 * out;
                data[i] = out;
            }
        }
    };

    // -------------------------------------------------------------------------
    // Waveshaper functions (reimplements SST waveshapers)
    // -------------------------------------------------------------------------
    static float applyWaveshaper(int type, float x)
    {
        switch (type)
        {
            case 0: // Soft clip (tanh)
                return std::tanh(x);

            case 1: // Hard clip
                return juce::jlimit(-1.0f, 1.0f, x);

            case 2: // Asymmetric soft clip
            {
                // SST asym: positive side compressed more than negative
                if (x >= 0.0f)
                    return std::tanh(x * 1.2f);
                else
                    return std::tanh(x * 0.8f) * 1.1f;
            }

            case 3: // Sine fold
                return std::sin(x);

            case 4: // Digital (quantize-like hard steps)
            {
                // SST digital: essentially a stairstep quantizer
                float q = 0.1f; // quantization step
                return std::floor(x / q + 0.5f) * q;
            }

            case 5: // OJD (asymmetric overdrive, inspired by BJT overdrive)
            {
                // Approximation of the OJD circuit
                if (x > 0.0f)
                    return 1.0f - std::exp(-x);
                else
                    return -1.0f + std::exp(x * 0.5f);
            }

            case 6: // Full-wave rectify
                return std::abs(x);

            case 7: // Fuzz soft
            {
                // Soft fuzz: tanh with asymmetric bias
                float biased = x + 0.1f;
                return std::tanh(biased * 1.5f) - std::tanh(0.15f); // DC-correct
            }
        }
        return std::tanh(x);
    }

    // -------------------------------------------------------------------------
    // Cubic interpolation delay read
    // -------------------------------------------------------------------------
    float readDelayCubic(int currentWritePos, float delaySamples) const
    {
        float readPos = (float)currentWritePos - delaySamples;
        int iPos = (int)std::floor(readPos);
        float frac = readPos - (float)iPos;

        float y0 = delayBuffer[(iPos - 1) & (maxDelayLength - 1)];
        float y1 = delayBuffer[(iPos)     & (maxDelayLength - 1)];
        float y2 = delayBuffer[(iPos + 1) & (maxDelayLength - 1)];
        float y3 = delayBuffer[(iPos + 2) & (maxDelayLength - 1)];

        // Cubic Hermite
        float c0 = y1;
        float c1 = 0.5f * (y2 - y0);
        float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);

        return ((c3 * frac + c2) * frac + c1) * frac + c0;
    }

    void updateFilterCoeffs()
    {
        // SST crossover: LP2B at 800 Hz, Q=0.707 (Butterworth)
        xoverFilter.setLP2(800.0, 0.707, sampleRate);
        // SST lowbass: LP2B at 200 Hz, Q=0.707
        lowbassFilter.setLP2(200.0, 0.707, sampleRate);
    }

    Params params;
    double sampleRate = 44100.0;
    int blockSize = 512;
    bool bypassed = false;
    bool isPrepared = false;

    // Delay line
    float delayBuffer[maxDelayLength] = {};
    int wpos = 0;

    // Quadrature oscillators
    QuadOsc hornLfo;
    QuadOsc rotorLfo;

    // Crossover filters
    Biquad2 xoverFilter;
    Biquad2 lowbassFilter;

    // Smoothed values
    float dLSmooth = 0.0f, dRSmooth = 0.0f;
    float hornAmpL = 1.0f, hornAmpR = 1.0f;
    float driveSmooth = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GuitarRotaryProcessor)
};
