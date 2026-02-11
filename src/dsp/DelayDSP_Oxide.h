#pragma once
// ==============================================================================
//  DelayDSP_Oxide.h
//  OnStage - "Oxide" Tape Delay (based on Airwindows TapeDelay, MIT license)
//
//  Warm analog tape echo with prime-number tone shaping and feedback.
//  Outputs PURE WET signal. Dry/Wet mixing handled by DelayProcessor wrapper.
//  Controls: Delay, Feedback, Lean/Fat, Depth
// ==============================================================================

#include <cmath>
#include <cstdint>
#include <cstdlib>

class DelayDSP_Oxide
{
public:
    static constexpr int NUM_PARAMS = 4;

    static const char* getParamName(int index)
    {
        static const char* names[] = { "Delay", "Feedbk", "Lean/Fat", "Depth" };
        return (index >= 0 && index < NUM_PARAMS) ? names[index] : "";
    }

    static const char* getParamSuffix(int index)
    {
        static const char* suffixes[] = { "", "", "", " taps" };
        return (index >= 0 && index < NUM_PARAMS) ? suffixes[index] : "";
    }

    static float getDefaultValue(int index)
    {
        static const float defaults[] = { 0.5f, 0.0f, 1.0f, 0.0f };
        return (index >= 0 && index < NUM_PARAMS) ? defaults[index] : 0.0f;
    }

    static void getParamRange(int index, double& min, double& max, double& step)
    {
        (void)index; min = 0.0; max = 1.0; step = 0.01;
    }

    void prepare(double sampleRate) { currentSampleRate = sampleRate; reset(); }

    void reset()
    {
        for (int i = 0; i < 258; i++) { pL[i] = 0; pR[i] = 0; }
        for (int i = 0; i < 44100; i++) { dL[i] = 0.0; dR[i] = 0.0; }
        maxdelay = 0; delay = 0; gcount = 0; chase = 0;
        fpdL = 1; while (fpdL < 16386) fpdL = (uint32_t)(rand()) * (uint32_t)(rand());
        fpdR = 1; while (fpdR < 16386) fpdR = (uint32_t)(rand()) * (uint32_t)(rand());
    }

    // Outputs pure wet. Params: A=Delay, B=Feedback, C=Lean/Fat, D=Depth
    void process(float* leftChannel, float* rightChannel, int numSamples,
                 float A, float B, float C, float D)
    {
        int targetdelay = (int)(44000.0 * A);
        double feedback = (B * 1.3);
        double leanfat = ((C * 2.0) - 1.0);
        double fatwet = fabs(leanfat);
        int fatness = (int)floor((D * 29.0) + 3.0);
        int count;
        double storedelayL, storedelayR;
        double sumL = 0.0, sumR = 0.0;
        double floattotalL = 0.0, floattotalR = 0.0;
        int sumtotalL = 0, sumtotalR = 0;

        for (int i = 0; i < numSamples; ++i)
        {
            double inputSampleL = (double)leftChannel[i];
            double inputSampleR = rightChannel ? (double)rightChannel[i] : inputSampleL;
            if (fabs(inputSampleL) < 1.18e-23) inputSampleL = fpdL * 1.18e-17;
            if (fabs(inputSampleR) < 1.18e-23) inputSampleR = fpdR * 1.18e-17;

            if (gcount < 0 || gcount > 128) { gcount = 128; }
            count = gcount;
            if (delay < 0 || delay > maxdelay) { delay = maxdelay; }

            // Feedback loop: input + delayed * feedback (unchanged from original)
            sumL = inputSampleL + (dL[delay] * feedback);
            sumR = inputSampleR + (dR[delay] * feedback);
            pL[count + 128] = pL[count] = sumtotalL = (int)(sumL * 8388608.0);
            pR[count + 128] = pR[count] = sumtotalR = (int)(sumR * 8388608.0);

            switch (fatness)
            {
                case 32: sumtotalL += pL[count+127]; sumtotalR += pR[count+127];
                case 31: sumtotalL += pL[count+113]; sumtotalR += pR[count+113];
                case 30: sumtotalL += pL[count+109]; sumtotalR += pR[count+109];
                case 29: sumtotalL += pL[count+107]; sumtotalR += pR[count+107];
                case 28: sumtotalL += pL[count+103]; sumtotalR += pR[count+103];
                case 27: sumtotalL += pL[count+101]; sumtotalR += pR[count+101];
                case 26: sumtotalL += pL[count+97]; sumtotalR += pR[count+97];
                case 25: sumtotalL += pL[count+89]; sumtotalR += pR[count+89];
                case 24: sumtotalL += pL[count+83]; sumtotalR += pR[count+83];
                case 23: sumtotalL += pL[count+79]; sumtotalR += pR[count+79];
                case 22: sumtotalL += pL[count+73]; sumtotalR += pR[count+73];
                case 21: sumtotalL += pL[count+71]; sumtotalR += pR[count+71];
                case 20: sumtotalL += pL[count+67]; sumtotalR += pR[count+67];
                case 19: sumtotalL += pL[count+61]; sumtotalR += pR[count+61];
                case 18: sumtotalL += pL[count+59]; sumtotalR += pR[count+59];
                case 17: sumtotalL += pL[count+53]; sumtotalR += pR[count+53];
                case 16: sumtotalL += pL[count+47]; sumtotalR += pR[count+47];
                case 15: sumtotalL += pL[count+43]; sumtotalR += pR[count+43];
                case 14: sumtotalL += pL[count+41]; sumtotalR += pR[count+41];
                case 13: sumtotalL += pL[count+37]; sumtotalR += pR[count+37];
                case 12: sumtotalL += pL[count+31]; sumtotalR += pR[count+31];
                case 11: sumtotalL += pL[count+29]; sumtotalR += pR[count+29];
                case 10: sumtotalL += pL[count+23]; sumtotalR += pR[count+23];
                case 9: sumtotalL += pL[count+19]; sumtotalR += pR[count+19];
                case 8: sumtotalL += pL[count+17]; sumtotalR += pR[count+17];
                case 7: sumtotalL += pL[count+13]; sumtotalR += pR[count+13];
                case 6: sumtotalL += pL[count+11]; sumtotalR += pR[count+11];
                case 5: sumtotalL += pL[count+7]; sumtotalR += pR[count+7];
                case 4: sumtotalL += pL[count+5]; sumtotalR += pR[count+5];
                case 3: sumtotalL += pL[count+3]; sumtotalR += pR[count+3];
                case 2: sumtotalL += pL[count+2]; sumtotalR += pR[count+2];
                case 1: sumtotalL += pL[count+1]; sumtotalR += pR[count+1];
            }

            floattotalL = (double)(sumtotalL / fatness + 1);
            floattotalR = (double)(sumtotalR / fatness + 1);
            floattotalL /= 8388608.0; floattotalR /= 8388608.0;
            floattotalL *= fatwet; floattotalR *= fatwet;
            if (leanfat < 0) { storedelayL = sumL - floattotalL; storedelayR = sumR - floattotalR; }
            else { storedelayL = (sumL * (1 - fatwet)) + floattotalL; storedelayR = (sumR * (1 - fatwet)) + floattotalR; }

            chase += abs(maxdelay - targetdelay);
            if (chase > 9000) {
                if (maxdelay > targetdelay) {
                    dL[delay] = storedelayL; dR[delay] = storedelayR;
                    maxdelay -= 1; delay -= 1; if (delay < 0) { delay = maxdelay; }
                    dL[delay] = storedelayL; dR[delay] = storedelayR;
                }
                if (maxdelay < targetdelay) {
                    maxdelay += 1; delay += 1; if (delay > maxdelay) { delay = 0; }
                    dL[delay] = storedelayL; dR[delay] = storedelayR;
                }
                chase = 0;
            } else {
                dL[delay] = storedelayL; dR[delay] = storedelayR;
            }

            gcount--; delay--;
            if (delay < 0 || delay > maxdelay) { delay = maxdelay; }

            // Output ONLY the delayed signal (pure wet)
            leftChannel[i] = (float)dL[delay];
            if (rightChannel) rightChannel[i] = (float)dR[delay];

            fpdL ^= fpdL << 13; fpdL ^= fpdL >> 17; fpdL ^= fpdL << 5;
            fpdR ^= fpdR << 13; fpdR ^= fpdR >> 17; fpdR ^= fpdR << 5;
        }
    }

private:
    double currentSampleRate = 44100.0;
    int pL[258] = {}; int pR[258] = {};
    double dL[44100] = {}; double dR[44100] = {};
    int gcount = 0, delay = 0, maxdelay = 0, chase = 0;
    uint32_t fpdL = 1, fpdR = 1;
};
