// ==============================================================================
//  DoublerProcessor.h
//  OnStage — ADT (Automatic Double Tracking)
//
//  Based on Airwindows ADT by Chris Johnson (MIT License).
//  Two independent interpolated delay taps with Console-style
//  sin()/asin() saturation for analog-like richness.
//
//  Parameters (all 0-1):
//    Headroom, A Delay, A Level, B Delay, B Level, Output
//
//  Copyright (c) airwindows, MIT License. OnStage integration by Rob.
// ==============================================================================

#pragma once
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

class DoublerProcessor
{
public:
    struct Params
    {
        float headroom = 0.5f;   // A: 0-1 saturation headroom
        float delayA   = 0.5f;   // B: 0-1 first delay tap time
        float levelA   = 0.5f;   // C: 0-1 first tap intensity (0.5=off, >0.5=normal, <0.5=inverted)
        float delayB   = 0.5f;   // D: 0-1 second delay tap time
        float levelB   = 0.5f;   // E: 0-1 second tap intensity
        float output   = 0.5f;   // F: 0-1 output level

        bool operator== (const Params& o) const
        {
            return headroom == o.headroom && delayA == o.delayA && levelA == o.levelA
                && delayB == o.delayB && levelB == o.levelB && output == o.output;
        }
        bool operator!= (const Params& o) const { return !(*this == o); }
    };

    DoublerProcessor() = default;

    void prepare (const juce::dsp::ProcessSpec& /*spec*/)
    {
        for (int i = 0; i < 10000; ++i) { pL[i] = 0.0; pR[i] = 0.0; }
        offsetA = 9001.0;
        offsetB = 9001.0;
        gcount = 0;
        fpdL = 1557111;
        fpdR = 7891233;
    }

    void reset()
    {
        for (int i = 0; i < 10000; ++i) { pL[i] = 0.0; pR[i] = 0.0; }
        offsetA = 9001.0;
        offsetB = 9001.0;
        gcount = 0;
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        if (bypassed) return;

        const int N  = buffer.getNumSamples();
        const int ch = buffer.getNumChannels();
        if (ch < 1) return;

        float* dataL = buffer.getWritePointer (0);
        float* dataR = ch > 1 ? buffer.getWritePointer (1) : nullptr;

        // Parameter mapping (identical to Airwindows ADT)
        double gain = (double)params.headroom * 1.272;
        double targetA = std::pow ((double)params.delayA, 4.0) * 4790.0;
        double intensityA = (double)params.levelA - 0.5;
        double targetB = std::pow ((double)params.delayB, 4.0) * 4790.0;
        double intensityB = (double)params.levelB - 0.5;
        double outputLevel = (double)params.output * 2.0;

        for (int i = 0; i < N; ++i)
        {
            double inputSampleL = (double)dataL[i];
            double inputSampleR = dataR ? (double)dataR[i] : inputSampleL;

            // Denormal prevention
            if (std::abs (inputSampleL) < 1.18e-23) { fpdL ^= fpdL << 13; fpdL ^= fpdL >> 17; fpdL ^= fpdL << 5; inputSampleL = fpdL * 1.18e-17; }
            if (std::abs (inputSampleR) < 1.18e-23) { fpdR ^= fpdR << 13; fpdR ^= fpdR >> 17; fpdR ^= fpdR << 5; inputSampleR = fpdR * 1.18e-17; }

            // Chase delay taps smoothly
            if (std::abs (offsetA - targetA) > 1000.0) offsetA = targetA;
            offsetA = ((offsetA * 999.0) + targetA) / 1000.0;
            double fractionA = offsetA - std::floor (offsetA);
            double minusA = 1.0 - fractionA;

            if (std::abs (offsetB - targetB) > 1000.0) offsetB = targetB;
            offsetB = ((offsetB * 999.0) + targetB) / 1000.0;
            double fractionB = offsetB - std::floor (offsetB);
            double minusB = 1.0 - fractionB;

            // Gain staging into saturation
            if (gain > 0.0) { inputSampleL /= gain; inputSampleR /= gain; }

            // Clip to saturation range
            if (inputSampleL >  1.2533141373155) inputSampleL =  1.2533141373155;
            if (inputSampleL < -1.2533141373155) inputSampleL = -1.2533141373155;
            if (inputSampleR >  1.2533141373155) inputSampleR =  1.2533141373155;
            if (inputSampleR < -1.2533141373155) inputSampleR = -1.2533141373155;

            // Spiral saturation encode (Console-style)
            double absL = std::abs (inputSampleL);
            inputSampleL = (absL == 0.0) ? 0.0 : std::sin (inputSampleL * absL) / absL;
            double absR = std::abs (inputSampleR);
            inputSampleR = (absR == 0.0) ? 0.0 : std::sin (inputSampleR * absR) / absR;

            // Circular delay buffer
            if (gcount < 1 || gcount > 4800) gcount = 4800;
            int count = gcount;
            double totalL = 0.0;
            double totalR = 0.0;

            pL[count + 4800] = pL[count] = inputSampleL;
            pR[count + 4800] = pR[count] = inputSampleR;

            // Delay tap A
            if (intensityA != 0.0)
            {
                count = (int)(gcount + std::floor (offsetA));
                double tempL = (pL[count] * minusA);
                tempL += pL[count + 1];
                tempL += (pL[count + 2] * fractionA);
                tempL -= (((pL[count] - pL[count + 1]) - (pL[count + 1] - pL[count + 2])) / 50.0);
                totalL += (tempL * intensityA);

                double tempR = (pR[count] * minusA);
                tempR += pR[count + 1];
                tempR += (pR[count + 2] * fractionA);
                tempR -= (((pR[count] - pR[count + 1]) - (pR[count + 1] - pR[count + 2])) / 50.0);
                totalR += (tempR * intensityA);
            }

            // Delay tap B
            if (intensityB != 0.0)
            {
                count = (int)(gcount + std::floor (offsetB));
                double tempL = (pL[count] * minusB);
                tempL += pL[count + 1];
                tempL += (pL[count + 2] * fractionB);
                tempL -= (((pL[count] - pL[count + 1]) - (pL[count + 1] - pL[count + 2])) / 50.0);
                totalL += (tempL * intensityB);

                double tempR = (pR[count] * minusB);
                tempR += pR[count + 1];
                tempR += (pR[count + 2] * fractionB);
                tempR -= (((pR[count] - pR[count + 1]) - (pR[count + 1] - pR[count + 2])) / 50.0);
                totalR += (tempR * intensityB);
            }

            gcount--;

            // Add delay taps to dry signal
            inputSampleL += totalL;
            inputSampleR += totalR;

            // Clip to prevent NaN/runaway
            if (inputSampleL >  1.0) inputSampleL =  1.0;
            if (inputSampleL < -1.0) inputSampleL = -1.0;
            if (inputSampleR >  1.0) inputSampleR =  1.0;
            if (inputSampleR < -1.0) inputSampleR = -1.0;

            // asin() decode (inverse of spiral encode)
            inputSampleL = std::asin (inputSampleL);
            inputSampleR = std::asin (inputSampleR);

            // Restore gain
            inputSampleL *= gain;
            inputSampleR *= gain;

            // Output level
            if (outputLevel < 1.0)
            {
                inputSampleL *= outputLevel;
                inputSampleR *= outputLevel;
            }

            // Write output
            dataL[i] = (float)inputSampleL;
            if (dataR)
                dataR[i] = (float)inputSampleR;
        }
    }

    void   setParams (const Params& p) { params = p; }
    Params getParams() const           { return params; }
    void   setBypassed (bool b)        { bypassed = b; }
    bool   isBypassed() const          { return bypassed; }

private:
    Params params;
    bool bypassed = false;

    // Delay buffer (double-buffered ring, 4800+4800+margin)
    double pL[10000] = {};
    double pR[10000] = {};
    int gcount = 0;
    double offsetA = 9001.0;
    double offsetB = 9001.0;

    // PRNG for denormal prevention
    uint32_t fpdL = 1557111, fpdR = 7891233;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DoublerProcessor)
};
