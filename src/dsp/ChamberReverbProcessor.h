// ==============================================================================
//  ChamberReverbProcessor.h
//  OnStage — Chamber Reverb
//
//  Based on Airwindows Chamber2 by Chris Johnson (MIT License).
//  3-stage 4×4 Householder matrix reverb with Golden Ratio delay coefficients.
//  13 delay line pairs (A-M L/R), predelay buffer, feedback interpolation.
//
//  Parameters (all 0-1):
//    Delay    (A) — size/delay length scaling
//    Regen    (B) — feedback/regeneration amount
//    Thick    (C) — thickness: controls echo spacing vs blurred delay
//    Wet      (D) — dry/wet mix (submix style: 50% = both full volume)
//
//  Copyright (c) airwindows, MIT License. OnStage integration by Rob.
// ==============================================================================

#pragma once
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

class ChamberReverbProcessor
{
public:
    struct Params
    {
        float delay = 0.34f;
        float regen = 0.31f;
        float thick = 0.28f;
        float wet   = 0.25f;

        bool operator== (const Params& o) const
        {
            return delay == o.delay && regen == o.regen
                && thick == o.thick && wet == o.wet;
        }
        bool operator!= (const Params& o) const { return !(*this == o); }
    };

    ChamberReverbProcessor() = default;

    Params getParams() const              { return params.load(); }
    void   setParams (const Params& p)    { params.store (p); }
    bool   isBypassed() const             { return bypassed; }
    void   setBypassed (bool b)           { bypassed = b; }
    float  getCurrentDecayLevel() const   { return decayLevel; }

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        currentSampleRate = spec.sampleRate;
        resetState();
    }

    void reset() { resetState(); }

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

        double size = ((double) p.delay * 0.9) + 0.1;
        double regenAmount = (1.0 - std::pow (1.0 - (double) p.regen, 2.0)) * 0.123;
        double echoScale = 1.0 - (double) p.thick;
        double echo = 0.618033988749894848204586 + ((1.0 - 0.618033988749894848204586) * echoScale);
        double interpolate = (1.0 - echo) * 0.381966011250105;
        double wet = (double) p.wet * 2.0;
        double dry = 2.0 - wet;
        if (wet > 1.0) wet = 1.0; if (wet < 0.0) wet = 0.0;
        if (dry > 1.0) dry = 1.0; if (dry < 0.0) dry = 0.0;

        delayM = (int) std::sqrt (9900.0 * size);
        delayE = (int) (9900.0 * size);
        delayF = (int) (delayE * echo);
        delayG = (int) (delayF * echo);
        delayH = (int) (delayG * echo);
        delayA = (int) (delayH * echo);
        delayB = (int) (delayA * echo);
        delayC = (int) (delayB * echo);
        delayD = (int) (delayC * echo);
        delayI = (int) (delayD * echo);
        delayJ = (int) (delayI * echo);
        delayK = (int) (delayJ * echo);
        delayL = (int) (delayK * echo);

        for (int i = 0; i < numSamples; ++i)
        {
            double inputSampleL = (double) outL[i];
            double inputSampleR = (double) outR[i];
            if (std::fabs (inputSampleL) < 1.18e-23) inputSampleL = fpdL * 1.18e-17;
            if (std::fabs (inputSampleR) < 1.18e-23) inputSampleR = fpdR * 1.18e-17;
            double drySampleL = inputSampleL;
            double drySampleR = inputSampleR;

            cycle++;
            if (cycle == cycleEnd)
            {
                // Predelay
                aML[countM] = inputSampleL;
                aMR[countM] = inputSampleR;
                countM++; if (countM < 0 || countM > delayM) countM = 0;
                inputSampleL = aML[countM - ((countM > delayM) ? delayM + 1 : 0)];
                inputSampleR = aMR[countM - ((countM > delayM) ? delayM + 1 : 0)];

                // Feedback interpolation
                feedbackAL = (feedbackAL * (1.0 - interpolate)) + (previousAL * interpolate); previousAL = feedbackAL;
                feedbackBL = (feedbackBL * (1.0 - interpolate)) + (previousBL * interpolate); previousBL = feedbackBL;
                feedbackCL = (feedbackCL * (1.0 - interpolate)) + (previousCL * interpolate); previousCL = feedbackCL;
                feedbackDL = (feedbackDL * (1.0 - interpolate)) + (previousDL * interpolate); previousDL = feedbackDL;
                feedbackAR = (feedbackAR * (1.0 - interpolate)) + (previousAR * interpolate); previousAR = feedbackAR;
                feedbackBR = (feedbackBR * (1.0 - interpolate)) + (previousBR * interpolate); previousBR = feedbackBR;
                feedbackCR = (feedbackCR * (1.0 - interpolate)) + (previousCR * interpolate); previousCR = feedbackCR;
                feedbackDR = (feedbackDR * (1.0 - interpolate)) + (previousDR * interpolate); previousDR = feedbackDR;

                // Block 1
                aIL[countI] = inputSampleL + (feedbackAL * regenAmount);
                aJL[countJ] = inputSampleL + (feedbackBL * regenAmount);
                aKL[countK] = inputSampleL + (feedbackCL * regenAmount);
                aLL[countL] = inputSampleL + (feedbackDL * regenAmount);
                aIR[countI] = inputSampleR + (feedbackAR * regenAmount);
                aJR[countJ] = inputSampleR + (feedbackBR * regenAmount);
                aKR[countK] = inputSampleR + (feedbackCR * regenAmount);
                aLR[countL] = inputSampleR + (feedbackDR * regenAmount);

                countI++; if (countI < 0 || countI > delayI) countI = 0;
                countJ++; if (countJ < 0 || countJ > delayJ) countJ = 0;
                countK++; if (countK < 0 || countK > delayK) countK = 0;
                countL++; if (countL < 0 || countL > delayL) countL = 0;

                double outIL = aIL[countI - ((countI > delayI) ? delayI + 1 : 0)];
                double outJL = aJL[countJ - ((countJ > delayJ) ? delayJ + 1 : 0)];
                double outKL = aKL[countK - ((countK > delayK) ? delayK + 1 : 0)];
                double outLL = aLL[countL - ((countL > delayL) ? delayL + 1 : 0)];
                double outIR = aIR[countI - ((countI > delayI) ? delayI + 1 : 0)];
                double outJR = aJR[countJ - ((countJ > delayJ) ? delayJ + 1 : 0)];
                double outKR = aKR[countK - ((countK > delayK) ? delayK + 1 : 0)];
                double outLR = aLR[countL - ((countL > delayL) ? delayL + 1 : 0)];

                // Block 2: Householder
                aAL[countA] = (outIL - (outJL + outKL + outLL));
                aBL[countB] = (outJL - (outIL + outKL + outLL));
                aCL[countC] = (outKL - (outIL + outJL + outLL));
                aDL[countD] = (outLL - (outIL + outJL + outKL));
                aAR[countA] = (outIR - (outJR + outKR + outLR));
                aBR[countB] = (outJR - (outIR + outKR + outLR));
                aCR[countC] = (outKR - (outIR + outJR + outLR));
                aDR[countD] = (outLR - (outIR + outJR + outKR));

                countA++; if (countA < 0 || countA > delayA) countA = 0;
                countB++; if (countB < 0 || countB > delayB) countB = 0;
                countC++; if (countC < 0 || countC > delayC) countC = 0;
                countD++; if (countD < 0 || countD > delayD) countD = 0;

                double outAL = aAL[countA - ((countA > delayA) ? delayA + 1 : 0)];
                double outBL = aBL[countB - ((countB > delayB) ? delayB + 1 : 0)];
                double outCL = aCL[countC - ((countC > delayC) ? delayC + 1 : 0)];
                double outDL = aDL[countD - ((countD > delayD) ? delayD + 1 : 0)];
                double outAR = aAR[countA - ((countA > delayA) ? delayA + 1 : 0)];
                double outBR = aBR[countB - ((countB > delayB) ? delayB + 1 : 0)];
                double outCR = aCR[countC - ((countC > delayC) ? delayC + 1 : 0)];
                double outDR = aDR[countD - ((countD > delayD) ? delayD + 1 : 0)];

                // Block 3: Householder
                aEL[countE] = (outAL - (outBL + outCL + outDL));
                aFL[countF] = (outBL - (outAL + outCL + outDL));
                aGL[countG] = (outCL - (outAL + outBL + outDL));
                aHL[countH] = (outDL - (outAL + outBL + outCL));
                aER[countE] = (outAR - (outBR + outCR + outDR));
                aFR[countF] = (outBR - (outAR + outCR + outDR));
                aGR[countG] = (outCR - (outAR + outBR + outDR));
                aHR[countH] = (outDR - (outAR + outBR + outCR));

                countE++; if (countE < 0 || countE > delayE) countE = 0;
                countF++; if (countF < 0 || countF > delayF) countF = 0;
                countG++; if (countG < 0 || countG > delayG) countG = 0;
                countH++; if (countH < 0 || countH > delayH) countH = 0;

                double outEL = aEL[countE - ((countE > delayE) ? delayE + 1 : 0)];
                double outFL = aFL[countF - ((countF > delayF) ? delayF + 1 : 0)];
                double outGL = aGL[countG - ((countG > delayG) ? delayG + 1 : 0)];
                double outHL = aHL[countH - ((countH > delayH) ? delayH + 1 : 0)];
                double outER = aER[countE - ((countE > delayE) ? delayE + 1 : 0)];
                double outFR = aFR[countF - ((countF > delayF) ? delayF + 1 : 0)];
                double outGR = aGR[countG - ((countG > delayG) ? delayG + 1 : 0)];
                double outHR = aHR[countH - ((countH > delayH) ? delayH + 1 : 0)];

                // Cross-channel feedback
                feedbackAR = (outEL - (outFL + outGL + outHL));
                feedbackBL = (outFL - (outEL + outGL + outHL));
                feedbackCR = (outGL - (outEL + outFL + outHL));
                feedbackDL = (outHL - (outEL + outFL + outGL));
                feedbackAL = (outER - (outFR + outGR + outHR));
                feedbackBR = (outFR - (outER + outGR + outHR));
                feedbackCL = (outGR - (outER + outFR + outHR));
                feedbackDR = (outHR - (outER + outFR + outGR));

                inputSampleL = (outEL + outFL + outGL + outHL) / 8.0;
                inputSampleR = (outER + outFR + outGR + outHR) / 8.0;

                // Oversampling interpolation
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
            }
            else
            {
                inputSampleL = lastRefL[cycle];
                inputSampleR = lastRefR[cycle];
            }

            switch (cycleEnd) {
                case 4: lastRefL[8] = inputSampleL; inputSampleL = (inputSampleL + lastRefL[7]) * 0.5; lastRefL[7] = lastRefL[8];
                        lastRefR[8] = inputSampleR; inputSampleR = (inputSampleR + lastRefR[7]) * 0.5; lastRefR[7] = lastRefR[8]; [[fallthrough]];
                case 3: lastRefL[8] = inputSampleL; inputSampleL = (inputSampleL + lastRefL[6]) * 0.5; lastRefL[6] = lastRefL[8];
                        lastRefR[8] = inputSampleR; inputSampleR = (inputSampleR + lastRefR[6]) * 0.5; lastRefR[6] = lastRefR[8]; [[fallthrough]];
                case 2: lastRefL[8] = inputSampleL; inputSampleL = (inputSampleL + lastRefL[5]) * 0.5; lastRefL[5] = lastRefL[8];
                        lastRefR[8] = inputSampleR; inputSampleR = (inputSampleR + lastRefR[5]) * 0.5; lastRefR[5] = lastRefR[8]; [[fallthrough]];
                case 1: break;
            }

            if (wet < 1.0) { inputSampleL *= wet; inputSampleR *= wet; }
            if (dry < 1.0) { drySampleL *= dry; drySampleR *= dry; }
            inputSampleL += drySampleL;
            inputSampleR += drySampleR;

            int expon; std::frexp ((float) inputSampleL, &expon);
            fpdL ^= fpdL << 13; fpdL ^= fpdL >> 17; fpdL ^= fpdL << 5;
            inputSampleL += ((double(fpdL) - uint32_t(0x7fffffff)) * 5.5e-36l * std::pow (2, expon + 62));
            std::frexp ((float) inputSampleR, &expon);
            fpdR ^= fpdR << 13; fpdR ^= fpdR >> 17; fpdR ^= fpdR << 5;
            inputSampleR += ((double(fpdR) - uint32_t(0x7fffffff)) * 5.5e-36l * std::pow (2, expon + 62));

            outL[i] = (float) inputSampleL;
            outR[i] = (float) inputSampleR;

            float mx = juce::jmax(std::fabs((float)inputSampleL), std::fabs((float)inputSampleR));
            if (mx > peakLevel) peakLevel = mx;
        }
        decayLevel = decayLevel * 0.95f + peakLevel * 0.05f;
    }

    struct AtomicParams
    {
        void store (const Params& p) { delay.store(p.delay, std::memory_order_relaxed); regen.store(p.regen, std::memory_order_relaxed); thick.store(p.thick, std::memory_order_relaxed); wet.store(p.wet, std::memory_order_relaxed); }
        Params load() const { return { delay.load(std::memory_order_relaxed), regen.load(std::memory_order_relaxed), thick.load(std::memory_order_relaxed), wet.load(std::memory_order_relaxed) }; }
    private:
        std::atomic<float> delay{0.34f}, regen{0.31f}, thick{0.28f}, wet{0.25f};
    };

    AtomicParams params;

private:
    double currentSampleRate = 44100.0;
    bool   bypassed   = false;
    float  decayLevel = 0.0f;
    double aEL[10000]={}, aER[10000]={}, aFL[10000]={}, aFR[10000]={};
    double aGL[10000]={}, aGR[10000]={}, aHL[10000]={}, aHR[10000]={};
    double aAL[10000]={}, aAR[10000]={}, aBL[10000]={}, aBR[10000]={};
    double aCL[10000]={}, aCR[10000]={}, aDL[10000]={}, aDR[10000]={};
    double aIL[10000]={}, aIR[10000]={}, aJL[10000]={}, aJR[10000]={};
    double aKL[10000]={}, aKR[10000]={}, aLL[10000]={}, aLR[10000]={};
    double aML[10000]={}, aMR[10000]={};
    double feedbackAL=0, feedbackAR=0, feedbackBL=0, feedbackBR=0;
    double feedbackCL=0, feedbackCR=0, feedbackDL=0, feedbackDR=0;
    double previousAL=0, previousAR=0, previousBL=0, previousBR=0;
    double previousCL=0, previousCR=0, previousDL=0, previousDR=0;
    double lastRefL[10]={}, lastRefR[10]={};
    int countA=1, countB=1, countC=1, countD=1, countE=1, countF=1, countG=1, countH=1;
    int countI=1, countJ=1, countK=1, countL=1, countM=1;
    int delayA=0, delayB=0, delayC=0, delayD=0, delayE=0, delayF=0, delayG=0, delayH=0;
    int delayI=0, delayJ=0, delayK=0, delayL=0, delayM=0;
    int cycle = 0;
    uint32_t fpdL = 1, fpdR = 1;

    void resetState()
    {
        std::memset(aEL,0,sizeof(aEL)); std::memset(aER,0,sizeof(aER));
        std::memset(aFL,0,sizeof(aFL)); std::memset(aFR,0,sizeof(aFR));
        std::memset(aGL,0,sizeof(aGL)); std::memset(aGR,0,sizeof(aGR));
        std::memset(aHL,0,sizeof(aHL)); std::memset(aHR,0,sizeof(aHR));
        std::memset(aAL,0,sizeof(aAL)); std::memset(aAR,0,sizeof(aAR));
        std::memset(aBL,0,sizeof(aBL)); std::memset(aBR,0,sizeof(aBR));
        std::memset(aCL,0,sizeof(aCL)); std::memset(aCR,0,sizeof(aCR));
        std::memset(aDL,0,sizeof(aDL)); std::memset(aDR,0,sizeof(aDR));
        std::memset(aIL,0,sizeof(aIL)); std::memset(aIR,0,sizeof(aIR));
        std::memset(aJL,0,sizeof(aJL)); std::memset(aJR,0,sizeof(aJR));
        std::memset(aKL,0,sizeof(aKL)); std::memset(aKR,0,sizeof(aKR));
        std::memset(aLL,0,sizeof(aLL)); std::memset(aLR,0,sizeof(aLR));
        std::memset(aML,0,sizeof(aML)); std::memset(aMR,0,sizeof(aMR));
        feedbackAL=feedbackAR=feedbackBL=feedbackBR=0;
        feedbackCL=feedbackCR=feedbackDL=feedbackDR=0;
        previousAL=previousAR=previousBL=previousBR=0;
        previousCL=previousCR=previousDL=previousDR=0;
        std::memset(lastRefL,0,sizeof(lastRefL)); std::memset(lastRefR,0,sizeof(lastRefR));
        countA=countB=countC=countD=1; countE=countF=countG=countH=1;
        countI=countJ=countK=countL=1; countM=1; cycle=0;
        fpdL=1; while(fpdL<16386) fpdL=(uint32_t)(std::rand())*(uint32_t)(std::rand());
        fpdR=1; while(fpdR<16386) fpdR=(uint32_t)(std::rand())*(uint32_t)(std::rand());
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChamberReverbProcessor)
};
