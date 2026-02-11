#pragma once
// ==============================================================================
//  DelayDSP_Drift.h
//  OnStage - "Drift" Stereo Doubler Delay (based on Airwindows Doublelay, MIT)
//
//  Pitch-shifted stereo delay with Golden Ratio feedback crossfeed.
//  Outputs PURE WET signal. Dry/Wet mixing handled by DelayProcessor wrapper.
//  Controls: Detune, Delay L, Delay R, Feedback
// ==============================================================================

#include <cmath>
#include <cstdint>
#include <cstdlib>

class DelayDSP_Drift
{
public:
    static constexpr int NUM_PARAMS = 4;

    static const char* getParamName(int index)
    {
        static const char* names[] = { "Detune", "Delay L", "Delay R", "Feedbk" };
        return (index >= 0 && index < NUM_PARAMS) ? names[index] : "";
    }

    static const char* getParamSuffix(int index)
    {
        static const char* suffixes[] = { "", " sec", " sec", "" };
        return (index >= 0 && index < NUM_PARAMS) ? suffixes[index] : "";
    }

    static float getDefaultValue(int index)
    {
        static const float defaults[] = { 0.2f, 0.1f, 0.2f, 0.0f };
        return (index >= 0 && index < NUM_PARAMS) ? defaults[index] : 0.0f;
    }

    static void getParamRange(int index, double& min, double& max, double& step)
    {
        (void)index; min = 0.0; max = 1.0; step = 0.01;
    }

    void prepare(double sampleRate) { currentSampleRate = sampleRate; reset(); }

    void reset()
    {
        for (int i = 0; i < 48010; i++) { dL[i] = 0.0; dR[i] = 0.0; }
        dcount = 0;
        for (int i = 0; i < 5010; i++) { pL[i] = 0.0; pR[i] = 0.0; }
        for (int i = 0; i < 8; i++) {
            tempL[i] = 0.0; positionL[i] = 0.0; lastpositionL[i] = 0.0; trackingL[i] = 0.0;
            tempR[i] = 0.0; positionR[i] = 0.0; lastpositionR[i] = 0.0; trackingR[i] = 0.0;
        }
        gcountL = 0; lastcountL = 0; gcountR = 0; lastcountR = 0;
        prevwidth = 0;
        feedbackL_state = 0.0; feedbackR_state = 0.0;
        activeL = 0; bestspliceL = 4; activeR = 0; bestspliceR = 4;
        bestyetL = 1.0; bestyetR = 1.0;
        airPrevL = 0.0; airEvenL = 0.0; airOddL = 0.0; airFactorL = 0.0;
        airPrevR = 0.0; airEvenR = 0.0; airOddR = 0.0; airFactorR = 0.0;
        flip = false;
        for (int i = 0; i < 6; i++) { lastRefL[i] = 0.0; lastRefR[i] = 0.0; }
        cycle = 0;
        fpdL = 1; while (fpdL < 16386) fpdL = (uint32_t)(rand()) * (uint32_t)(rand());
        fpdR = 1; while (fpdR < 16386) fpdR = (uint32_t)(rand()) * (uint32_t)(rand());
    }

    // Outputs pure wet. Params: A=Detune, B=DelayL, C=DelayR, D=Feedback
    void process(float* leftChannel, float* rightChannel, int numSamples,
                 float A, float B, float C, float D)
    {
        double overallscale = 1.0;
        overallscale /= 44100.0;
        overallscale *= currentSampleRate;

        int cycleEnd = (int)floor(overallscale);
        if (cycleEnd < 1) cycleEnd = 1;
        if (cycleEnd > 4) cycleEnd = 4;
        if (cycle > cycleEnd - 1) cycle = cycleEnd - 1;
        double delayTrim = (currentSampleRate / cycleEnd) / 48001.0;
        if (delayTrim > 0.99999) delayTrim = 0.99999;
        if (delayTrim < 0.0) delayTrim = 0.0;

        double trim = (A * 2.0) - 1.0;
        trim *= fabs(trim); trim /= 40;
        double speedL = trim + 1.0;
        double speedR = (-trim) + 1.0;
        if (speedL < 0.0) speedL = 0.0;
        if (speedR < 0.0) speedR = 0.0;

        int delayLval = (int)(B * (int)(48000.0 * delayTrim));
        int delayRval = (int)(C * (int)(48000.0 * delayTrim));

        double adjust = 1100;
        int width = 2300;
        if (prevwidth != width) {
            positionL[0] = 0; positionL[1] = (int)(width / 3); positionL[2] = (int)((width / 3) * 2);
            positionL[3] = (int)(width / 5); positionL[4] = (int)((width / 5) * 2);
            positionL[5] = (int)((width / 5) * 3); positionL[6] = (int)((width / 5) * 4);
            positionL[7] = (int)(width / 2);
            positionR[0] = 0; positionR[1] = (int)(width / 3); positionR[2] = (int)((width / 3) * 2);
            positionR[3] = (int)(width / 5); positionR[4] = (int)((width / 5) * 2);
            positionR[5] = (int)((width / 5) * 3); positionR[6] = (int)((width / 5) * 4);
            positionR[7] = (int)(width / 2);
            prevwidth = width;
        }
        double feedbackDirect = D * 0.618033988749894848204586;
        double feedbackCross = D * (1.0 - 0.618033988749894848204586);

        for (int i = 0; i < numSamples; ++i)
        {
            double inputSampleL = (double)leftChannel[i];
            double inputSampleR = rightChannel ? (double)rightChannel[i] : inputSampleL;
            if (fabs(inputSampleL) < 1.18e-23) inputSampleL = fpdL * 1.18e-17;
            if (fabs(inputSampleR) < 1.18e-23) inputSampleR = fpdR * 1.18e-17;

            cycle++;
            if (cycle == cycleEnd) {
                // Air compensation
                airFactorL = airPrevL - inputSampleL;
                if (flip) { airEvenL += airFactorL; airOddL -= airFactorL; airFactorL = airEvenL; }
                else { airOddL += airFactorL; airEvenL -= airFactorL; airFactorL = airOddL; }
                airOddL = (airOddL - ((airOddL - airEvenL) / 256.0)) / 1.0001;
                airEvenL = (airEvenL - ((airEvenL - airOddL) / 256.0)) / 1.0001;
                airPrevL = inputSampleL; inputSampleL += airFactorL;

                airFactorR = airPrevR - inputSampleR;
                if (flip) { airEvenR += airFactorR; airOddR -= airFactorR; airFactorR = airEvenR; }
                else { airOddR += airFactorR; airEvenR -= airFactorR; airFactorR = airOddR; }
                airOddR = (airOddR - ((airOddR - airEvenR) / 256.0)) / 1.0001;
                airEvenR = (airEvenR - ((airEvenR - airOddR) / 256.0)) / 1.0001;
                airPrevR = inputSampleR; inputSampleR += airFactorR;
                flip = !flip;

                // Golden ratio crossfeed
                inputSampleL += feedbackL_state * feedbackDirect;
                inputSampleR += feedbackR_state * feedbackDirect;
                inputSampleL += feedbackR_state * feedbackCross;
                inputSampleR += feedbackL_state * feedbackCross;

                if (dcount < 1 || dcount > 48005) dcount = 48005;
                int count = dcount;
                dL[count] = inputSampleL; dR[count] = inputSampleR;

                inputSampleL = dL[count + delayLval - ((count + delayLval > 48005) ? 48005 : 0)];
                inputSampleR = dR[count + delayRval - ((count + delayRval > 48005) ? 48005 : 0)];

                dcount--;
                gcountL++; gcountR++;
                for (count = 0; count < 8; count++) { positionL[count] += speedL; positionR[count] += speedR; }

                int gplusL = gcountL + (int)adjust;
                int lastplusL = lastcountL + (int)adjust;
                if (gplusL > width) gplusL -= width;
                if (lastplusL > width) lastplusL -= width;
                int gplusR = gcountR + (int)adjust;
                int lastplusR = lastcountR + (int)adjust;
                if (gplusR > width) gplusR -= width;
                if (lastplusR > width) lastplusR -= width;

                if (trackingL[activeL] == 0.0) {
                    double posplusL = positionL[activeL] + adjust;
                    double lastposplusL = lastpositionL[activeL] + adjust;
                    if (posplusL > width) posplusL -= width;
                    if (lastposplusL > width) lastposplusL -= width;
                    if ((gplusL > positionL[activeL]) && (lastplusL < lastpositionL[activeL])) trackingL[activeL] = 1.0;
                    if ((posplusL > gcountL) && (lastposplusL < lastcountL)) trackingL[activeL] = 1.0;
                }
                if (trackingR[activeR] == 0.0) {
                    double posplusR = positionR[activeR] + adjust;
                    double lastposplusR = lastpositionR[activeR] + adjust;
                    if (posplusR > width) posplusR -= width;
                    if (lastposplusR > width) lastposplusR -= width;
                    if ((gplusR > positionR[activeR]) && (lastplusR < lastpositionR[activeR])) trackingR[activeR] = 1.0;
                    if ((posplusR > gcountR) && (lastposplusR < lastcountR)) trackingR[activeR] = 1.0;
                }

                for (count = 0; count < 8; count++) {
                    if (positionL[count] > width) positionL[count] -= width;
                    if (positionR[count] > width) positionR[count] -= width;
                    lastpositionL[count] = positionL[count];
                    lastpositionR[count] = positionR[count];
                }
                if (gcountL < 0 || gcountL > width) gcountL -= width;
                lastcountL = gcountL; int bcountL = gcountL;
                if (gcountR < 0 || gcountR > width) gcountR -= width;
                lastcountR = gcountR; int bcountR = gcountR;

                pL[bcountL + width] = pL[bcountL] = inputSampleL;
                pR[bcountR + width] = pR[bcountR] = inputSampleR;

                for (count = 0; count < 8; count++) {
                    int base = (int)floor(positionL[count]);
                    tempL[count] = (pL[base] * (1 - (positionL[count] - base)));
                    tempL[count] += pL[base + 1];
                    tempL[count] += (pL[base + 2] * (positionL[count] - base));
                    tempL[count] -= (((pL[base] - pL[base + 1]) - (pL[base + 1] - pL[base + 2])) / 50);
                    tempL[count] /= 2;
                    base = (int)floor(positionR[count]);
                    tempR[count] = (pR[base] * (1 - (positionR[count] - base)));
                    tempR[count] += pR[base + 1];
                    tempR[count] += (pR[base + 2] * (positionR[count] - base));
                    tempR[count] -= (((pR[base] - pR[base + 1]) - (pR[base + 1] - pR[base + 2])) / 50);
                    tempR[count] /= 2;
                }

                if (trackingL[activeL] > 0.0) {
                    double crossfade = sin(trackingL[bestspliceL] * 1.57);
                    inputSampleL = (tempL[activeL] * crossfade) + (tempL[bestspliceL] * (1.0 - crossfade));
                    for (count = 0; count < 8; count++) {
                        double depth = (0.5 - fabs(tempL[activeL] - tempL[count]));
                        if ((depth > 0) && (count != activeL)) { trackingL[count] -= (depth / adjust); bestspliceL = count; }
                    }
                    bestyetL = 1.0;
                    for (count = 0; count < 8; count++) {
                        if ((trackingL[count] < bestyetL) && (count != activeL)) { bestspliceL = count; bestyetL = trackingL[count]; }
                    }
                    if (trackingL[bestspliceL] < 0.0) {
                        for (count = 0; count < 8; count++) trackingL[count] = 1.0;
                        activeL = bestspliceL; trackingL[activeL] = 0.0;
                    }
                } else inputSampleL = tempL[activeL];

                if (trackingR[activeR] > 0.0) {
                    double crossfade = sin(trackingR[bestspliceR] * 1.57);
                    inputSampleR = (tempR[activeR] * crossfade) + (tempR[bestspliceR] * (1.0 - crossfade));
                    for (count = 0; count < 8; count++) {
                        double depth = (0.5 - fabs(tempR[activeR] - tempR[count]));
                        if ((depth > 0) && (count != activeR)) { trackingR[count] -= (depth / adjust); bestspliceR = count; }
                    }
                    bestyetR = 1.0;
                    for (count = 0; count < 8; count++) {
                        if ((trackingR[count] < bestyetR) && (count != activeR)) { bestspliceR = count; bestyetR = trackingR[count]; }
                    }
                    if (trackingR[bestspliceR] < 0.0) {
                        for (count = 0; count < 8; count++) trackingR[count] = 1.0;
                        activeR = bestspliceR; trackingR[activeR] = 0.0;
                    }
                } else inputSampleR = tempR[activeR];

                feedbackL_state = inputSampleL;
                feedbackR_state = inputSampleR;

                // Output ONLY the effect signal (pure wet) — no dry crossfade
                // Upsampling
                if (cycleEnd == 4) {
                    lastRefL[0] = lastRefL[4]; lastRefL[2] = (lastRefL[0] + inputSampleL) / 2;
                    lastRefL[1] = (lastRefL[0] + lastRefL[2]) / 2; lastRefL[3] = (lastRefL[2] + inputSampleL) / 2; lastRefL[4] = inputSampleL;
                    lastRefR[0] = lastRefR[4]; lastRefR[2] = (lastRefR[0] + inputSampleR) / 2;
                    lastRefR[1] = (lastRefR[0] + lastRefR[2]) / 2; lastRefR[3] = (lastRefR[2] + inputSampleR) / 2; lastRefR[4] = inputSampleR;
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

            fpdL ^= fpdL << 13; fpdL ^= fpdL >> 17; fpdL ^= fpdL << 5;
            fpdR ^= fpdR << 13; fpdR ^= fpdR >> 17; fpdR ^= fpdR << 5;

            leftChannel[i] = (float)inputSampleL;
            if (rightChannel) rightChannel[i] = (float)inputSampleR;
        }
    }

private:
    double currentSampleRate = 44100.0;
    double dL[48010] = {}; double dR[48010] = {};
    int dcount = 0;
    double pL[5010] = {}; double pR[5010] = {};
    int gcountL = 0, lastcountL = 0, gcountR = 0, lastcountR = 0;
    int prevwidth = 0;
    double trackingL[9] = {}, tempL[9] = {}, positionL[9] = {}, lastpositionL[9] = {};
    double trackingR[9] = {}, tempR[9] = {}, positionR[9] = {}, lastpositionR[9] = {};
    int activeL = 0, bestspliceL = 4, activeR = 0, bestspliceR = 4;
    double feedbackL_state = 0.0, feedbackR_state = 0.0;
    double bestyetL = 1.0, bestyetR = 1.0;
    double airPrevL = 0.0, airEvenL = 0.0, airOddL = 0.0, airFactorL = 0.0;
    double airPrevR = 0.0, airEvenR = 0.0, airOddR = 0.0, airFactorR = 0.0;
    bool flip = false;
    double lastRefL[7] = {}; double lastRefR[7] = {};
    int cycle = 0;
    uint32_t fpdL = 1, fpdR = 1;
};
