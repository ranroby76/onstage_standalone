// ==============================================================================
//  SpaceReverbProcessor.h
//  OnStage — Space Reverb
//
//  Based on Airwindows Galactic3 by Chris Johnson (MIT License).
//  3-stage 4×4 Householder matrix reverb with bezier curve undersampling,
//  vibrato predelay, dual IIR filters, and variable resolution.
//
//  Parameters (all 0-1):
//    Replace    (A) — feedback amount (inverted: higher = more wash)
//    Brightness (B) — lowpass filter cutoff
//    Detune     (C) — vibrato/drift amount
//    Derez      (D) — sample rate reduction (resolution)
//    Bigness    (E) — delay line size scaling
//    DryWet     (F) — dry/wet mix (cubic curve)
//
//  Copyright (c) airwindows, MIT License. OnStage integration by Rob.
// ==============================================================================

#pragma once
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

class SpaceReverbProcessor
{
public:
    struct Params
    {
        float replace    = 0.5f;   // A
        float brightness = 0.5f;   // B
        float detune     = 0.5f;   // C
        float derez      = 1.0f;   // D
        float bigness    = 1.0f;   // E
        float dryWet     = 1.0f;   // F

        bool operator== (const Params& o) const
        {
            return replace == o.replace && brightness == o.brightness
                && detune == o.detune && derez == o.derez
                && bigness == o.bigness && dryWet == o.dryWet;
        }
        bool operator!= (const Params& o) const { return !(*this == o); }
    };

    SpaceReverbProcessor() = default;

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

        double regen = 0.0625 + ((1.0 - (double)p.replace) * 0.0625);
        double attenuate = (1.0 - (regen / 0.125)) * 1.333;
        double lowpass = std::pow (1.00001 - (1.0 - (double)p.brightness), 2.0) / std::sqrt (overallscale);
        double drift = std::pow ((double)p.detune, 3.0) * 0.001;
        double derez = (double)p.derez / overallscale;
        if (derez < 0.0005) derez = 0.0005; if (derez > 1.0) derez = 1.0;
        derez = 1.0 / ((int)(1.0 / derez));
        double size = ((double)p.bigness * 1.77) + 0.1;
        double wet = 1.0 - std::pow (1.0 - (double)p.dryWet, 3.0);

        delayI = (int)(3407.0 * size);
        delayJ = (int)(1823.0 * size);
        delayK = (int)(859.0 * size);
        delayL = (int)(331.0 * size);
        delayA = (int)(4801.0 * size);
        delayB = (int)(2909.0 * size);
        delayC = (int)(1153.0 * size);
        delayD = (int)(461.0 * size);
        delayE = (int)(7607.0 * size);
        delayF = (int)(4217.0 * size);
        delayG = (int)(2269.0 * size);
        delayH = (int)(1597.0 * size);
        delayM = 256;

        for (int i = 0; i < numSamples; ++i)
        {
            double inputSampleL = (double)outL[i];
            double inputSampleR = (double)outR[i];
            if (std::fabs(inputSampleL) < 1.18e-23) inputSampleL = fpdL * 1.18e-17;
            if (std::fabs(inputSampleR) < 1.18e-23) inputSampleR = fpdR * 1.18e-17;
            double drySampleL = inputSampleL;
            double drySampleR = inputSampleR;

            // Vibrato LFO
            vibM += (oldfpd * drift);
            if (vibM > (3.141592653589793238 * 2.0)) {
                vibM = 0.0;
                oldfpd = 0.4294967295 + (fpdL * 0.0000000000618);
            }

            // Vibrato predelay
            aML[countM] = inputSampleL * attenuate;
            aMR[countM] = inputSampleR * attenuate;
            countM++; if (countM < 0 || countM > delayM) countM = 0;

            double offsetML = (std::sin(vibM) + 1.0) * 127;
            double offsetMR = (std::sin(vibM + (3.141592653589793238 / 2.0)) + 1.0) * 127;
            int workingML = countM + (int)offsetML;
            int workingMR = countM + (int)offsetMR;
            double interpolML = (aML[workingML - ((workingML > delayM) ? delayM + 1 : 0)] * (1 - (offsetML - std::floor(offsetML))));
            interpolML += (aML[workingML + 1 - ((workingML + 1 > delayM) ? delayM + 1 : 0)] * ((offsetML - std::floor(offsetML))));
            double interpolMR = (aMR[workingMR - ((workingMR > delayM) ? delayM + 1 : 0)] * (1 - (offsetMR - std::floor(offsetMR))));
            interpolMR += (aMR[workingMR + 1 - ((workingMR + 1 > delayM) ? delayM + 1 : 0)] * ((offsetMR - std::floor(offsetMR))));
            inputSampleL = interpolML;
            inputSampleR = interpolMR;

            // Input lowpass
            iirAL = (iirAL * (1.0 - lowpass)) + (inputSampleL * lowpass); inputSampleL = iirAL;
            iirAR = (iirAR * (1.0 - lowpass)) + (inputSampleR * lowpass); inputSampleR = iirAR;

            // Bezier curve undersampling
            bez[bez_cycle] += derez;
            bez[bez_SampL] += ((inputSampleL + bez[bez_InL]) * derez);
            bez[bez_SampR] += ((inputSampleR + bez[bez_InR]) * derez);
            bez[bez_InL] = inputSampleL; bez[bez_InR] = inputSampleR;

            if (bez[bez_cycle] > 1.0)
            {
                bez[bez_cycle] = 0.0;

                // Block 1: input + cross-channel feedback
                aIL[countI] = (bez[bez_SampL] + bez[bez_UnInL]) + (feedbackAR * regen);
                aJL[countJ] = (bez[bez_SampL] + bez[bez_UnInL]) + (feedbackBR * regen);
                aKL[countK] = (bez[bez_SampL] + bez[bez_UnInL]) + (feedbackCR * regen);
                aLL[countL] = (bez[bez_SampL] + bez[bez_UnInL]) + (feedbackDR * regen);
                bez[bez_UnInL] = bez[bez_SampL];

                aIR[countI] = (bez[bez_SampR] + bez[bez_UnInR]) + (feedbackAL * regen);
                aJR[countJ] = (bez[bez_SampR] + bez[bez_UnInR]) + (feedbackBL * regen);
                aKR[countK] = (bez[bez_SampR] + bez[bez_UnInR]) + (feedbackCL * regen);
                aLR[countL] = (bez[bez_SampR] + bez[bez_UnInR]) + (feedbackDL * regen);
                bez[bez_UnInR] = bez[bez_SampR];

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

                // Feedback (no cross-channel on Galactic3 — same channel)
                feedbackAL = (outEL - (outFL + outGL + outHL));
                feedbackBL = (outFL - (outEL + outGL + outHL));
                feedbackCL = (outGL - (outEL + outFL + outHL));
                feedbackDL = (outHL - (outEL + outFL + outGL));
                feedbackAR = (outER - (outFR + outGR + outHR));
                feedbackBR = (outFR - (outER + outGR + outHR));
                feedbackCR = (outGR - (outER + outFR + outHR));
                feedbackDR = (outHR - (outER + outFR + outGR));

                inputSampleL = (outEL + outFL + outGL + outHL) / 8.0;
                inputSampleR = (outER + outFR + outGR + outHR) / 8.0;

                // Bezier reconstruction shift
                bez[bez_CL] = bez[bez_BL]; bez[bez_BL] = bez[bez_AL]; bez[bez_AL] = inputSampleL; bez[bez_SampL] = 0.0;
                bez[bez_CR] = bez[bez_BR]; bez[bez_BR] = bez[bez_AR]; bez[bez_AR] = inputSampleR; bez[bez_SampR] = 0.0;
            }

            // Bezier curve interpolation
            double CBL = (bez[bez_CL] * (1.0 - bez[bez_cycle])) + (bez[bez_BL] * bez[bez_cycle]);
            double CBR = (bez[bez_CR] * (1.0 - bez[bez_cycle])) + (bez[bez_BR] * bez[bez_cycle]);
            double BAL = (bez[bez_BL] * (1.0 - bez[bez_cycle])) + (bez[bez_AL] * bez[bez_cycle]);
            double BAR = (bez[bez_BR] * (1.0 - bez[bez_cycle])) + (bez[bez_AR] * bez[bez_cycle]);
            double CBAL = (bez[bez_BL] + (CBL * (1.0 - bez[bez_cycle])) + (BAL * bez[bez_cycle])) * 0.125;
            double CBAR = (bez[bez_BR] + (CBR * (1.0 - bez[bez_cycle])) + (BAR * bez[bez_cycle])) * 0.125;
            inputSampleL = CBAL;
            inputSampleR = CBAR;

            // Output lowpass
            iirBL = (iirBL * (1.0 - lowpass)) + (inputSampleL * lowpass); inputSampleL = iirBL;
            iirBR = (iirBR * (1.0 - lowpass)) + (inputSampleR * lowpass); inputSampleR = iirBR;

            // Dry/wet
            if (wet < 1.0) {
                inputSampleL = (inputSampleL * wet) + (drySampleL * (1.0 - wet));
                inputSampleR = (inputSampleR * wet) + (drySampleR * (1.0 - wet));
            }

            // TPDF dither
            int expon; std::frexp((float)inputSampleL, &expon);
            fpdL ^= fpdL << 13; fpdL ^= fpdL >> 17; fpdL ^= fpdL << 5;
            inputSampleL += ((double(fpdL) - uint32_t(0x7fffffff)) * 5.5e-36l * std::pow(2, expon + 62));
            std::frexp((float)inputSampleR, &expon);
            fpdR ^= fpdR << 13; fpdR ^= fpdR >> 17; fpdR ^= fpdR << 5;
            inputSampleR += ((double(fpdR) - uint32_t(0x7fffffff)) * 5.5e-36l * std::pow(2, expon + 62));

            outL[i] = (float)inputSampleL;
            outR[i] = (float)inputSampleR;

            float mx = juce::jmax(std::fabs((float)inputSampleL), std::fabs((float)inputSampleR));
            if (mx > peakLevel) peakLevel = mx;
        }
        decayLevel = decayLevel * 0.95f + peakLevel * 0.05f;
    }

    struct AtomicParams
    {
        void store (const Params& p) { replace.store(p.replace,std::memory_order_relaxed); brightness.store(p.brightness,std::memory_order_relaxed); detune.store(p.detune,std::memory_order_relaxed); derez.store(p.derez,std::memory_order_relaxed); bigness.store(p.bigness,std::memory_order_relaxed); dryWet.store(p.dryWet,std::memory_order_relaxed); }
        Params load() const { return { replace.load(std::memory_order_relaxed), brightness.load(std::memory_order_relaxed), detune.load(std::memory_order_relaxed), derez.load(std::memory_order_relaxed), bigness.load(std::memory_order_relaxed), dryWet.load(std::memory_order_relaxed) }; }
    private:
        std::atomic<float> replace{0.5f}, brightness{0.5f}, detune{0.5f}, derez{1.0f}, bigness{1.0f}, dryWet{1.0f};
    };

    AtomicParams params;

private:
    double currentSampleRate = 44100.0;
    bool   bypassed   = false;
    float  decayLevel = 0.0f;

    double iirAL=0, iirBL=0, iirAR=0, iirBR=0;

    double aIL[6480]={}, aIR[6480]={}, aJL[3660]={}, aJR[3660]={};
    double aKL[1720]={}, aKR[1720]={}, aLL[680]={},  aLR[680]={};
    double aAL[9700]={}, aAR[9700]={}, aBL[6000]={}, aBR[6000]={};
    double aCL[2320]={}, aCR[2320]={}, aDL[940]={},  aDR[940]={};
    double aEL[15220]={}, aER[15220]={}, aFL[8460]={}, aFR[8460]={};
    double aGL[4540]={}, aGR[4540]={}, aHL[3200]={}, aHR[3200]={};
    double aML[3111]={}, aMR[3111]={};

    double feedbackAL=0, feedbackBL=0, feedbackCL=0, feedbackDL=0;
    double feedbackAR=0, feedbackBR=0, feedbackCR=0, feedbackDR=0;

    int countA=1, countB=1, countC=1, countD=1, countE=1, countF=1, countG=1, countH=1;
    int countI=1, countJ=1, countK=1, countL=1, countM=1;
    int delayA=0, delayB=0, delayC=0, delayD=0, delayE=0, delayF=0, delayG=0, delayH=0;
    int delayI=0, delayJ=0, delayK=0, delayL=0, delayM=256;

    double vibM = 3.0;
    double oldfpd = 429496.7295;

    // Bezier curve state
    enum { bez_AL, bez_AR, bez_BL, bez_BR, bez_CL, bez_CR, bez_InL, bez_InR,
           bez_UnInL, bez_UnInR, bez_SampL, bez_SampR, bez_cycle, bez_total };
    double bez[bez_total] = {};

    uint32_t fpdL = 1, fpdR = 1;

    void resetState()
    {
        iirAL=iirBL=iirAR=iirBR=0;
        std::memset(aIL,0,sizeof(aIL)); std::memset(aIR,0,sizeof(aIR));
        std::memset(aJL,0,sizeof(aJL)); std::memset(aJR,0,sizeof(aJR));
        std::memset(aKL,0,sizeof(aKL)); std::memset(aKR,0,sizeof(aKR));
        std::memset(aLL,0,sizeof(aLL)); std::memset(aLR,0,sizeof(aLR));
        std::memset(aAL,0,sizeof(aAL)); std::memset(aAR,0,sizeof(aAR));
        std::memset(aBL,0,sizeof(aBL)); std::memset(aBR,0,sizeof(aBR));
        std::memset(aCL,0,sizeof(aCL)); std::memset(aCR,0,sizeof(aCR));
        std::memset(aDL,0,sizeof(aDL)); std::memset(aDR,0,sizeof(aDR));
        std::memset(aEL,0,sizeof(aEL)); std::memset(aER,0,sizeof(aER));
        std::memset(aFL,0,sizeof(aFL)); std::memset(aFR,0,sizeof(aFR));
        std::memset(aGL,0,sizeof(aGL)); std::memset(aGR,0,sizeof(aGR));
        std::memset(aHL,0,sizeof(aHL)); std::memset(aHR,0,sizeof(aHR));
        std::memset(aML,0,sizeof(aML)); std::memset(aMR,0,sizeof(aMR));
        feedbackAL=feedbackBL=feedbackCL=feedbackDL=0;
        feedbackAR=feedbackBR=feedbackCR=feedbackDR=0;
        countA=countB=countC=countD=1; countE=countF=countG=countH=1;
        countI=countJ=countK=countL=1; countM=1;
        vibM=3.0; oldfpd=429496.7295;
        std::memset(bez,0,sizeof(bez)); bez[bez_cycle]=1.0;
        fpdL=1; while(fpdL<16386) fpdL=(uint32_t)(std::rand())*(uint32_t)(std::rand());
        fpdR=1; while(fpdR<16386) fpdR=(uint32_t)(std::rand())*(uint32_t)(std::rand());
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpaceReverbProcessor)
};
