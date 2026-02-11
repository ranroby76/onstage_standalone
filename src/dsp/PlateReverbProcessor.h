// ==============================================================================
//  PlateReverbProcessor.h
//  OnStage — Plate Reverb (Airwindows kPlateD port)
//
//  Architecture: 9 allpass early reflections (A-I) → predelay Z →
//  5 cascaded 5×5 Householder blocks (A-Y, 25 delay lines per channel) with
//  4 biquad bandpass filters + mulch damping. Input/output compressor.
//  Cross-channel stereo feedback. Oversampling with lastRef interpolation.
//
//  Parameters: A=InputPad, B=Damping, C=LowCut, D=Predelay, E=Wetness
//  MIT License (Airwindows)
// ==============================================================================

#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <cmath>
#include <cstring>

class PlateReverbProcessor
{
public:
    // ==============================================================================
    struct Params
    {
        float inputPad  = 1.0f;   // A: input pad (0–1)
        float damping   = 0.5f;   // B: damping/regen (0–1)
        float lowCut    = 1.0f;   // C: highpass / low cut (0–1)
        float predelay  = 0.0f;   // D: predelay amount (0–1)
        float wetness   = 0.25f;  // E: dry/wet (0–1, submix style)

        bool operator== (const Params& o) const
        {
            return inputPad == o.inputPad && damping == o.damping
                && lowCut == o.lowCut && predelay == o.predelay
                && wetness == o.wetness;
        }
        bool operator!= (const Params& o) const { return !(*this == o); }
    };

    struct AtomicParams
    {
        std::atomic<float> inputPad { 1.0f };
        std::atomic<float> damping  { 0.5f };
        std::atomic<float> lowCut   { 1.0f };
        std::atomic<float> predelay { 0.0f };
        std::atomic<float> wetness  { 0.25f };

        void store (const Params& p)
        {
            inputPad.store (p.inputPad, std::memory_order_relaxed);
            damping.store  (p.damping,  std::memory_order_relaxed);
            lowCut.store   (p.lowCut,   std::memory_order_relaxed);
            predelay.store (p.predelay, std::memory_order_relaxed);
            wetness.store  (p.wetness,  std::memory_order_relaxed);
        }
        Params load() const
        {
            return { inputPad.load (std::memory_order_relaxed),
                     damping.load  (std::memory_order_relaxed),
                     lowCut.load   (std::memory_order_relaxed),
                     predelay.load (std::memory_order_relaxed),
                     wetness.load  (std::memory_order_relaxed) };
        }
    };

    // ==============================================================================
    PlateReverbProcessor() = default;

    Params getParams() const              { return atomicParams.load(); }
    void   setParams (const Params& p)    { atomicParams.store (p); }
    bool   isBypassed() const             { return bypassed; }
    void   setBypassed (bool b)           { bypassed = b; }
    float  getCurrentDecayLevel() const   { return decayLevel; }

    // ==============================================================================
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        reset();
    }

    void reset()
    {
        iirAL = iirBL = iirAR = iirBR = 0.0;
        gainIn = gainOutL = gainOutR = 1.0;

        // Clear early reflection buffers
        std::memset(eAL, 0, sizeof(eAL)); std::memset(eAR, 0, sizeof(eAR));
        std::memset(eBL, 0, sizeof(eBL)); std::memset(eBR, 0, sizeof(eBR));
        std::memset(eCL, 0, sizeof(eCL)); std::memset(eCR, 0, sizeof(eCR));
        std::memset(eDL, 0, sizeof(eDL)); std::memset(eDR, 0, sizeof(eDR));
        std::memset(eEL, 0, sizeof(eEL)); std::memset(eER, 0, sizeof(eER));
        std::memset(eFL, 0, sizeof(eFL)); std::memset(eFR, 0, sizeof(eFR));
        std::memset(eGL, 0, sizeof(eGL)); std::memset(eGR, 0, sizeof(eGR));
        std::memset(eHL, 0, sizeof(eHL)); std::memset(eHR, 0, sizeof(eHR));
        std::memset(eIL, 0, sizeof(eIL)); std::memset(eIR, 0, sizeof(eIR));

        // Clear late delay line buffers (A-Y)
        std::memset(aAL, 0, sizeof(aAL)); std::memset(aAR, 0, sizeof(aAR));
        std::memset(aBL, 0, sizeof(aBL)); std::memset(aBR, 0, sizeof(aBR));
        std::memset(aCL, 0, sizeof(aCL)); std::memset(aCR, 0, sizeof(aCR));
        std::memset(aDL, 0, sizeof(aDL)); std::memset(aDR, 0, sizeof(aDR));
        std::memset(aEL, 0, sizeof(aEL)); std::memset(aER, 0, sizeof(aER));
        std::memset(aFL, 0, sizeof(aFL)); std::memset(aFR, 0, sizeof(aFR));
        std::memset(aGL, 0, sizeof(aGL)); std::memset(aGR, 0, sizeof(aGR));
        std::memset(aHL, 0, sizeof(aHL)); std::memset(aHR, 0, sizeof(aHR));
        std::memset(aIL, 0, sizeof(aIL)); std::memset(aIR, 0, sizeof(aIR));
        std::memset(aJL, 0, sizeof(aJL)); std::memset(aJR, 0, sizeof(aJR));
        std::memset(aKL, 0, sizeof(aKL)); std::memset(aKR, 0, sizeof(aKR));
        std::memset(aLL, 0, sizeof(aLL)); std::memset(aLR, 0, sizeof(aLR));
        std::memset(aML, 0, sizeof(aML)); std::memset(aMR, 0, sizeof(aMR));
        std::memset(aNL, 0, sizeof(aNL)); std::memset(aNR, 0, sizeof(aNR));
        std::memset(aOL, 0, sizeof(aOL)); std::memset(aOR, 0, sizeof(aOR));
        std::memset(aPL, 0, sizeof(aPL)); std::memset(aPR, 0, sizeof(aPR));
        std::memset(aQL, 0, sizeof(aQL)); std::memset(aQR, 0, sizeof(aQR));
        std::memset(aRL, 0, sizeof(aRL)); std::memset(aRR, 0, sizeof(aRR));
        std::memset(aSL, 0, sizeof(aSL)); std::memset(aSR, 0, sizeof(aSR));
        std::memset(aTL, 0, sizeof(aTL)); std::memset(aTR, 0, sizeof(aTR));
        std::memset(aUL, 0, sizeof(aUL)); std::memset(aUR, 0, sizeof(aUR));
        std::memset(aVL, 0, sizeof(aVL)); std::memset(aVR, 0, sizeof(aVR));
        std::memset(aWL, 0, sizeof(aWL)); std::memset(aWR, 0, sizeof(aWR));
        std::memset(aXL, 0, sizeof(aXL)); std::memset(aXR, 0, sizeof(aXR));
        std::memset(aYL, 0, sizeof(aYL)); std::memset(aYR, 0, sizeof(aYR));
        std::memset(aZL, 0, sizeof(aZL)); std::memset(aZR, 0, sizeof(aZR));

        feedbackAL = feedbackBL = feedbackCL = feedbackDL = feedbackEL = 0.0;
        feedbackER = feedbackJR = feedbackOR = feedbackTR = feedbackYR = 0.0;
        previousAL = previousBL = previousCL = previousDL = previousEL = 0.0;
        previousAR = previousBR = previousCR = previousDR = previousER = 0.0;

        prevMulchBL = prevMulchBR = prevMulchCL = prevMulchCR = 0.0;
        prevMulchDL = prevMulchDR = prevMulchEL = prevMulchER = 0.0;
        prevOutDL = prevOutDR = prevOutEL = prevOutER = 0.0;
        prevInDL = prevInDR = prevInEL = prevInER = 0.0;

        std::memset(lastRefL, 0, sizeof(lastRefL));
        std::memset(lastRefR, 0, sizeof(lastRefR));
        std::memset(fixA, 0, sizeof(fixA));
        std::memset(fixB, 0, sizeof(fixB));
        std::memset(fixC, 0, sizeof(fixC));
        std::memset(fixD, 0, sizeof(fixD));

        earlyAL=earlyBL=earlyCL=earlyDL=earlyEL=earlyFL=earlyGL=earlyHL=earlyIL=1;
        earlyAR=earlyBR=earlyCR=earlyDR=earlyER=earlyFR=earlyGR=earlyHR=earlyIR=1;
        countAL=countBL=countCL=countDL=countEL=countFL=countGL=countHL=countIL=1;
        countJL=countKL=countLL=countML=countNL=countOL=countPL=countQL=countRL=1;
        countSL=countTL=countUL=countVL=countWL=countXL=countYL=1;
        countAR=countBR=countCR=countDR=countER=countFR=countGR=countHR=countIR=1;
        countJR=countKR=countLR=countMR=countNR=countOR=countPR=countQR=countRR=1;
        countSR=countTR=countUR=countVR=countWR=countXR=countYR=1;
        countZ = 1;
        cycle = 0;

        fpdL = 1; while (fpdL < 16386) fpdL = (uint32_t)(std::rand()) * (uint32_t)(std::rand());
        fpdR = 1; while (fpdR < 16386) fpdR = (uint32_t)(std::rand()) * (uint32_t)(std::rand());
        decayLevel = 0.0f;
    }

    // ==============================================================================
    void process (juce::AudioBuffer<float>& buffer)
    {
        if (bypassed) return;

        auto p = atomicParams.load();
        const int numSamples = buffer.getNumSamples();
        const int numCh = juce::jmin(2, buffer.getNumChannels());
        if (numCh < 1) return;

        float* leftCh  = buffer.getWritePointer(0);
        float* rightCh = numCh > 1 ? buffer.getWritePointer(1) : leftCh;

        double overallscale = 1.0;
        overallscale /= 44100.0;
        overallscale *= sampleRate;
        int cycleEnd = (int)std::floor(overallscale);
        if (cycleEnd < 1) cycleEnd = 1;
        if (cycleEnd > 4) cycleEnd = 4;
        if (cycle > cycleEnd - 1) cycle = cycleEnd - 1;

        double downRate = sampleRate / cycleEnd;

        double inputPad = (double)p.inputPad;
        double regen = ((double)p.damping * 0.425) + 0.16;
        regen = (regen * 0.0001) + 0.00024;
        double iirAmount = ((double)p.lowCut * 0.3) + 0.04;
        iirAmount = (iirAmount * 1000.0) / downRate;
        double earlyVolume = std::pow((double)p.predelay, 2.0) * 0.5;
        int adjPredelay = (int)(downRate * earlyVolume);
        if (adjPredelay > kPredelay) adjPredelay = kPredelay;

        double wet = (double)p.wetness * 2.0;
        double dry = 2.0 - wet;
        if (wet > 1.0) wet = 1.0; if (wet < 0.0) wet = 0.0;
        if (dry > 1.0) dry = 1.0; if (dry < 0.0) dry = 0.0;

        // Biquad bandpass filter coefficients
        fixA[fix_freq] = 20.0 / downRate;
        fixA[fix_reso] = 0.0018769;
        fixD[fix_freq] = 14.0 / downRate;
        fixD[fix_reso] = 0.0024964;
        fixB[fix_freq] = (fixA[fix_freq] + fixA[fix_freq] + fixD[fix_freq]) / 3.0;
        fixB[fix_reso] = 0.0020834;
        fixC[fix_freq] = (fixA[fix_freq] + fixD[fix_freq] + fixD[fix_freq]) / 3.0;
        fixC[fix_reso] = 0.0022899;

        auto computeBiquad = [](double* fix) {
            double K = std::tan(M_PI * fix[fix_freq]);
            double norm = 1.0 / (1.0 + K / fix[fix_reso] + K * K);
            fix[fix_a0] = K / fix[fix_reso] * norm;
            fix[fix_a1] = 0.0;
            fix[fix_a2] = -fix[fix_a0];
            fix[fix_b1] = 2.0 * (K * K - 1.0) * norm;
            fix[fix_b2] = (1.0 - K / fix[fix_reso] + K * K) * norm;
        };
        computeBiquad(fixA); computeBiquad(fixB);
        computeBiquad(fixC); computeBiquad(fixD);

        float peakLevel = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            double inputSampleL = (double)leftCh[i];
            double inputSampleR = (double)rightCh[i];
            if (std::fabs(inputSampleL) < 1.18e-23) inputSampleL = fpdL * 1.18e-17;
            if (std::fabs(inputSampleR) < 1.18e-23) inputSampleR = fpdR * 1.18e-17;
            double drySampleL = inputSampleL;
            double drySampleR = inputSampleR;

            cycle++;
            if (cycle == cycleEnd)
            {
                if (inputPad < 1.0) { inputSampleL *= inputPad; inputSampleR *= inputPad; }

                double outSample;
                // 10k input filter
                outSample = (inputSampleL + prevInDL) * 0.5; prevInDL = inputSampleL; inputSampleL = outSample;
                outSample = (inputSampleR + prevInDR) * 0.5; prevInDR = inputSampleR; inputSampleR = outSample;

                // Soft clip
                if (inputSampleL > 1.57079633) inputSampleL = 1.57079633;
                if (inputSampleL < -1.57079633) inputSampleL = -1.57079633;
                if (inputSampleR > 1.57079633) inputSampleR = 1.57079633;
                if (inputSampleR < -1.57079633) inputSampleR = -1.57079633;
                inputSampleL = std::sin(inputSampleL);
                inputSampleR = std::sin(inputSampleR);

                // Highpass
                iirAL = (iirAL * (1.0 - iirAmount)) + (inputSampleL * iirAmount);
                inputSampleL -= iirAL;
                iirAR = (iirAR * (1.0 - iirAmount)) + (inputSampleR * iirAmount);
                inputSampleR -= iirAR;

                // Input compressor
                inputSampleL *= 0.5; inputSampleR *= 0.5;
                if (gainIn < 0.0078125) gainIn = 0.0078125; if (gainIn > 1.0) gainIn = 1.0;
                inputSampleL *= gainIn; inputSampleR *= gainIn;
                gainIn += std::sin((std::fabs(inputSampleL*4)>1)?4:std::fabs(inputSampleL*4))*std::pow(inputSampleL,4);
                gainIn += std::sin((std::fabs(inputSampleR*4)>1)?4:std::fabs(inputSampleR*4))*std::pow(inputSampleR,4);

                // Second highpass
                iirBL = (iirBL * (1.0 - iirAmount)) + (inputSampleL * iirAmount);
                inputSampleL -= iirBL;
                iirBR = (iirBR * (1.0 - iirAmount)) + (inputSampleR * iirAmount);
                inputSampleR -= iirBR;

                // Second 10k filter
                outSample = (inputSampleL + prevInEL) * 0.5; prevInEL = inputSampleL; inputSampleL = outSample;
                outSample = (inputSampleR + prevInER) * 0.5; prevInER = inputSampleR; inputSampleR = outSample;

                // ============ EARLY REFLECTIONS (3 stages of 3 allpasses) ============
                // Stage 1: L→A,B,C  R→C,F,I
                double oeAL = inputSampleL - (eAL[(earlyAL+1)-((earlyAL+1>kEarlyA)?kEarlyA+1:0)]*0.5);
                double oeBL = inputSampleL - (eBL[(earlyBL+1)-((earlyBL+1>kEarlyB)?kEarlyB+1:0)]*0.5);
                double oeCL = inputSampleL - (eCL[(earlyCL+1)-((earlyCL+1>kEarlyC)?kEarlyC+1:0)]*0.5);
                double oeCR = inputSampleR - (eCR[(earlyCR+1)-((earlyCR+1>kEarlyC)?kEarlyC+1:0)]*0.5);
                double oeFR = inputSampleR - (eFR[(earlyFR+1)-((earlyFR+1>kEarlyF)?kEarlyF+1:0)]*0.5);
                double oeIR = inputSampleR - (eIR[(earlyIR+1)-((earlyIR+1>kEarlyI)?kEarlyI+1:0)]*0.5);

                eAL[earlyAL] = oeAL; oeAL *= 0.5;
                eBL[earlyBL] = oeBL; oeBL *= 0.5;
                eCL[earlyCL] = oeCL; oeCL *= 0.5;
                eCR[earlyCR] = oeCR; oeCR *= 0.5;
                eFR[earlyFR] = oeFR; oeFR *= 0.5;
                eIR[earlyIR] = oeIR; oeIR *= 0.5;

                earlyAL++; if (earlyAL<0||earlyAL>kEarlyA) earlyAL=0;
                earlyBL++; if (earlyBL<0||earlyBL>kEarlyB) earlyBL=0;
                earlyCL++; if (earlyCL<0||earlyCL>kEarlyC) earlyCL=0;
                earlyCR++; if (earlyCR<0||earlyCR>kEarlyC) earlyCR=0;
                earlyFR++; if (earlyFR<0||earlyFR>kEarlyF) earlyFR=0;
                earlyIR++; if (earlyIR<0||earlyIR>kEarlyI) earlyIR=0;

                oeAL += eAL[earlyAL-((earlyAL>kEarlyA)?kEarlyA+1:0)];
                oeBL += eBL[earlyBL-((earlyBL>kEarlyB)?kEarlyB+1:0)];
                oeCL += eCL[earlyCL-((earlyCL>kEarlyC)?kEarlyC+1:0)];
                oeCR += eCR[earlyCR-((earlyCR>kEarlyC)?kEarlyC+1:0)];
                oeFR += eFR[earlyFR-((earlyFR>kEarlyF)?kEarlyF+1:0)];
                oeIR += eIR[earlyIR-((earlyIR>kEarlyI)?kEarlyI+1:0)];

                // Stage 2: L→D,E,F  R→B,E,H
                double oeDL = ((oeBL+oeCL)-oeAL) - (eDL[(earlyDL+1)-((earlyDL+1>kEarlyD)?kEarlyD+1:0)]*0.5);
                double oeEL = ((oeAL+oeCL)-oeBL) - (eEL[(earlyEL+1)-((earlyEL+1>kEarlyE)?kEarlyE+1:0)]*0.5);
                double oeFL = ((oeAL+oeBL)-oeCL) - (eFL[(earlyFL+1)-((earlyFL+1>kEarlyF)?kEarlyF+1:0)]*0.5);
                double oeBR = ((oeFR+oeIR)-oeCR) - (eBR[(earlyBR+1)-((earlyBR+1>kEarlyB)?kEarlyB+1:0)]*0.5);
                double oeER = ((oeCR+oeIR)-oeFR) - (eER[(earlyER+1)-((earlyER+1>kEarlyE)?kEarlyE+1:0)]*0.5);
                double oeHR = ((oeCR+oeFR)-oeIR) - (eHR[(earlyHR+1)-((earlyHR+1>kEarlyH)?kEarlyH+1:0)]*0.5);

                eDL[earlyDL] = oeDL; oeDL *= 0.5;
                eEL[earlyEL] = oeEL; oeEL *= 0.5;
                eFL[earlyFL] = oeFL; oeFL *= 0.5;
                eBR[earlyBR] = oeBR; oeBR *= 0.5;
                eER[earlyER] = oeER; oeER *= 0.5;
                eHR[earlyHR] = oeHR; oeHR *= 0.5;

                earlyDL++; if (earlyDL<0||earlyDL>kEarlyD) earlyDL=0;
                earlyEL++; if (earlyEL<0||earlyEL>kEarlyE) earlyEL=0;
                earlyFL++; if (earlyFL<0||earlyFL>kEarlyF) earlyFL=0;
                earlyBR++; if (earlyBR<0||earlyBR>kEarlyB) earlyBR=0;
                earlyER++; if (earlyER<0||earlyER>kEarlyE) earlyER=0;
                earlyHR++; if (earlyHR<0||earlyHR>kEarlyH) earlyHR=0;

                oeDL += eDL[earlyDL-((earlyDL>kEarlyD)?kEarlyD+1:0)];
                oeEL += eEL[earlyEL-((earlyEL>kEarlyE)?kEarlyE+1:0)];
                oeFL += eFL[earlyFL-((earlyFL>kEarlyF)?kEarlyF+1:0)];
                oeBR += eBR[earlyBR-((earlyBR>kEarlyB)?kEarlyB+1:0)];
                oeER += eER[earlyER-((earlyER>kEarlyE)?kEarlyE+1:0)];
                oeHR += eHR[earlyHR-((earlyHR>kEarlyH)?kEarlyH+1:0)];

                // Stage 3: L→G,H,I  R→A,D,G
                double oeGL = ((oeEL+oeFL)-oeDL) - (eGL[(earlyGL+1)-((earlyGL+1>kEarlyG)?kEarlyG+1:0)]*0.5);
                double oeHL = ((oeDL+oeFL)-oeEL) - (eHL[(earlyHL+1)-((earlyHL+1>kEarlyH)?kEarlyH+1:0)]*0.5);
                double oeIL = ((oeDL+oeEL)-oeFL) - (eIL[(earlyIL+1)-((earlyIL+1>kEarlyI)?kEarlyI+1:0)]*0.5);
                double oeAR = ((oeER+oeHR)-oeBR) - (eAR[(earlyAR+1)-((earlyAR+1>kEarlyA)?kEarlyA+1:0)]*0.5);
                double oeDR = ((oeBR+oeHR)-oeER) - (eDR[(earlyDR+1)-((earlyDR+1>kEarlyD)?kEarlyD+1:0)]*0.5);
                double oeGR = ((oeBR+oeER)-oeHR) - (eGR[(earlyGR+1)-((earlyGR+1>kEarlyG)?kEarlyG+1:0)]*0.5);

                eGL[earlyGL] = oeGL; oeGL *= 0.5;
                eHL[earlyHL] = oeHL; oeHL *= 0.5;
                eIL[earlyIL] = oeIL; oeIL *= 0.5;
                eAR[earlyAR] = oeAR; oeAR *= 0.5;
                eDR[earlyDR] = oeDR; oeDR *= 0.5;
                eGR[earlyGR] = oeGR; oeGR *= 0.5;

                earlyGL++; if (earlyGL<0||earlyGL>kEarlyG) earlyGL=0;
                earlyHL++; if (earlyHL<0||earlyHL>kEarlyH) earlyHL=0;
                earlyIL++; if (earlyIL<0||earlyIL>kEarlyI) earlyIL=0;
                earlyAR++; if (earlyAR<0||earlyAR>kEarlyA) earlyAR=0;
                earlyDR++; if (earlyDR<0||earlyDR>kEarlyD) earlyDR=0;
                earlyGR++; if (earlyGR<0||earlyGR>kEarlyG) earlyGR=0;

                oeGL += eGL[earlyGL-((earlyGL>kEarlyG)?kEarlyG+1:0)];
                oeHL += eHL[earlyHL-((earlyHL>kEarlyH)?kEarlyH+1:0)];
                oeIL += eIL[earlyIL-((earlyIL>kEarlyI)?kEarlyI+1:0)];
                oeAR += eAR[earlyAR-((earlyAR>kEarlyA)?kEarlyA+1:0)];
                oeDR += eDR[earlyDR-((earlyDR>kEarlyD)?kEarlyD+1:0)];
                oeGR += eGR[earlyGR-((earlyGR>kEarlyG)?kEarlyG+1:0)];

                // Predelay Z
                aZL[countZ] = (oeGL + oeHL + oeIL) * 0.25;
                aZR[countZ] = (oeAR + oeDR + oeGR) * 0.25;
                countZ++; if (countZ < 0 || countZ > adjPredelay) countZ = 0;
                inputSampleL = aZL[countZ-((countZ>adjPredelay)?adjPredelay+1:0)];
                inputSampleR = aZR[countZ-((countZ>adjPredelay)?adjPredelay+1:0)];

                // ============ LATE REVERB: 5 cascaded 5×5 Householder blocks ============

                // Block 1: L→A,B,C,D,E  R→E,J,O,T,Y (inject input + feedback)
                aAL[countAL] = inputSampleL + (feedbackAL * regen);
                aBL[countBL] = inputSampleL + (feedbackBL * regen);
                aCL[countCL] = inputSampleL + (feedbackCL * regen);
                aDL[countDL] = inputSampleL + (feedbackDL * regen);
                aEL[countEL] = inputSampleL + (feedbackEL * regen);

                aER[countER] = inputSampleR + (feedbackER * regen);
                aJR[countJR] = inputSampleR + (feedbackJR * regen);
                aOR[countOR] = inputSampleR + (feedbackOR * regen);
                aTR[countTR] = inputSampleR + (feedbackTR * regen);
                aYR[countYR] = inputSampleR + (feedbackYR * regen);

                countAL++; if (countAL<0||countAL>kDelayA) countAL=0;
                countBL++; if (countBL<0||countBL>kDelayB) countBL=0;
                countCL++; if (countCL<0||countCL>kDelayC) countCL=0;
                countDL++; if (countDL<0||countDL>kDelayD) countDL=0;
                countEL++; if (countEL<0||countEL>kDelayE) countEL=0;
                countER++; if (countER<0||countER>kDelayE) countER=0;
                countJR++; if (countJR<0||countJR>kDelayJ) countJR=0;
                countOR++; if (countOR<0||countOR>kDelayO) countOR=0;
                countTR++; if (countTR<0||countTR>kDelayT) countTR=0;
                countYR++; if (countYR<0||countYR>kDelayY) countYR=0;

                double outAL = aAL[countAL-((countAL>kDelayA)?kDelayA+1:0)];
                double outBL = aBL[countBL-((countBL>kDelayB)?kDelayB+1:0)];
                double outCL = aCL[countCL-((countCL>kDelayC)?kDelayC+1:0)];
                double outDL = aDL[countDL-((countDL>kDelayD)?kDelayD+1:0)];
                double outEL = aEL[countEL-((countEL>kDelayE)?kDelayE+1:0)];
                double outER = aER[countER-((countER>kDelayE)?kDelayE+1:0)];
                double outJR = aJR[countJR-((countJR>kDelayJ)?kDelayJ+1:0)];
                double outOR = aOR[countOR-((countOR>kDelayO)?kDelayO+1:0)];
                double outTR = aTR[countTR-((countTR>kDelayT)?kDelayT+1:0)];
                double outYR = aYR[countYR-((countYR>kDelayY)?kDelayY+1:0)];

                // Biquad A filtering
                outSample = (outAL*fixA[fix_a0])+fixA[fix_sL1];
                fixA[fix_sL1] = (outAL*fixA[fix_a1])-(outSample*fixA[fix_b1])+fixA[fix_sL2];
                fixA[fix_sL2] = (outAL*fixA[fix_a2])-(outSample*fixA[fix_b2]); outAL = outSample;
                outSample = (outER*fixA[fix_a0])+fixA[fix_sR1];
                fixA[fix_sR1] = (outER*fixA[fix_a1])-(outSample*fixA[fix_b1])+fixA[fix_sR2];
                fixA[fix_sR2] = (outER*fixA[fix_a2])-(outSample*fixA[fix_b2]); outER = outSample;

                // Block 2: 5×5 Householder
                aFL[countFL] = ((outAL*3.0)-((outBL+outCL+outDL+outEL)*2.0));
                aGL[countGL] = ((outBL*3.0)-((outAL+outCL+outDL+outEL)*2.0));
                aHL[countHL] = ((outCL*3.0)-((outAL+outBL+outDL+outEL)*2.0));
                aIL[countIL] = ((outDL*3.0)-((outAL+outBL+outCL+outEL)*2.0));
                aJL[countJL] = ((outEL*3.0)-((outAL+outBL+outCL+outDL)*2.0));
                aDR[countDR] = ((outER*3.0)-((outJR+outOR+outTR+outYR)*2.0));
                aIR[countIR] = ((outJR*3.0)-((outER+outOR+outTR+outYR)*2.0));
                aNR[countNR] = ((outOR*3.0)-((outER+outJR+outTR+outYR)*2.0));
                aSR[countSR] = ((outTR*3.0)-((outER+outJR+outOR+outYR)*2.0));
                aXR[countXR] = ((outYR*3.0)-((outER+outJR+outOR+outTR)*2.0));

                countFL++; if (countFL<0||countFL>kDelayF) countFL=0;
                countGL++; if (countGL<0||countGL>kDelayG) countGL=0;
                countHL++; if (countHL<0||countHL>kDelayH) countHL=0;
                countIL++; if (countIL<0||countIL>kDelayI) countIL=0;
                countJL++; if (countJL<0||countJL>kDelayJ) countJL=0;
                countDR++; if (countDR<0||countDR>kDelayD) countDR=0;
                countIR++; if (countIR<0||countIR>kDelayI) countIR=0;
                countNR++; if (countNR<0||countNR>kDelayN) countNR=0;
                countSR++; if (countSR<0||countSR>kDelayS) countSR=0;
                countXR++; if (countXR<0||countXR>kDelayX) countXR=0;

                double outFL = aFL[countFL-((countFL>kDelayF)?kDelayF+1:0)];
                double outGL = aGL[countGL-((countGL>kDelayG)?kDelayG+1:0)];
                double outHL = aHL[countHL-((countHL>kDelayH)?kDelayH+1:0)];
                double outIL = aIL[countIL-((countIL>kDelayI)?kDelayI+1:0)];
                double outJL = aJL[countJL-((countJL>kDelayJ)?kDelayJ+1:0)];
                double outDR = aDR[countDR-((countDR>kDelayD)?kDelayD+1:0)];
                double outIR = aIR[countIR-((countIR>kDelayI)?kDelayI+1:0)];
                double outNR = aNR[countNR-((countNR>kDelayN)?kDelayN+1:0)];
                double outSR = aSR[countSR-((countSR>kDelayS)?kDelayS+1:0)];
                double outXR = aXR[countXR-((countXR>kDelayX)?kDelayX+1:0)];

                // Biquad B + mulch B
                outSample = (outFL*fixB[fix_a0])+fixB[fix_sL1];
                fixB[fix_sL1] = (outFL*fixB[fix_a1])-(outSample*fixB[fix_b1])+fixB[fix_sL2];
                fixB[fix_sL2] = (outFL*fixB[fix_a2])-(outSample*fixB[fix_b2]); outFL = outSample;
                outSample = (outDR*fixB[fix_a0])+fixB[fix_sR1];
                fixB[fix_sR1] = (outDR*fixB[fix_a1])-(outSample*fixB[fix_b1])+fixB[fix_sR2];
                fixB[fix_sR2] = (outDR*fixB[fix_a2])-(outSample*fixB[fix_b2]); outDR = outSample;
                outSample = (outGL+prevMulchBL)*0.5; prevMulchBL = outGL; outGL = outSample;
                outSample = (outIR+prevMulchBR)*0.5; prevMulchBR = outIR; outIR = outSample;

                // Block 3: 5×5 Householder
                aKL[countKL] = ((outFL*3.0)-((outGL+outHL+outIL+outJL)*2.0));
                aLL[countLL] = ((outGL*3.0)-((outFL+outHL+outIL+outJL)*2.0));
                aML[countML] = ((outHL*3.0)-((outFL+outGL+outIL+outJL)*2.0));
                aNL[countNL] = ((outIL*3.0)-((outFL+outGL+outHL+outJL)*2.0));
                aOL[countOL] = ((outJL*3.0)-((outFL+outGL+outHL+outIL)*2.0));
                aCR[countCR] = ((outDR*3.0)-((outIR+outNR+outSR+outXR)*2.0));
                aHR[countHR] = ((outIR*3.0)-((outDR+outNR+outSR+outXR)*2.0));
                aMR[countMR] = ((outNR*3.0)-((outDR+outIR+outSR+outXR)*2.0));
                aRR[countRR] = ((outSR*3.0)-((outDR+outIR+outNR+outXR)*2.0));
                aWR[countWR] = ((outXR*3.0)-((outDR+outIR+outNR+outSR)*2.0));

                countKL++; if (countKL<0||countKL>kDelayK) countKL=0;
                countLL++; if (countLL<0||countLL>kDelayL) countLL=0;
                countML++; if (countML<0||countML>kDelayM) countML=0;
                countNL++; if (countNL<0||countNL>kDelayN) countNL=0;
                countOL++; if (countOL<0||countOL>kDelayO) countOL=0;
                countCR++; if (countCR<0||countCR>kDelayC) countCR=0;
                countHR++; if (countHR<0||countHR>kDelayH) countHR=0;
                countMR++; if (countMR<0||countMR>kDelayM) countMR=0;
                countRR++; if (countRR<0||countRR>kDelayR) countRR=0;
                countWR++; if (countWR<0||countWR>kDelayW) countWR=0;

                double outKL = aKL[countKL-((countKL>kDelayK)?kDelayK+1:0)];
                double outLL = aLL[countLL-((countLL>kDelayL)?kDelayL+1:0)];
                double outML = aML[countML-((countML>kDelayM)?kDelayM+1:0)];
                double outNL = aNL[countNL-((countNL>kDelayN)?kDelayN+1:0)];
                double outOL = aOL[countOL-((countOL>kDelayO)?kDelayO+1:0)];
                double outCR = aCR[countCR-((countCR>kDelayC)?kDelayC+1:0)];
                double outHR = aHR[countHR-((countHR>kDelayH)?kDelayH+1:0)];
                double outMR = aMR[countMR-((countMR>kDelayM)?kDelayM+1:0)];
                double outRR = aRR[countRR-((countRR>kDelayR)?kDelayR+1:0)];
                double outWR = aWR[countWR-((countWR>kDelayW)?kDelayW+1:0)];

                // Biquad C + mulch C
                outSample = (outKL*fixC[fix_a0])+fixC[fix_sL1];
                fixC[fix_sL1] = (outKL*fixC[fix_a1])-(outSample*fixC[fix_b1])+fixC[fix_sL2];
                fixC[fix_sL2] = (outKL*fixC[fix_a2])-(outSample*fixC[fix_b2]); outKL = outSample;
                outSample = (outCR*fixC[fix_a0])+fixC[fix_sR1];
                fixC[fix_sR1] = (outCR*fixC[fix_a1])-(outSample*fixC[fix_b1])+fixC[fix_sR2];
                fixC[fix_sR2] = (outCR*fixC[fix_a2])-(outSample*fixC[fix_b2]); outCR = outSample;
                outSample = (outLL+prevMulchCL)*0.5; prevMulchCL = outLL; outLL = outSample;
                outSample = (outHR+prevMulchCR)*0.5; prevMulchCR = outHR; outHR = outSample;

                // Block 4: 5×5 Householder
                aPL[countPL] = ((outKL*3.0)-((outLL+outML+outNL+outOL)*2.0));
                aQL[countQL] = ((outLL*3.0)-((outKL+outML+outNL+outOL)*2.0));
                aRL[countRL] = ((outML*3.0)-((outKL+outLL+outNL+outOL)*2.0));
                aSL[countSL] = ((outNL*3.0)-((outKL+outLL+outML+outOL)*2.0));
                aTL[countTL] = ((outOL*3.0)-((outKL+outLL+outML+outNL)*2.0));
                aBR[countBR] = ((outCR*3.0)-((outHR+outMR+outRR+outWR)*2.0));
                aGR[countGR] = ((outHR*3.0)-((outCR+outMR+outRR+outWR)*2.0));
                aLR[countLR] = ((outMR*3.0)-((outCR+outHR+outRR+outWR)*2.0));
                aQR[countQR] = ((outRR*3.0)-((outCR+outHR+outMR+outWR)*2.0));
                aVR[countVR] = ((outWR*3.0)-((outCR+outHR+outMR+outRR)*2.0));

                countPL++; if (countPL<0||countPL>kDelayP) countPL=0;
                countQL++; if (countQL<0||countQL>kDelayQ) countQL=0;
                countRL++; if (countRL<0||countRL>kDelayR) countRL=0;
                countSL++; if (countSL<0||countSL>kDelayS) countSL=0;
                countTL++; if (countTL<0||countTL>kDelayT) countTL=0;
                countBR++; if (countBR<0||countBR>kDelayB) countBR=0;
                countGR++; if (countGR<0||countGR>kDelayG) countGR=0;
                countLR++; if (countLR<0||countLR>kDelayL) countLR=0;
                countQR++; if (countQR<0||countQR>kDelayQ) countQR=0;
                countVR++; if (countVR<0||countVR>kDelayV) countVR=0;

                double outPL = aPL[countPL-((countPL>kDelayP)?kDelayP+1:0)];
                double outQL = aQL[countQL-((countQL>kDelayQ)?kDelayQ+1:0)];
                double outRL = aRL[countRL-((countRL>kDelayR)?kDelayR+1:0)];
                double outSL = aSL[countSL-((countSL>kDelayS)?kDelayS+1:0)];
                double outTL = aTL[countTL-((countTL>kDelayT)?kDelayT+1:0)];
                double outBR = aBR[countBR-((countBR>kDelayB)?kDelayB+1:0)];
                double outGR = aGR[countGR-((countGR>kDelayG)?kDelayG+1:0)];
                double outLR = aLR[countLR-((countLR>kDelayL)?kDelayL+1:0)];
                double outQR = aQR[countQR-((countQR>kDelayQ)?kDelayQ+1:0)];
                double outVR = aVR[countVR-((countVR>kDelayV)?kDelayV+1:0)];

                // Biquad D + mulch D
                outSample = (outPL*fixD[fix_a0])+fixD[fix_sL1];
                fixD[fix_sL1] = (outPL*fixD[fix_a1])-(outSample*fixD[fix_b1])+fixD[fix_sL2];
                fixD[fix_sL2] = (outPL*fixD[fix_a2])-(outSample*fixD[fix_b2]); outPL = outSample;
                outSample = (outBR*fixD[fix_a0])+fixD[fix_sR1];
                fixD[fix_sR1] = (outBR*fixD[fix_a1])-(outSample*fixD[fix_b1])+fixD[fix_sR2];
                fixD[fix_sR2] = (outBR*fixD[fix_a2])-(outSample*fixD[fix_b2]); outBR = outSample;
                outSample = (outQL+prevMulchDL)*0.5; prevMulchDL = outQL; outQL = outSample;
                outSample = (outGR+prevMulchDR)*0.5; prevMulchDR = outGR; outGR = outSample;

                // Block 5: final 5×5 Householder
                aUL[countUL] = ((outPL*3.0)-((outQL+outRL+outSL+outTL)*2.0));
                aVL[countVL] = ((outQL*3.0)-((outPL+outRL+outSL+outTL)*2.0));
                aWL[countWL] = ((outRL*3.0)-((outPL+outQL+outSL+outTL)*2.0));
                aXL[countXL] = ((outSL*3.0)-((outPL+outQL+outRL+outTL)*2.0));
                aYL[countYL] = ((outTL*3.0)-((outPL+outQL+outRL+outSL)*2.0));
                aAR[countAR] = ((outBR*3.0)-((outGR+outLR+outQR+outVR)*2.0));
                aFR[countFR] = ((outGR*3.0)-((outBR+outLR+outQR+outVR)*2.0));
                aKR[countKR] = ((outLR*3.0)-((outBR+outGR+outQR+outVR)*2.0));
                aPR[countPR] = ((outQR*3.0)-((outBR+outGR+outLR+outVR)*2.0));
                aUR[countUR] = ((outVR*3.0)-((outBR+outGR+outLR+outQR)*2.0));

                countUL++; if (countUL<0||countUL>kDelayU) countUL=0;
                countVL++; if (countVL<0||countVL>kDelayV) countVL=0;
                countWL++; if (countWL<0||countWL>kDelayW) countWL=0;
                countXL++; if (countXL<0||countXL>kDelayX) countXL=0;
                countYL++; if (countYL<0||countYL>kDelayY) countYL=0;
                countAR++; if (countAR<0||countAR>kDelayA) countAR=0;
                countFR++; if (countFR<0||countFR>kDelayF) countFR=0;
                countKR++; if (countKR<0||countKR>kDelayK) countKR=0;
                countPR++; if (countPR<0||countPR>kDelayP) countPR=0;
                countUR++; if (countUR<0||countUR>kDelayU) countUR=0;

                double outUL = aUL[countUL-((countUL>kDelayU)?kDelayU+1:0)];
                double outVL = aVL[countVL-((countVL>kDelayV)?kDelayV+1:0)];
                double outWL = aWL[countWL-((countWL>kDelayW)?kDelayW+1:0)];
                double outXL = aXL[countXL-((countXL>kDelayX)?kDelayX+1:0)];
                double outYL = aYL[countYL-((countYL>kDelayY)?kDelayY+1:0)];
                double outAR = aAR[countAR-((countAR>kDelayA)?kDelayA+1:0)];
                double outFR = aFR[countFR-((countFR>kDelayF)?kDelayF+1:0)];
                double outKR = aKR[countKR-((countKR>kDelayK)?kDelayK+1:0)];
                double outPR = aPR[countPR-((countPR>kDelayP)?kDelayP+1:0)];
                double outUR = aUR[countUR-((countUR>kDelayU)?kDelayU+1:0)];

                // Mulch E
                outSample = (outVL+prevMulchEL)*0.5; prevMulchEL = outVL; outVL = outSample;
                outSample = (outFR+prevMulchER)*0.5; prevMulchER = outFR; outFR = outSample;

                // Cross-channel feedback
                feedbackER = ((outUL*3.0)-((outVL+outWL+outXL+outYL)*2.0));
                feedbackAL = ((outAR*3.0)-((outFR+outKR+outPR+outUR)*2.0));
                feedbackJR = ((outVL*3.0)-((outUL+outWL+outXL+outYL)*2.0));
                feedbackBL = ((outFR*3.0)-((outAR+outKR+outPR+outUR)*2.0));
                feedbackOR = ((outWL*3.0)-((outUL+outVL+outXL+outYL)*2.0));
                feedbackCL = ((outKR*3.0)-((outAR+outFR+outPR+outUR)*2.0));
                feedbackTR = ((outXL*3.0)-((outUL+outVL+outWL+outYL)*2.0));
                feedbackDL = ((outPR*3.0)-((outAR+outFR+outKR+outUR)*2.0));
                feedbackYR = ((outYL*3.0)-((outUL+outVL+outWL+outXL)*2.0));
                feedbackEL = ((outUR*3.0)-((outAR+outFR+outKR+outPR)*2.0));

                // Final output sum
                inputSampleL = (outUL + outVL + outWL + outXL + outYL) * 0.0016;
                inputSampleR = (outAR + outFR + outKR + outPR + outUR) * 0.0016;

                // Output compressor
                inputSampleL *= 0.5; inputSampleR *= 0.5;
                if (gainOutL < 0.0078125) gainOutL = 0.0078125; if (gainOutL > 1.0) gainOutL = 1.0;
                if (gainOutR < 0.0078125) gainOutR = 0.0078125; if (gainOutR > 1.0) gainOutR = 1.0;
                inputSampleL *= gainOutL; inputSampleR *= gainOutR;
                gainOutL += std::sin((std::fabs(inputSampleL*4)>1)?4:std::fabs(inputSampleL*4))*std::pow(inputSampleL,4);
                gainOutR += std::sin((std::fabs(inputSampleR*4)>1)?4:std::fabs(inputSampleR*4))*std::pow(inputSampleR,4);
                inputSampleL *= 2.0; inputSampleR *= 2.0;

                // Output averaging
                outSample = (inputSampleL+prevOutDL)*0.5; prevOutDL = inputSampleL; inputSampleL = outSample;
                outSample = (inputSampleR+prevOutDR)*0.5; prevOutDR = inputSampleR; inputSampleR = outSample;
                outSample = (inputSampleL+prevOutEL)*0.5; prevOutEL = inputSampleL; inputSampleL = outSample;
                outSample = (inputSampleR+prevOutER)*0.5; prevOutER = inputSampleR; inputSampleR = outSample;

                // Oversampling interpolation
                if (cycleEnd == 4) {
                    lastRefL[0]=lastRefL[4]; lastRefL[2]=(lastRefL[0]+inputSampleL)/2; lastRefL[1]=(lastRefL[0]+lastRefL[2])/2; lastRefL[3]=(lastRefL[2]+inputSampleL)/2; lastRefL[4]=inputSampleL;
                    lastRefR[0]=lastRefR[4]; lastRefR[2]=(lastRefR[0]+inputSampleR)/2; lastRefR[1]=(lastRefR[0]+lastRefR[2])/2; lastRefR[3]=(lastRefR[2]+inputSampleR)/2; lastRefR[4]=inputSampleR;
                }
                if (cycleEnd == 3) {
                    lastRefL[0]=lastRefL[3]; lastRefL[2]=(lastRefL[0]+lastRefL[0]+inputSampleL)/3; lastRefL[1]=(lastRefL[0]+inputSampleL+inputSampleL)/3; lastRefL[3]=inputSampleL;
                    lastRefR[0]=lastRefR[3]; lastRefR[2]=(lastRefR[0]+lastRefR[0]+inputSampleR)/3; lastRefR[1]=(lastRefR[0]+inputSampleR+inputSampleR)/3; lastRefR[3]=inputSampleR;
                }
                if (cycleEnd == 2) {
                    lastRefL[0]=lastRefL[2]; lastRefL[1]=(lastRefL[0]+inputSampleL)/2; lastRefL[2]=inputSampleL;
                    lastRefR[0]=lastRefR[2]; lastRefR[1]=(lastRefR[0]+inputSampleR)/2; lastRefR[2]=inputSampleR;
                }
                if (cycleEnd == 1) { lastRefL[0]=inputSampleL; lastRefR[0]=inputSampleR; }
                cycle = 0;
                inputSampleL = lastRefL[cycle]; inputSampleR = lastRefR[cycle];
            } else {
                inputSampleL = lastRefL[cycle]; inputSampleR = lastRefR[cycle];
            }

            // Hard clip + asin amplitude restore
            if (inputSampleL > 1.0) inputSampleL = 1.0; if (inputSampleL < -1.0) inputSampleL = -1.0;
            if (inputSampleR > 1.0) inputSampleR = 1.0; if (inputSampleR < -1.0) inputSampleR = -1.0;
            inputSampleL = std::asin(inputSampleL);
            inputSampleR = std::asin(inputSampleR);

            // Submix dry/wet
            if (wet < 1.0) { inputSampleL *= wet; inputSampleR *= wet; }
            if (dry < 1.0) { drySampleL *= dry; drySampleR *= dry; }
            inputSampleL += drySampleL; inputSampleR += drySampleR;

            // TPDF dither
            int expon; frexpf((float)inputSampleL, &expon);
            fpdL ^= fpdL << 13; fpdL ^= fpdL >> 17; fpdL ^= fpdL << 5;
            inputSampleL += ((double(fpdL)-uint32_t(0x7fffffff)) * 5.5e-36l * pow(2,expon+62));
            frexpf((float)inputSampleR, &expon);
            fpdR ^= fpdR << 13; fpdR ^= fpdR >> 17; fpdR ^= fpdR << 5;
            inputSampleR += ((double(fpdR)-uint32_t(0x7fffffff)) * 5.5e-36l * pow(2,expon+62));

            leftCh[i]  = (float)inputSampleL;
            rightCh[i] = (float)inputSampleR;

            float mx = juce::jmax(std::fabs((float)inputSampleL), std::fabs((float)inputSampleR));
            if (mx > peakLevel) peakLevel = mx;
        }

        decayLevel = decayLevel * 0.95f + peakLevel * 0.05f;
    }

private:
    // ==============================================================================
    AtomicParams atomicParams;
    double sampleRate = 44100.0;
    bool   bypassed   = false;
    float  decayLevel = 0.0f;

    // Delay size constants (from kPlateD)
    static constexpr int kEarlyA = 103, kEarlyB = 709, kEarlyC = 151, kEarlyD = 263;
    static constexpr int kEarlyE = 1433, kEarlyF = 593, kEarlyG = 1361, kEarlyH = 31, kEarlyI = 691;
    static constexpr int kPredelay = 24010;
    static constexpr int kDelayA = 619, kDelayB = 181, kDelayC = 101, kDelayD = 677;
    static constexpr int kDelayE = 401, kDelayF = 151, kDelayG = 409, kDelayH = 31;
    static constexpr int kDelayI = 641, kDelayJ = 661, kDelayK = 11, kDelayL = 691;
    static constexpr int kDelayM = 719, kDelayN = 17, kDelayO = 61, kDelayP = 743;
    static constexpr int kDelayQ = 89, kDelayR = 659, kDelayS = 5, kDelayT = 547;
    static constexpr int kDelayU = 769, kDelayV = 421, kDelayW = 47, kDelayX = 521, kDelayY = 163;

    // Biquad filter enum
    enum { fix_freq, fix_reso, fix_a0, fix_a1, fix_a2, fix_b1, fix_b2, fix_sL1, fix_sL2, fix_sR1, fix_sR2, fix_total };

    // IIR state
    double iirAL = 0, iirBL = 0, iirAR = 0, iirBR = 0;
    double gainIn = 1.0, gainOutL = 1.0, gainOutR = 1.0;

    // Early reflection buffers (9 allpass pairs)
    double eAL[kEarlyA+5]={}, eAR[kEarlyA+5]={};
    double eBL[kEarlyB+5]={}, eBR[kEarlyB+5]={};
    double eCL[kEarlyC+5]={}, eCR[kEarlyC+5]={};
    double eDL[kEarlyD+5]={}, eDR[kEarlyD+5]={};
    double eEL[kEarlyE+5]={}, eER[kEarlyE+5]={};
    double eFL[kEarlyF+5]={}, eFR[kEarlyF+5]={};
    double eGL[kEarlyG+5]={}, eGR[kEarlyG+5]={};
    double eHL[kEarlyH+5]={}, eHR[kEarlyH+5]={};
    double eIL[kEarlyI+5]={}, eIR[kEarlyI+5]={};

    // Late delay line buffers (25 pairs A-Y + predelay Z)
    double aAL[kDelayA+5]={}, aAR[kDelayA+5]={};
    double aBL[kDelayB+5]={}, aBR[kDelayB+5]={};
    double aCL[kDelayC+5]={}, aCR[kDelayC+5]={};
    double aDL[kDelayD+5]={}, aDR[kDelayD+5]={};
    double aEL[kDelayE+5]={}, aER[kDelayE+5]={};
    double aFL[kDelayF+5]={}, aFR[kDelayF+5]={};
    double aGL[kDelayG+5]={}, aGR[kDelayG+5]={};
    double aHL[kDelayH+5]={}, aHR[kDelayH+5]={};
    double aIL[kDelayI+5]={}, aIR[kDelayI+5]={};
    double aJL[kDelayJ+5]={}, aJR[kDelayJ+5]={};
    double aKL[kDelayK+5]={}, aKR[kDelayK+5]={};
    double aLL[kDelayL+5]={}, aLR[kDelayL+5]={};
    double aML[kDelayM+5]={}, aMR[kDelayM+5]={};
    double aNL[kDelayN+5]={}, aNR[kDelayN+5]={};
    double aOL[kDelayO+5]={}, aOR[kDelayO+5]={};
    double aPL[kDelayP+5]={}, aPR[kDelayP+5]={};
    double aQL[kDelayQ+5]={}, aQR[kDelayQ+5]={};
    double aRL[kDelayR+5]={}, aRR[kDelayR+5]={};
    double aSL[kDelayS+5]={}, aSR[kDelayS+5]={};
    double aTL[kDelayT+5]={}, aTR[kDelayT+5]={};
    double aUL[kDelayU+5]={}, aUR[kDelayU+5]={};
    double aVL[kDelayV+5]={}, aVR[kDelayV+5]={};
    double aWL[kDelayW+5]={}, aWR[kDelayW+5]={};
    double aXL[kDelayX+5]={}, aXR[kDelayX+5]={};
    double aYL[kDelayY+5]={}, aYR[kDelayY+5]={};
    double aZL[kPredelay+5]={}, aZR[kPredelay+5]={};

    // Feedback state
    double feedbackAL=0, feedbackBL=0, feedbackCL=0, feedbackDL=0, feedbackEL=0;
    double feedbackER=0, feedbackJR=0, feedbackOR=0, feedbackTR=0, feedbackYR=0;
    double previousAL=0, previousBL=0, previousCL=0, previousDL=0, previousEL=0;
    double previousAR=0, previousBR=0, previousCR=0, previousDR=0, previousER=0;

    // Mulch damping
    double prevMulchBL=0, prevMulchBR=0, prevMulchCL=0, prevMulchCR=0;
    double prevMulchDL=0, prevMulchDR=0, prevMulchEL=0, prevMulchER=0;
    double prevOutDL=0, prevOutDR=0, prevOutEL=0, prevOutER=0;
    double prevInDL=0, prevInDR=0, prevInEL=0, prevInER=0;

    // Oversampling refs
    double lastRefL[7] = {};
    double lastRefR[7] = {};

    // Biquad filter state
    double fixA[fix_total] = {};
    double fixB[fix_total] = {};
    double fixC[fix_total] = {};
    double fixD[fix_total] = {};

    // Early reflection counters (per-channel)
    int earlyAL=1,earlyBL=1,earlyCL=1,earlyDL=1,earlyEL=1,earlyFL=1,earlyGL=1,earlyHL=1,earlyIL=1;
    int earlyAR=1,earlyBR=1,earlyCR=1,earlyDR=1,earlyER=1,earlyFR=1,earlyGR=1,earlyHR=1,earlyIR=1;

    // Late delay counters (per-channel)
    int countAL=1,countBL=1,countCL=1,countDL=1,countEL=1,countFL=1,countGL=1,countHL=1,countIL=1;
    int countJL=1,countKL=1,countLL=1,countML=1,countNL=1,countOL=1,countPL=1,countQL=1,countRL=1;
    int countSL=1,countTL=1,countUL=1,countVL=1,countWL=1,countXL=1,countYL=1;
    int countAR=1,countBR=1,countCR=1,countDR=1,countER=1,countFR=1,countGR=1,countHR=1,countIR=1;
    int countJR=1,countKR=1,countLR=1,countMR=1,countNR=1,countOR=1,countPR=1,countQR=1,countRR=1;
    int countSR=1,countTR=1,countUR=1,countVR=1,countWR=1,countXR=1,countYR=1;
    int countZ = 1;
    int cycle = 0;

    // PRNG
    uint32_t fpdL = 1, fpdR = 1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlateReverbProcessor)
};
