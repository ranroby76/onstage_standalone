// ==============================================================================
//  RoomReverbProcessor.h
//  OnStage — Room Reverb
//
//  Based on Airwindows Verbity2 by Chris Johnson (MIT License).
//  5-bank feedforward 5×5 Householder matrix reverb (Bricasti-style).
//  25 delay lines arranged in 5 stages, stereo cross-modulation,
//  Chrome Oxide tape-style softening of feedback.
//
//  Parameters (all 0-1):
//    RmSize   (A) — room size, controls all delay lengths
//    Sustain  (B) — feedback/regen amount (reverb tail length)
//    Mulch    (C) — tone: lowpass/highpass balance + feedback softening
//    Wetness  (D) — dry/wet mix (submix style: 50% = both full volume)
//
//  Copyright (c) airwindows, MIT License. OnStage integration by Rob.
// ==============================================================================

#pragma once
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

class RoomReverbProcessor
{
public:
    struct Params
    {
        float roomSize = 0.5f;   // A: 0-1 room size
        float sustain  = 0.5f;   // B: 0-1 reverb tail length
        float mulch    = 0.5f;   // C: 0-1 tone/darkness
        float wetness  = 1.0f;   // D: 0-1 dry/wet

        bool operator== (const Params& o) const
        {
            return roomSize == o.roomSize && sustain == o.sustain
                && mulch == o.mulch && wetness == o.wetness;
        }
        bool operator!= (const Params& o) const { return !(*this == o); }
    };

    RoomReverbProcessor() = default;

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        currentSampleRate = spec.sampleRate;
        resetState();
    }

    void reset()
    {
        resetState();
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        if (bypassed) return;
        const auto p = params.load();
        const int numSamples = buffer.getNumSamples();
        if (buffer.getNumChannels() < 2) return;
        float peakLevel = 0.0f;

        float* outL = buffer.getWritePointer (0);
        float* outR = buffer.getWritePointer (1);

        double overallscale = 1.0;
        overallscale /= 44100.0;
        overallscale *= currentSampleRate;
        int cycleEnd = (int) std::floor (overallscale);
        if (cycleEnd < 1) cycleEnd = 1;
        if (cycleEnd > 4) cycleEnd = 4;
        if (cycle > cycleEnd - 1) cycle = cycleEnd - 1;

        // Parameter mapping — faithful to Verbity2
        double size = (std::pow ((double) p.roomSize, 2.0) * 0.9) + 0.1;
        double regen = (1.0 - std::pow (1.0 - (double) p.sustain, 3.0)) * 0.00032;
        double mulchSetting = (double) p.mulch;
        double lowpass = (1.0 - (mulchSetting * 0.75)) / std::sqrt (overallscale);
        double highpass = (0.007 + (mulchSetting * 0.022)) / std::sqrt (overallscale);
        double interpolateMax = 0.07 + (mulchSetting * 0.4);
        double wet = (double) p.wetness * 2.0;
        double dry = 2.0 - wet;
        if (wet > 1.0) wet = 1.0;
        if (wet < 0.0) wet = 0.0;
        if (dry > 1.0) dry = 1.0;
        if (dry < 0.0) dry = 0.0;

        // Set delay lengths
        delayA = (int)(5003.0 * size); delayF = (int)(4951.0 * size);
        delayK = (int)(4919.0 * size); delayP = (int)(4799.0 * size);
        delayU = (int)(4751.0 * size);

        delayB = (int)(4349.0 * size); delayG = (int)(4157.0 * size);
        delayL = (int)(3929.0 * size); delayQ = (int)(3529.0 * size);
        delayV = (int)(3329.0 * size);

        delayC = (int)(3323.0 * size); delayH = (int)(2791.0 * size);
        delayM = (int)(2767.0 * size); delayR = (int)(2389.0 * size);
        delayW = (int)(2347.0 * size);

        delayD = (int)(2141.0 * size); delayI = (int)(1811.0 * size);
        delayN = (int)(1733.0 * size); delayS = (int)(1171.0 * size);
        delayX = (int)(787.0 * size);

        delayE = (int)(677.0 * size);  delayJ = (int)(643.0 * size);
        delayO = (int)(439.0 * size);  delayT = (int)(349.0 * size);
        delayY = (int)(281.0 * size);

        for (int i = 0; i < numSamples; ++i)
        {
            double inputSampleL = (double) outL[i];
            double inputSampleR = (double) outR[i];
            if (std::fabs (inputSampleL) < 1.18e-23) inputSampleL = fpdL * 1.18e-17;
            if (std::fabs (inputSampleR) < 1.18e-23) inputSampleR = fpdR * 1.18e-17;
            double drySampleL = inputSampleL;
            double drySampleR = inputSampleR;

            // Highpass filter
            if (std::fabs (iirAL) < 1.18e-37) iirAL = 0.0;
            iirAL = (iirAL * (1.0 - highpass)) + (inputSampleL * highpass);
            inputSampleL -= iirAL;
            if (std::fabs (iirAR) < 1.18e-37) iirAR = 0.0;
            iirAR = (iirAR * (1.0 - highpass)) + (inputSampleR * highpass);
            inputSampleR -= iirAR;

            // Chrome Oxide randomized interpolation
            double interpolateL = interpolateMax + (interpolateMax * ((double) fpdL / (double) UINT32_MAX));
            double interpolateR = interpolateMax + (interpolateMax * ((double) fpdR / (double) UINT32_MAX));

            cycle++;
            if (cycle == cycleEnd)
            {
                // Soften feedback
                feedbackAL = (feedbackAL * (1.0 - interpolateL)) + (previousAL * interpolateL); previousAL = feedbackAL;
                feedbackBL = (feedbackBL * (1.0 - interpolateL)) + (previousBL * interpolateL); previousBL = feedbackBL;
                feedbackCL = (feedbackCL * (1.0 - interpolateL)) + (previousCL * interpolateL); previousCL = feedbackCL;
                feedbackDL = (feedbackDL * (1.0 - interpolateL)) + (previousDL * interpolateL); previousDL = feedbackDL;
                feedbackEL = (feedbackEL * (1.0 - interpolateL)) + (previousEL * interpolateL); previousEL = feedbackEL;
                feedbackAR = (feedbackAR * (1.0 - interpolateR)) + (previousAR * interpolateR); previousAR = feedbackAR;
                feedbackBR = (feedbackBR * (1.0 - interpolateR)) + (previousBR * interpolateR); previousBR = feedbackBR;
                feedbackCR = (feedbackCR * (1.0 - interpolateR)) + (previousCR * interpolateR); previousCR = feedbackCR;
                feedbackDR = (feedbackDR * (1.0 - interpolateR)) + (previousDR * interpolateR); previousDR = feedbackDR;
                feedbackER = (feedbackER * (1.0 - interpolateR)) + (previousER * interpolateR); previousER = feedbackER;

                // -------- Bank 1: input + feedback into A-E delay lines
                aAL[countA] = inputSampleL + (feedbackAL * (regen * (1.0 - std::fabs (feedbackAL * regen))));
                aBL[countB] = inputSampleL + (feedbackBL * (regen * (1.0 - std::fabs (feedbackBL * regen))));
                aCL[countC] = inputSampleL + (feedbackCL * (regen * (1.0 - std::fabs (feedbackCL * regen))));
                aDL[countD] = inputSampleL + (feedbackDL * (regen * (1.0 - std::fabs (feedbackDL * regen))));
                aEL[countE] = inputSampleL + (feedbackEL * (regen * (1.0 - std::fabs (feedbackEL * regen))));

                aAR[countA] = inputSampleR + (feedbackAR * (regen * (1.0 - std::fabs (feedbackAR * regen))));
                aBR[countB] = inputSampleR + (feedbackBR * (regen * (1.0 - std::fabs (feedbackBR * regen))));
                aCR[countC] = inputSampleR + (feedbackCR * (regen * (1.0 - std::fabs (feedbackCR * regen))));
                aDR[countD] = inputSampleR + (feedbackDR * (regen * (1.0 - std::fabs (feedbackDR * regen))));
                aER[countE] = inputSampleR + (feedbackER * (regen * (1.0 - std::fabs (feedbackER * regen))));

                countA++; if (countA < 0 || countA > delayA) countA = 0;
                countB++; if (countB < 0 || countB > delayB) countB = 0;
                countC++; if (countC < 0 || countC > delayC) countC = 0;
                countD++; if (countD < 0 || countD > delayD) countD = 0;
                countE++; if (countE < 0 || countE > delayE) countE = 0;

                double outAL = aAL[countA - ((countA > delayA) ? delayA + 1 : 0)];
                double outBL = aBL[countB - ((countB > delayB) ? delayB + 1 : 0)];
                double outCL = aCL[countC - ((countC > delayC) ? delayC + 1 : 0)];
                double outDL = aDL[countD - ((countD > delayD) ? delayD + 1 : 0)];
                double outEL = aEL[countE - ((countE > delayE) ? delayE + 1 : 0)];

                double outAR = aAR[countA - ((countA > delayA) ? delayA + 1 : 0)];
                double outBR = aBR[countB - ((countB > delayB) ? delayB + 1 : 0)];
                double outCR = aCR[countC - ((countC > delayC) ? delayC + 1 : 0)];
                double outDR = aDR[countD - ((countD > delayD) ? delayD + 1 : 0)];
                double outER = aER[countE - ((countE > delayE) ? delayE + 1 : 0)];

                // -------- Bank 2: 5×5 Householder into F-J
                aFL[countF] = ((outAL * 3.0) - ((outBL + outCL + outDL + outEL) * 2.0));
                aGL[countG] = ((outBL * 3.0) - ((outAL + outCL + outDL + outEL) * 2.0));
                aHL[countH] = ((outCL * 3.0) - ((outAL + outBL + outDL + outEL) * 2.0));
                aIL[countI] = ((outDL * 3.0) - ((outAL + outBL + outCL + outEL) * 2.0));
                aJL[countJ] = ((outEL * 3.0) - ((outAL + outBL + outCL + outDL) * 2.0));

                aFR[countF] = ((outAR * 3.0) - ((outBR + outCR + outDR + outER) * 2.0));
                aGR[countG] = ((outBR * 3.0) - ((outAR + outCR + outDR + outER) * 2.0));
                aHR[countH] = ((outCR * 3.0) - ((outAR + outBR + outDR + outER) * 2.0));
                aIR[countI] = ((outDR * 3.0) - ((outAR + outBR + outCR + outER) * 2.0));
                aJR[countJ] = ((outER * 3.0) - ((outAR + outBR + outCR + outDR) * 2.0));

                countF++; if (countF < 0 || countF > delayF) countF = 0;
                countG++; if (countG < 0 || countG > delayG) countG = 0;
                countH++; if (countH < 0 || countH > delayH) countH = 0;
                countI++; if (countI < 0 || countI > delayI) countI = 0;
                countJ++; if (countJ < 0 || countJ > delayJ) countJ = 0;

                double outFL = aFL[countF - ((countF > delayF) ? delayF + 1 : 0)];
                double outGL = aGL[countG - ((countG > delayG) ? delayG + 1 : 0)];
                double outHL = aHL[countH - ((countH > delayH) ? delayH + 1 : 0)];
                double outIL = aIL[countI - ((countI > delayI) ? delayI + 1 : 0)];
                double outJL = aJL[countJ - ((countJ > delayJ) ? delayJ + 1 : 0)];

                double outFR = aFR[countF - ((countF > delayF) ? delayF + 1 : 0)];
                double outGR = aGR[countG - ((countG > delayG) ? delayG + 1 : 0)];
                double outHR = aHR[countH - ((countH > delayH) ? delayH + 1 : 0)];
                double outIR = aIR[countI - ((countI > delayI) ? delayI + 1 : 0)];
                double outJR = aJR[countJ - ((countJ > delayJ) ? delayJ + 1 : 0)];

                // -------- Bank 3: 5×5 Householder into K-O
                aKL[countK] = ((outFL * 3.0) - ((outGL + outHL + outIL + outJL) * 2.0));
                aLL[countL] = ((outGL * 3.0) - ((outFL + outHL + outIL + outJL) * 2.0));
                aML[countM] = ((outHL * 3.0) - ((outFL + outGL + outIL + outJL) * 2.0));
                aNL[countN] = ((outIL * 3.0) - ((outFL + outGL + outHL + outJL) * 2.0));
                aOL[countO] = ((outJL * 3.0) - ((outFL + outGL + outHL + outIL) * 2.0));

                aKR[countK] = ((outFR * 3.0) - ((outGR + outHR + outIR + outJR) * 2.0));
                aLR[countL] = ((outGR * 3.0) - ((outFR + outHR + outIR + outJR) * 2.0));
                aMR[countM] = ((outHR * 3.0) - ((outFR + outGR + outIR + outJR) * 2.0));
                aNR[countN] = ((outIR * 3.0) - ((outFR + outGR + outHR + outJR) * 2.0));
                aOR[countO] = ((outJR * 3.0) - ((outFR + outGR + outHR + outIR) * 2.0));

                countK++; if (countK < 0 || countK > delayK) countK = 0;
                countL++; if (countL < 0 || countL > delayL) countL = 0;
                countM++; if (countM < 0 || countM > delayM) countM = 0;
                countN++; if (countN < 0 || countN > delayN) countN = 0;
                countO++; if (countO < 0 || countO > delayO) countO = 0;

                double outKL = aKL[countK - ((countK > delayK) ? delayK + 1 : 0)];
                double outLL = aLL[countL - ((countL > delayL) ? delayL + 1 : 0)];
                double outML = aML[countM - ((countM > delayM) ? delayM + 1 : 0)];
                double outNL = aNL[countN - ((countN > delayN) ? delayN + 1 : 0)];
                double outOL = aOL[countO - ((countO > delayO) ? delayO + 1 : 0)];

                double outKR = aKR[countK - ((countK > delayK) ? delayK + 1 : 0)];
                double outLR = aLR[countL - ((countL > delayL) ? delayL + 1 : 0)];
                double outMR = aMR[countM - ((countM > delayM) ? delayM + 1 : 0)];
                double outNR = aNR[countN - ((countN > delayN) ? delayN + 1 : 0)];
                double outOR = aOR[countO - ((countO > delayO) ? delayO + 1 : 0)];

                // -------- Bank 4: 5×5 Householder into P-T
                aPL[countP] = ((outKL * 3.0) - ((outLL + outML + outNL + outOL) * 2.0));
                aQL[countQ] = ((outLL * 3.0) - ((outKL + outML + outNL + outOL) * 2.0));
                aRL[countR] = ((outML * 3.0) - ((outKL + outLL + outNL + outOL) * 2.0));
                aSL[countS] = ((outNL * 3.0) - ((outKL + outLL + outML + outOL) * 2.0));
                aTL[countT] = ((outOL * 3.0) - ((outKL + outLL + outML + outNL) * 2.0));

                aPR[countP] = ((outKR * 3.0) - ((outLR + outMR + outNR + outOR) * 2.0));
                aQR[countQ] = ((outLR * 3.0) - ((outKR + outMR + outNR + outOR) * 2.0));
                aRR[countR] = ((outMR * 3.0) - ((outKR + outLR + outNR + outOR) * 2.0));
                aSR[countS] = ((outNR * 3.0) - ((outKR + outLR + outMR + outOR) * 2.0));
                aTR[countT] = ((outOR * 3.0) - ((outKR + outLR + outMR + outNR) * 2.0));

                countP++; if (countP < 0 || countP > delayP) countP = 0;
                countQ++; if (countQ < 0 || countQ > delayQ) countQ = 0;
                countR++; if (countR < 0 || countR > delayR) countR = 0;
                countS++; if (countS < 0 || countS > delayS) countS = 0;
                countT++; if (countT < 0 || countT > delayT) countT = 0;

                double outPL = aPL[countP - ((countP > delayP) ? delayP + 1 : 0)];
                double outQL = aQL[countQ - ((countQ > delayQ) ? delayQ + 1 : 0)];
                double outRL = aRL[countR - ((countR > delayR) ? delayR + 1 : 0)];
                double outSL = aSL[countS - ((countS > delayS) ? delayS + 1 : 0)];
                double outTL = aTL[countT - ((countT > delayT) ? delayT + 1 : 0)];

                double outPR = aPR[countP - ((countP > delayP) ? delayP + 1 : 0)];
                double outQR = aQR[countQ - ((countQ > delayQ) ? delayQ + 1 : 0)];
                double outRR = aRR[countR - ((countR > delayR) ? delayR + 1 : 0)];
                double outSR = aSR[countS - ((countS > delayS) ? delayS + 1 : 0)];
                double outTR = aTR[countT - ((countT > delayT) ? delayT + 1 : 0)];

                // -------- Bank 5: 5×5 Householder into U-Y
                aUL[countU] = ((outPL * 3.0) - ((outQL + outRL + outSL + outTL) * 2.0));
                aVL[countV] = ((outQL * 3.0) - ((outPL + outRL + outSL + outTL) * 2.0));
                aWL[countW] = ((outRL * 3.0) - ((outPL + outQL + outSL + outTL) * 2.0));
                aXL[countX] = ((outSL * 3.0) - ((outPL + outQL + outRL + outTL) * 2.0));
                aYL[countY] = ((outTL * 3.0) - ((outPL + outQL + outRL + outSL) * 2.0));

                aUR[countU] = ((outPR * 3.0) - ((outQR + outRR + outSR + outTR) * 2.0));
                aVR[countV] = ((outQR * 3.0) - ((outPR + outRR + outSR + outTR) * 2.0));
                aWR[countW] = ((outRR * 3.0) - ((outPR + outQR + outSR + outTR) * 2.0));
                aXR[countX] = ((outSR * 3.0) - ((outPR + outQR + outRR + outTR) * 2.0));
                aYR[countY] = ((outTR * 3.0) - ((outPR + outQR + outRR + outSR) * 2.0));

                countU++; if (countU < 0 || countU > delayU) countU = 0;
                countV++; if (countV < 0 || countV > delayV) countV = 0;
                countW++; if (countW < 0 || countW > delayW) countW = 0;
                countX++; if (countX < 0 || countX > delayX) countX = 0;
                countY++; if (countY < 0 || countY > delayY) countY = 0;

                double outUL = aUL[countU - ((countU > delayU) ? delayU + 1 : 0)];
                double outVL = aVL[countV - ((countV > delayV) ? delayV + 1 : 0)];
                double outWL = aWL[countW - ((countW > delayW) ? delayW + 1 : 0)];
                double outXL = aXL[countX - ((countX > delayX) ? delayX + 1 : 0)];
                double outYL = aYL[countY - ((countY > delayY) ? delayY + 1 : 0)];

                double outUR = aUR[countU - ((countU > delayU) ? delayU + 1 : 0)];
                double outVR = aVR[countV - ((countV > delayV) ? delayV + 1 : 0)];
                double outWR = aWR[countW - ((countW > delayW) ? delayW + 1 : 0)];
                double outXR = aXR[countX - ((countX > delayX) ? delayX + 1 : 0)];
                double outYR = aYR[countY - ((countY > delayY) ? delayY + 1 : 0)];

                // -------- Stereo cross-feedback (L→R, R→L)
                feedbackAR = ((outUL * 3.0) - ((outVL + outWL + outXL + outYL) * 2.0));
                feedbackBL = ((outVL * 3.0) - ((outUL + outWL + outXL + outYL) * 2.0));
                feedbackCR = ((outWL * 3.0) - ((outUL + outVL + outXL + outYL) * 2.0));
                feedbackDL = ((outXL * 3.0) - ((outUL + outVL + outWL + outYL) * 2.0));
                feedbackER = ((outYL * 3.0) - ((outUL + outVL + outWL + outXL) * 2.0));

                feedbackAL = ((outUR * 3.0) - ((outVR + outWR + outXR + outYR) * 2.0));
                feedbackBR = ((outVR * 3.0) - ((outUR + outWR + outXR + outYR) * 2.0));
                feedbackCL = ((outWR * 3.0) - ((outUR + outVR + outXR + outYR) * 2.0));
                feedbackDR = ((outXR * 3.0) - ((outUR + outVR + outWR + outYR) * 2.0));
                feedbackEL = ((outYR * 3.0) - ((outUR + outVR + outWR + outXR) * 2.0));

                // Sum outputs, corrected for Householder gain
                inputSampleL = (outUL + outVL + outWL + outXL + outYL) * 0.0016;
                inputSampleR = (outUR + outVR + outWR + outXR + outYR) * 0.0016;

                // Interpolation for higher sample rates
                if (cycleEnd == 4) {
                    lastRefL[0] = lastRefL[4];
                    lastRefL[2] = (lastRefL[0] + inputSampleL) / 2;
                    lastRefL[1] = (lastRefL[0] + lastRefL[2]) / 2;
                    lastRefL[3] = (lastRefL[2] + inputSampleL) / 2;
                    lastRefL[4] = inputSampleL;
                    lastRefR[0] = lastRefR[4];
                    lastRefR[2] = (lastRefR[0] + inputSampleR) / 2;
                    lastRefR[1] = (lastRefR[0] + lastRefR[2]) / 2;
                    lastRefR[3] = (lastRefR[2] + inputSampleR) / 2;
                    lastRefR[4] = inputSampleR;
                }
                if (cycleEnd == 3) {
                    lastRefL[0] = lastRefL[3];
                    lastRefL[2] = (lastRefL[0] + lastRefL[0] + inputSampleL) / 3;
                    lastRefL[1] = (lastRefL[0] + inputSampleL + inputSampleL) / 3;
                    lastRefL[3] = inputSampleL;
                    lastRefR[0] = lastRefR[3];
                    lastRefR[2] = (lastRefR[0] + lastRefR[0] + inputSampleR) / 3;
                    lastRefR[1] = (lastRefR[0] + inputSampleR + inputSampleR) / 3;
                    lastRefR[3] = inputSampleR;
                }
                if (cycleEnd == 2) {
                    lastRefL[0] = lastRefL[2];
                    lastRefL[1] = (lastRefL[0] + inputSampleL) / 2;
                    lastRefL[2] = inputSampleL;
                    lastRefR[0] = lastRefR[2];
                    lastRefR[1] = (lastRefR[0] + inputSampleR) / 2;
                    lastRefR[2] = inputSampleR;
                }
                if (cycleEnd == 1) {
                    lastRefL[0] = inputSampleL;
                    lastRefR[0] = inputSampleR;
                }
                cycle = 0;
                inputSampleL = lastRefL[cycle];
                inputSampleR = lastRefR[cycle];
            }
            else
            {
                inputSampleL = lastRefL[cycle];
                inputSampleR = lastRefR[cycle];
            }

            // Lowpass filter
            if (std::fabs (iirBL) < 1.18e-37) iirBL = 0.0;
            iirBL = (iirBL * (1.0 - lowpass)) + (inputSampleL * lowpass);
            inputSampleL = iirBL;
            if (std::fabs (iirBR) < 1.18e-37) iirBR = 0.0;
            iirBR = (iirBR * (1.0 - lowpass)) + (inputSampleR * lowpass);
            inputSampleR = iirBR;

            // Dry/wet mix (submix style)
            if (wet < 1.0) { inputSampleL *= wet; inputSampleR *= wet; }
            if (dry < 1.0) { drySampleL *= dry; drySampleR *= dry; }
            inputSampleL += drySampleL;
            inputSampleR += drySampleR;

            // Dither
            fpdL ^= fpdL << 13; fpdL ^= fpdL >> 17; fpdL ^= fpdL << 5;
            fpdR ^= fpdR << 13; fpdR ^= fpdR >> 17; fpdR ^= fpdR << 5;

            outL[i] = (float) inputSampleL;
            outR[i] = (float) inputSampleR;

            float mx = juce::jmax(std::fabs((float)inputSampleL), std::fabs((float)inputSampleR));
            if (mx > peakLevel) peakLevel = mx;
        }
        decayLevel = decayLevel * 0.95f + peakLevel * 0.05f;
    }

    void setParams (const Params& p) { params.store (p); }
    Params getParams() const { return params.load(); }
    bool   isBypassed() const             { return bypassed; }
    void   setBypassed (bool b)           { bypassed = b; }
    float  getCurrentDecayLevel() const   { return decayLevel; }

private:
    struct AtomicParams
    {
        void store (const Params& p) { roomSize = p.roomSize; sustain = p.sustain; mulch = p.mulch; wetness = p.wetness; }
        Params load() const { return { roomSize.load(), sustain.load(), mulch.load(), wetness.load() }; }
        std::atomic<float> roomSize { 0.5f };
        std::atomic<float> sustain  { 0.5f };
        std::atomic<float> mulch    { 0.5f };
        std::atomic<float> wetness  { 1.0f };
    };
    AtomicParams params;
    double currentSampleRate = 44100.0;
    bool   bypassed   = false;
    float  decayLevel = 0.0f;

    void resetState()
    {
        for (int c = 0; c < 5005; ++c) { aAL[c] = 0.0; aAR[c] = 0.0; }
        for (int c = 0; c < 4953; ++c) { aFL[c] = 0.0; aFR[c] = 0.0; }
        for (int c = 0; c < 4921; ++c) { aKL[c] = 0.0; aKR[c] = 0.0; }
        for (int c = 0; c < 4801; ++c) { aPL[c] = 0.0; aPR[c] = 0.0; }
        for (int c = 0; c < 4753; ++c) { aUL[c] = 0.0; aUR[c] = 0.0; }

        for (int c = 0; c < 4351; ++c) { aBL[c] = 0.0; aBR[c] = 0.0; }
        for (int c = 0; c < 4159; ++c) { aGL[c] = 0.0; aGR[c] = 0.0; }
        for (int c = 0; c < 3931; ++c) { aLL[c] = 0.0; aLR[c] = 0.0; }
        for (int c = 0; c < 3531; ++c) { aQL[c] = 0.0; aQR[c] = 0.0; }
        for (int c = 0; c < 3331; ++c) { aVL[c] = 0.0; aVR[c] = 0.0; }

        for (int c = 0; c < 3325; ++c) { aCL[c] = 0.0; aCR[c] = 0.0; }
        for (int c = 0; c < 2793; ++c) { aHL[c] = 0.0; aHR[c] = 0.0; }
        for (int c = 0; c < 2769; ++c) { aML[c] = 0.0; aMR[c] = 0.0; }
        for (int c = 0; c < 2391; ++c) { aRL[c] = 0.0; aRR[c] = 0.0; }
        for (int c = 0; c < 2349; ++c) { aWL[c] = 0.0; aWR[c] = 0.0; }

        for (int c = 0; c < 2143; ++c) { aDL[c] = 0.0; aDR[c] = 0.0; }
        for (int c = 0; c < 1813; ++c) { aIL[c] = 0.0; aIR[c] = 0.0; }
        for (int c = 0; c < 1735; ++c) { aNL[c] = 0.0; aNR[c] = 0.0; }
        for (int c = 0; c < 1173; ++c) { aSL[c] = 0.0; aSR[c] = 0.0; }
        for (int c = 0; c < 789;  ++c) { aXL[c] = 0.0; aXR[c] = 0.0; }

        for (int c = 0; c < 679;  ++c) { aEL[c] = 0.0; aER[c] = 0.0; }
        for (int c = 0; c < 645;  ++c) { aJL[c] = 0.0; aJR[c] = 0.0; }
        for (int c = 0; c < 441;  ++c) { aOL[c] = 0.0; aOR[c] = 0.0; }
        for (int c = 0; c < 351;  ++c) { aTL[c] = 0.0; aTR[c] = 0.0; }
        for (int c = 0; c < 283;  ++c) { aYL[c] = 0.0; aYR[c] = 0.0; }

        feedbackAL = feedbackBL = feedbackCL = feedbackDL = feedbackEL = 0.0;
        feedbackAR = feedbackBR = feedbackCR = feedbackDR = feedbackER = 0.0;
        previousAL = previousBL = previousCL = previousDL = previousEL = 0.0;
        previousAR = previousBR = previousCR = previousDR = previousER = 0.0;
        iirAL = iirBL = iirAR = iirBR = 0.0;

        for (int c = 0; c < 6; ++c) { lastRefL[c] = 0.0; lastRefR[c] = 0.0; }

        countA = 1; countB = 1; countC = 1; countD = 1; countE = 1;
        countF = 1; countG = 1; countH = 1; countI = 1; countJ = 1;
        countK = 1; countL = 1; countM = 1; countN = 1; countO = 1;
        countP = 1; countQ = 1; countR = 1; countS = 1; countT = 1;
        countU = 1; countV = 1; countW = 1; countX = 1; countY = 1;
        cycle = 0;

        fpdL = 1557111; fpdR = 7891233;
    }

    // ---- Delay line buffers (exact Verbity2 sizes) ----
    // Bank 1: A-E (AFKPU group)
    double aAL[5005] = {}; double aAR[5005] = {};
    double aFL[4953] = {}; double aFR[4953] = {};
    double aKL[4921] = {}; double aKR[4921] = {};
    double aPL[4801] = {}; double aPR[4801] = {};
    double aUL[4753] = {}; double aUR[4753] = {};
    // Bank 2: B-G-L-Q-V
    double aBL[4351] = {}; double aBR[4351] = {};
    double aGL[4159] = {}; double aGR[4159] = {};
    double aLL[3931] = {}; double aLR[3931] = {};
    double aQL[3531] = {}; double aQR[3531] = {};
    double aVL[3331] = {}; double aVR[3331] = {};
    // Bank 3: C-H-M-R-W
    double aCL[3325] = {}; double aCR[3325] = {};
    double aHL[2793] = {}; double aHR[2793] = {};
    double aML[2769] = {}; double aMR[2769] = {};
    double aRL[2391] = {}; double aRR[2391] = {};
    double aWL[2349] = {}; double aWR[2349] = {};
    // Bank 4: D-I-N-S-X
    double aDL[2143] = {}; double aDR[2143] = {};
    double aIL[1813] = {}; double aIR[1813] = {};
    double aNL[1735] = {}; double aNR[1735] = {};
    double aSL[1173] = {}; double aSR[1173] = {};
    double aXL[789]  = {}; double aXR[789]  = {};
    // Bank 5: E-J-O-T-Y
    double aEL[679]  = {}; double aER[679]  = {};
    double aJL[645]  = {}; double aJR[645]  = {};
    double aOL[441]  = {}; double aOR[441]  = {};
    double aTL[351]  = {}; double aTR[351]  = {};
    double aYL[283]  = {}; double aYR[283]  = {};

    // Feedback + softening state
    double feedbackAL = 0.0, feedbackBL = 0.0, feedbackCL = 0.0, feedbackDL = 0.0, feedbackEL = 0.0;
    double feedbackAR = 0.0, feedbackBR = 0.0, feedbackCR = 0.0, feedbackDR = 0.0, feedbackER = 0.0;
    double previousAL = 0.0, previousBL = 0.0, previousCL = 0.0, previousDL = 0.0, previousEL = 0.0;
    double previousAR = 0.0, previousBR = 0.0, previousCR = 0.0, previousDR = 0.0, previousER = 0.0;

    // IIR filters
    double iirAL = 0.0, iirBL = 0.0;
    double iirAR = 0.0, iirBR = 0.0;

    // Interpolation refs
    double lastRefL[7] = {};
    double lastRefR[7] = {};

    // Delay counters and lengths
    int countA = 1, countB = 1, countC = 1, countD = 1, countE = 1;
    int countF = 1, countG = 1, countH = 1, countI = 1, countJ = 1;
    int countK = 1, countL = 1, countM = 1, countN = 1, countO = 1;
    int countP = 1, countQ = 1, countR = 1, countS = 1, countT = 1;
    int countU = 1, countV = 1, countW = 1, countX = 1, countY = 1;

    int delayA = 0, delayB = 0, delayC = 0, delayD = 0, delayE = 0;
    int delayF = 0, delayG = 0, delayH = 0, delayI = 0, delayJ = 0;
    int delayK = 0, delayL = 0, delayM = 0, delayN = 0, delayO = 0;
    int delayP = 0, delayQ = 0, delayR = 0, delayS = 0, delayT = 0;
    int delayU = 0, delayV = 0, delayW = 0, delayX = 0, delayY = 0;

    int cycle = 0;
    uint32_t fpdL = 1557111, fpdR = 7891233;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RoomReverbProcessor)
};
