#pragma once
// ==============================================================================
//  DelayDSP_Warp.h
//  OnStage - "Warp" Pitch Delay (based on Airwindows PitchDelay, MIT license)
//
//  Tape-speed delay with pitch shifting, bandpass filtering and vibrato.
//  Outputs PURE WET signal. Dry/Wet mixing handled by DelayProcessor wrapper.
//  Controls: Time, Regen, Freq, Reso, Pitch
// ==============================================================================

#include <cmath>
#include <cstdint>
#include <cstdlib>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

class DelayDSP_Warp
{
public:
    static constexpr int NUM_PARAMS = 5;

    static const char* getParamName(int index)
    {
        static const char* names[] = { "Time", "Regen", "Freq", "Reso", "Pitch" };
        return (index >= 0 && index < NUM_PARAMS) ? names[index] : "";
    }

    static const char* getParamSuffix(int index) { (void)index; return ""; }

    static float getDefaultValue(int index)
    {
        static const float defaults[] = { 1.0f, 0.0f, 0.5f, 0.0f, 0.5f };
        return (index >= 0 && index < NUM_PARAMS) ? defaults[index] : 0.0f;
    }

    static void getParamRange(int index, double& min, double& max, double& step)
    {
        (void)index; min = 0.0; max = 1.0; step = 0.01;
    }

    void prepare(double sampleRate) { currentSampleRate = sampleRate; reset(); }

    void reset()
    {
        for (int x = 0; x < 88210; x++) { dL[x] = 0.0; dR[x] = 0.0; }
        prevSampleL = 0.0; regenSampleL = 0.0; delayL = 0.0; sweepL = 0.0;
        prevSampleR = 0.0; regenSampleR = 0.0; delayR = 0.0; sweepR = 0.0;
        for (int x = 0; x < 9; x++) {
            regenFilterL[x] = 0.0; outFilterL[x] = 0.0; lastRefL[x] = 0.0;
            regenFilterR[x] = 0.0; outFilterR[x] = 0.0; lastRefR[x] = 0.0;
        }
        cycle = 0;
        fpdL = 1; while (fpdL < 16386) fpdL = (uint32_t)(rand()) * (uint32_t)(rand());
        fpdR = 1; while (fpdR < 16386) fpdR = (uint32_t)(rand()) * (uint32_t)(rand());
    }

    // Outputs pure wet. Params: A=Time, B=Regen, C=Freq, D=Reso, E=Pitch
    void process(float* leftChannel, float* rightChannel, int numSamples,
                 float A, float B, float C, float D, float E)
    {
        double overallscale = 1.0;
        overallscale /= 44100.0;
        overallscale *= currentSampleRate;

        int cycleEnd = (int)floor(overallscale);
        if (cycleEnd < 1) cycleEnd = 1;
        if (cycleEnd > 4) cycleEnd = 4;
        if (cycle > cycleEnd - 1) cycle = cycleEnd - 1;

        double baseSpeed = (pow((double)A, 4.0) * 20.0) + 1.0;
        double feedbackL_val = B * (3.0 - fabs(regenSampleL * 2.0));
        double feedbackR_val = B * (3.0 - fabs(regenSampleR * 2.0));

        regenFilterL[0] = regenFilterR[0] = ((pow((double)C, 3.0) * 0.4) + 0.0001);
        regenFilterL[1] = regenFilterR[1] = pow((double)D, 2.0) + 0.01;
        double K = tan(M_PI * regenFilterR[0]);
        double norm = 1.0 / (1.0 + K / regenFilterR[1] + K * K);
        regenFilterL[2] = regenFilterR[2] = K / regenFilterR[1] * norm;
        regenFilterL[4] = regenFilterR[4] = -regenFilterR[2];
        regenFilterL[5] = regenFilterR[5] = 2.0 * (K * K - 1.0) * norm;
        regenFilterL[6] = regenFilterR[6] = (1.0 - K / regenFilterR[1] + K * K) * norm;

        outFilterL[0] = outFilterR[0] = regenFilterR[0];
        outFilterL[1] = outFilterR[1] = regenFilterR[1] * 1.618033988749894848204586;
        K = tan(M_PI * outFilterR[0]);
        norm = 1.0 / (1.0 + K / outFilterR[1] + K * K);
        outFilterL[2] = outFilterR[2] = K / outFilterR[1] * norm;
        outFilterL[4] = outFilterR[4] = -outFilterR[2];
        outFilterL[5] = outFilterR[5] = 2.0 * (K * K - 1.0) * norm;
        outFilterL[6] = outFilterR[6] = (1.0 - K / outFilterR[1] + K * K) * norm;

        double vibSpeed = (E - 0.5) * 61.8;
        double vibDepth = (fabs(vibSpeed) * 20.0 * baseSpeed) + 1.0;

        for (int i = 0; i < numSamples; ++i)
        {
            double inputSampleL = (double)leftChannel[i];
            double inputSampleR = rightChannel ? (double)rightChannel[i] : inputSampleL;
            if (fabs(inputSampleL) < 1.18e-23) inputSampleL = fpdL * 1.18e-17;
            if (fabs(inputSampleR) < 1.18e-23) inputSampleR = fpdR * 1.18e-17;

            cycle++;
            if (cycle == cycleEnd) {
                double speedL_val = baseSpeed;
                double speedR_val = baseSpeed;

                // Left channel
                int pos = (int)floor(delayL);
                double newSample = inputSampleL + (regenSampleL * feedbackL_val);
                double tempSample = (newSample * regenFilterL[2]) + regenFilterL[7];
                regenFilterL[7] = -(tempSample * regenFilterL[5]) + regenFilterL[8];
                regenFilterL[8] = (newSample * regenFilterL[4]) - (tempSample * regenFilterL[6]);
                newSample = tempSample;

                delayL -= speedL_val; if (delayL < 0) delayL += 88200.0;
                double increment = (newSample - prevSampleL) / speedL_val;
                dL[pos] = prevSampleL;
                while (pos != (int)floor(delayL)) {
                    dL[pos] = prevSampleL; prevSampleL += increment;
                    pos--; if (pos < 0) pos += 88200;
                }
                prevSampleL = newSample;

                sweepL += (0.0001 * vibSpeed);
                if (sweepL < 0.0) sweepL += 6.283185307179586;
                if (sweepL > 6.283185307179586) sweepL -= 6.283185307179586;
                double sweepOffsetL = sweepL + M_PI;
                if (sweepOffsetL > 6.283185307179586) sweepOffsetL -= 6.283185307179586;
                double newTapA = delayL - (sweepL * vibDepth); if (newTapA < 0) newTapA += 88200.0;
                double newTapB = delayL - (sweepOffsetL * vibDepth); if (newTapB < 0) newTapB += 88200.0;
                double tapAmplitudeA = (sin(sweepL + (M_PI * 1.5)) + 1.0) * 0.25;
                double tapAmplitudeB = (sin(sweepOffsetL + (M_PI * 1.5)) + 1.0) * 0.25;

                pos = (int)floor(newTapA); inputSampleL = dL[pos] * tapAmplitudeA;
                pos = (int)floor(newTapB); inputSampleL += dL[pos] * tapAmplitudeB;
                regenSampleL = sin(inputSampleL);

                tempSample = (inputSampleL * outFilterL[2]) + outFilterL[7];
                outFilterL[7] = -(tempSample * outFilterL[5]) + outFilterL[8];
                outFilterL[8] = (inputSampleL * outFilterL[4]) - (tempSample * outFilterL[6]);
                inputSampleL = tempSample;

                // Right channel
                pos = (int)floor(delayR);
                newSample = inputSampleR + (regenSampleR * feedbackR_val);
                tempSample = (newSample * regenFilterR[2]) + regenFilterR[7];
                regenFilterR[7] = -(tempSample * regenFilterR[5]) + regenFilterR[8];
                regenFilterR[8] = (newSample * regenFilterR[4]) - (tempSample * regenFilterR[6]);
                newSample = tempSample;

                delayR -= speedR_val; if (delayR < 0) delayR += 88200.0;
                increment = (newSample - prevSampleR) / speedR_val;
                dR[pos] = prevSampleR;
                while (pos != (int)floor(delayR)) {
                    dR[pos] = prevSampleR; prevSampleR += increment;
                    pos--; if (pos < 0) pos += 88200;
                }
                prevSampleR = newSample;

                sweepR += (0.0001 * vibSpeed);
                if (sweepR < 0.0) sweepR += 6.283185307179586;
                if (sweepR > 6.283185307179586) sweepR -= 6.283185307179586;
                double sweepOffsetR = sweepR + M_PI;
                if (sweepOffsetR > 6.283185307179586) sweepOffsetR -= 6.283185307179586;
                newTapA = delayR - (sweepR * vibDepth); if (newTapA < 0) newTapA += 88200.0;
                newTapB = delayR - (sweepOffsetR * vibDepth); if (newTapB < 0) newTapB += 88200.0;
                tapAmplitudeA = (sin(sweepR + (M_PI * 1.5)) + 1.0) * 0.25;
                tapAmplitudeB = (sin(sweepOffsetR + (M_PI * 1.5)) + 1.0) * 0.25;

                pos = (int)floor(newTapA); inputSampleR = dR[pos] * tapAmplitudeA;
                pos = (int)floor(newTapB); inputSampleR += dR[pos] * tapAmplitudeB;
                regenSampleR = sin(inputSampleR);

                tempSample = (inputSampleR * outFilterR[2]) + outFilterR[7];
                outFilterR[7] = -(tempSample * outFilterR[5]) + outFilterR[8];
                outFilterR[8] = (inputSampleR * outFilterR[4]) - (tempSample * outFilterR[6]);
                inputSampleR = tempSample;

                // Upsampling interpolation
                if (cycleEnd == 4) {
                    lastRefL[0] = lastRefL[4]; lastRefL[2] = (lastRefL[0] + inputSampleL) / 2;
                    lastRefL[1] = (lastRefL[0] + lastRefL[2]) / 2; lastRefL[3] = (lastRefL[2] + inputSampleL) / 2;
                    lastRefL[4] = inputSampleL;
                    lastRefR[0] = lastRefR[4]; lastRefR[2] = (lastRefR[0] + inputSampleR) / 2;
                    lastRefR[1] = (lastRefR[0] + lastRefR[2]) / 2; lastRefR[3] = (lastRefR[2] + inputSampleR) / 2;
                    lastRefR[4] = inputSampleR;
                }
                if (cycleEnd == 3) {
                    lastRefL[0] = lastRefL[3]; lastRefL[2] = (lastRefL[0] + lastRefL[0] + inputSampleL) / 3;
                    lastRefL[1] = (lastRefL[0] + inputSampleL + inputSampleL) / 3; lastRefL[3] = inputSampleL;
                    lastRefR[0] = lastRefR[3]; lastRefR[2] = (lastRefR[0] + lastRefR[0] + inputSampleR) / 3;
                    lastRefR[1] = (lastRefR[0] + inputSampleR + inputSampleR) / 3; lastRefR[3] = inputSampleR;
                }
                if (cycleEnd == 2) {
                    lastRefL[0] = lastRefL[2]; lastRefL[1] = (lastRefL[0] + inputSampleL) / 2; lastRefL[2] = inputSampleL;
                    lastRefR[0] = lastRefR[2]; lastRefR[1] = (lastRefR[0] + inputSampleR) / 2; lastRefR[2] = inputSampleR;
                }
                if (cycleEnd == 1) { lastRefL[0] = inputSampleL; lastRefR[0] = inputSampleR; }
                cycle = 0;
                inputSampleL = lastRefL[cycle];
                inputSampleR = lastRefR[cycle];
            } else {
                inputSampleL = lastRefL[cycle];
                inputSampleR = lastRefR[cycle];
            }

            // Multi-pole average
            switch (cycleEnd) {
                case 4:
                    lastRefL[8] = inputSampleL; inputSampleL = (inputSampleL + lastRefL[7]) * 0.5; lastRefL[7] = lastRefL[8];
                    lastRefR[8] = inputSampleR; inputSampleR = (inputSampleR + lastRefR[7]) * 0.5; lastRefR[7] = lastRefR[8];
                case 3:
                    lastRefL[8] = inputSampleL; inputSampleL = (inputSampleL + lastRefL[6]) * 0.5; lastRefL[6] = lastRefL[8];
                    lastRefR[8] = inputSampleR; inputSampleR = (inputSampleR + lastRefR[6]) * 0.5; lastRefR[6] = lastRefR[8];
                case 2:
                    lastRefL[8] = inputSampleL; inputSampleL = (inputSampleL + lastRefL[5]) * 0.5; lastRefL[5] = lastRefL[8];
                    lastRefR[8] = inputSampleR; inputSampleR = (inputSampleR + lastRefR[5]) * 0.5; lastRefR[5] = lastRefR[8];
                case 1: break;
            }

            // Output ONLY wet (effect) signal — no dry mixed in
            fpdL ^= fpdL << 13; fpdL ^= fpdL >> 17; fpdL ^= fpdL << 5;
            fpdR ^= fpdR << 13; fpdR ^= fpdR >> 17; fpdR ^= fpdR << 5;

            leftChannel[i] = (float)inputSampleL;
            if (rightChannel) rightChannel[i] = (float)inputSampleR;
        }
    }

private:
    double currentSampleRate = 44100.0;
    double dL[88211] = {}; double dR[88211] = {};
    double prevSampleL = 0.0, regenSampleL = 0.0, delayL = 0.0, sweepL = 0.0;
    double prevSampleR = 0.0, regenSampleR = 0.0, delayR = 0.0, sweepR = 0.0;
    double regenFilterL[9] = {}, outFilterL[9] = {}, lastRefL[10] = {};
    double regenFilterR[9] = {}, outFilterR[9] = {}, lastRefR[10] = {};
    int cycle = 0;
    uint32_t fpdL = 1, fpdR = 1;
};
