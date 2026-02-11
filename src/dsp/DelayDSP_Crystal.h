#pragma once
// ==============================================================================
//  DelayDSP_Crystal.h
//  OnStage - "Crystal" Pure Echo (based on Airwindows PurestEcho, MIT license)
//
//  Ultra-clean 4-tap delay with precise sub-sample timing. No feedback.
//  Outputs PURE WET signal (taps only). Dry/Wet mixing by DelayProcessor wrapper.
//  Controls: Time, Tap 1, Tap 2, Tap 3, Tap 4
// ==============================================================================

#include <cmath>
#include <cstdint>
#include <cstdlib>

class DelayDSP_Crystal
{
public:
    static constexpr int NUM_PARAMS = 5;

    static const char* getParamName(int index)
    {
        static const char* names[] = { "Time", "Tap 1", "Tap 2", "Tap 3", "Tap 4" };
        return (index >= 0 && index < NUM_PARAMS) ? names[index] : "";
    }

    static const char* getParamSuffix(int index) { (void)index; return ""; }

    static float getDefaultValue(int index)
    {
        static const float defaults[] = { 1.0f, 1.0f, 0.0f, 0.0f, 0.0f };
        return (index >= 0 && index < NUM_PARAMS) ? defaults[index] : 0.0f;
    }

    static void getParamRange(int index, double& min, double& max, double& step)
    {
        (void)index; min = 0.0; max = 1.0; step = 0.01;
    }

    void prepare(double sampleRate) { currentSampleRate = sampleRate; reset(); }

    void reset()
    {
        for (int i = 0; i < totalsamples; i++) { dL[i] = 0.0; dR[i] = 0.0; }
        gcount = 0;
        fpdL = 1; while (fpdL < 16386) fpdL = (uint32_t)(rand()) * (uint32_t)(rand());
        fpdR = 1; while (fpdR < 16386) fpdR = (uint32_t)(rand()) * (uint32_t)(rand());
    }

    // Outputs pure wet (taps only). Params: A=Time, B=Tap1, C=Tap2, D=Tap3, E=Tap4
    void process(float* leftChannel, float* rightChannel, int numSamples,
                 float A, float B, float C, float D, float E)
    {
        int loopLimit = (int)(totalsamples * 0.499);

        double time = pow((double)A, 2.0) * 0.999;
        double tap1 = (double)B;
        double tap2 = (double)C;
        double tap3 = (double)D;
        double tap4 = (double)E;

        // Original uses gainTrim to auto-level; for pure wet output we still
        // need tapsTrim for the delay buffer write level, but we output
        // only the echo taps without any dry signal.
        double tapSum = tap1 + tap2 + tap3 + tap4;
        if (tapSum < 0.0001) tapSum = 0.0001; // avoid silence when all taps 0
        double tapsTrim = 0.5 / (1.0 + tapSum);
        // Note: tapsTrim scales the buffer write to prevent clipping from
        // multiple taps summing. This preserves the original's gain structure
        // for the wet path.

        int position1 = (int)(loopLimit * time * 0.25);
        int position2 = (int)(loopLimit * time * 0.5);
        int position3 = (int)(loopLimit * time * 0.75);
        int position4 = (int)(loopLimit * time);

        double volAfter1 = (loopLimit * time * 0.25) - position1;
        double volAfter2 = (loopLimit * time * 0.5) - position2;
        double volAfter3 = (loopLimit * time * 0.75) - position3;
        double volAfter4 = (loopLimit * time) - position4;

        double volBefore1 = (1.0 - volAfter1) * tap1;
        double volBefore2 = (1.0 - volAfter2) * tap2;
        double volBefore3 = (1.0 - volAfter3) * tap3;
        double volBefore4 = (1.0 - volAfter4) * tap4;

        volAfter1 *= tap1; volAfter2 *= tap2;
        volAfter3 *= tap3; volAfter4 *= tap4;

        int oneBefore1 = position1 - 1; if (oneBefore1 < 0) oneBefore1 = 0;
        int oneBefore2 = position2 - 1; if (oneBefore2 < 0) oneBefore2 = 0;
        int oneBefore3 = position3 - 1; if (oneBefore3 < 0) oneBefore3 = 0;
        int oneBefore4 = position4 - 1; if (oneBefore4 < 0) oneBefore4 = 0;
        int oneAfter1 = position1 + 1;
        int oneAfter2 = position2 + 1;
        int oneAfter3 = position3 + 1;
        int oneAfter4 = position4 + 1;

        for (int i = 0; i < numSamples; ++i)
        {
            double inputSampleL = (double)leftChannel[i];
            double inputSampleR = rightChannel ? (double)rightChannel[i] : inputSampleL;
            if (fabs(inputSampleL) < 1.18e-23) inputSampleL = fpdL * 1.18e-17;
            if (fabs(inputSampleR) < 1.18e-23) inputSampleR = fpdR * 1.18e-17;

            if (gcount < 0 || gcount > loopLimit) gcount = loopLimit;
            dL[gcount + loopLimit] = dL[gcount] = inputSampleL * tapsTrim;
            dR[gcount + loopLimit] = dR[gcount] = inputSampleR * tapsTrim;

            double delaysBufferL = 0.0, delaysBufferR = 0.0;

            // Interpolated samples
            delaysBufferL += (dL[gcount + oneBefore4] * volBefore4);
            delaysBufferL += (dL[gcount + oneAfter4] * volAfter4);
            delaysBufferL += (dL[gcount + oneBefore3] * volBefore3);
            delaysBufferL += (dL[gcount + oneAfter3] * volAfter3);
            delaysBufferL += (dL[gcount + oneBefore2] * volBefore2);
            delaysBufferL += (dL[gcount + oneAfter2] * volAfter2);
            delaysBufferL += (dL[gcount + oneBefore1] * volBefore1);
            delaysBufferL += (dL[gcount + oneAfter1] * volAfter1);

            delaysBufferR += (dR[gcount + oneBefore4] * volBefore4);
            delaysBufferR += (dR[gcount + oneAfter4] * volAfter4);
            delaysBufferR += (dR[gcount + oneBefore3] * volBefore3);
            delaysBufferR += (dR[gcount + oneAfter3] * volAfter3);
            delaysBufferR += (dR[gcount + oneBefore2] * volBefore2);
            delaysBufferR += (dR[gcount + oneAfter2] * volAfter2);
            delaysBufferR += (dR[gcount + oneBefore1] * volBefore1);
            delaysBufferR += (dR[gcount + oneAfter1] * volAfter1);

            // Primary tap samples
            delaysBufferL += (dL[gcount + position4] * tap4);
            delaysBufferL += (dL[gcount + position3] * tap3);
            delaysBufferL += (dL[gcount + position2] * tap2);
            delaysBufferL += (dL[gcount + position1] * tap1);

            delaysBufferR += (dR[gcount + position4] * tap4);
            delaysBufferR += (dR[gcount + position3] * tap3);
            delaysBufferR += (dR[gcount + position2] * tap2);
            delaysBufferR += (dR[gcount + position1] * tap1);

            // Output ONLY the echo taps (pure wet) — no dry
            leftChannel[i] = (float)delaysBufferL;
            if (rightChannel) rightChannel[i] = (float)delaysBufferR;

            gcount--;

            fpdL ^= fpdL << 13; fpdL ^= fpdL >> 17; fpdL ^= fpdL << 5;
            fpdR ^= fpdR << 13; fpdR ^= fpdR >> 17; fpdR ^= fpdR << 5;
        }
    }

private:
    double currentSampleRate = 44100.0;
    static constexpr int totalsamples = 65535;
    double dL[65535] = {}; double dR[65535] = {};
    int gcount = 0;
    uint32_t fpdL = 1, fpdR = 1;
};
