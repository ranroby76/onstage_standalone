// ==============================================================================
//  MasterProcessor.h
//  OnStage — Real-Time Mastering Node
//
//  Based on Airwindows Mastering2 by Chris Johnson (MIT License).
//  Adapted for OnStage's ProcessorBase/Node architecture.
//
//  Signal flow:
//    1. Input Drive
//    2. M/S encode -> Elliptical EQ (Sidepass) on side -> M/S decode
//    3. Air3 band split: treble vs mid+bass
//    4. KalmanM split: mid vs bass (crossover from Skronk)
//    5. KalmanS split: bass vs sub (crossover from Skronk)
//    6. Zoom waveshaping on treble/mid/bass independently
//    7. Sub gain + recombine all 4 bands
//    8. Output Drive (inverse compensation)
//    9. Zero-latency sin() soft clip (replaces ClipOnly2 for real-time use)
//   10. Sinew adaptive slew limiter (Glue control)
//
//  Changes from original Mastering2:
//    - ClipOnly2 (1-4 sample latency) -> memoryless sin() soft clip = zero latency
//    - Dither stage removed (OnStage uses floating-point internal bus)
//    - long double -> double (MSVC compatibility)
//    - VST2 API -> JUCE ProcessSpec/AudioBuffer interface
//
//  Parameters (all 0-1 range):
//    Sidepass (0), Glue (0), Scope (0.5), Skronk (0.5),
//    Girth (0.5), Drive (0.5)
//
//  Copyright (c) airwindows, MIT License. OnStage integration by Rob.
// ==============================================================================

#pragma once
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <algorithm>

class MasterProcessor
{
public:
    // ==========================================================================
    struct Params
    {
        float sidepass = 0.0f;   // A: 0-1 elliptical EQ on side channel
        float glue     = 0.0f;   // B: 0-1 Sinew slew limiter (treble softening)
        float scope    = 0.5f;   // C: 0-1 treble zoom (0.5 = center/no change)
        float skronk   = 0.5f;   // D: 0-1 mid zoom + crossover control
        float girth    = 0.5f;   // E: 0-1 bass zoom + sub gain
        float drive    = 0.5f;   // F: 0-1 input/output drive (0.5 = unity)

        bool operator== (const Params& o) const
        {
            return sidepass == o.sidepass && glue == o.glue && scope == o.scope
                && skronk == o.skronk && girth == o.girth && drive == o.drive;
        }
        bool operator!= (const Params& o) const { return !(*this == o); }
    };

    MasterProcessor() = default;

    // ==========================================================================
    //  Prepare / Reset
    // ==========================================================================
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        reset();
        fpdL = 1557111;
        fpdR = 7891233;
    }

    void reset()
    {
        iirA = 0.0; iirB = 0.0; iirC = 0.0;
        fpFlip = true;
        for (int x = 0; x < air_total; x++) air[x] = 0.0;
        for (int x = 0; x < kal_total; x++) { kalM[x] = 0.0; kalS[x] = 0.0; }
        lastSinewL = 0.0;
        lastSinewR = 0.0;
    }

    // ==========================================================================
    //  Process
    // ==========================================================================
    void process (juce::AudioBuffer<float>& buffer)
    {
        if (bypassed) return;

        const int N  = buffer.getNumSamples();
        const int ch = buffer.getNumChannels();
        if (ch < 1) return;

        float* dataL = buffer.getWritePointer (0);
        float* dataR = ch > 1 ? buffer.getWritePointer (1) : nullptr;

        // Pre-compute parameter-derived values
        double overallscale = 1.0;
        overallscale /= 44100.0;
        overallscale *= sampleRate;

        double iirSide = std::pow ((double)params.sidepass, 3.0) * (0.1 / overallscale);

        double threshSinew = (0.25 + ((1.0 - (double)params.glue) * 0.333)) / overallscale;
        double depthSinew = 1.0 - std::pow (1.0 - (double)params.glue, 2.0);

        double trebleZoom = (double)params.scope - 0.5;
        double trebleGain = (trebleZoom * std::abs (trebleZoom)) + 1.0;
        if (trebleGain > 1.0) trebleGain = std::pow (trebleGain, 3.0 + std::sqrt (overallscale));

        double midZoom = (double)params.skronk - 0.5;
        double midGain = (midZoom * std::abs (midZoom)) + 1.0;
        double kalMid = 0.35 - ((double)params.skronk * 0.25);
        double kalSub = 0.45 + ((double)params.skronk * 0.25);

        double bassZoom = ((double)params.girth * 0.5) - 0.25;
        double bassGain = (-bassZoom * std::abs (bassZoom)) + 1.0;
        double subGain = (((double)params.girth * 0.25) - 0.125) + 1.0;
        if (subGain < 1.0) subGain = 1.0;

        double driveIn = ((double)params.drive - 0.5) + 1.0;
        double driveOut = (-((double)params.drive - 0.5) * std::abs ((double)params.drive - 0.5)) + 1.0;

        for (int i = 0; i < N; ++i)
        {
            double inputSampleL = (double)dataL[i];
            double inputSampleR = dataR ? (double)dataR[i] : inputSampleL;

            // Denormal prevention
            if (std::abs (inputSampleL) < 1.18e-23) { fpdL ^= fpdL << 13; fpdL ^= fpdL >> 17; fpdL ^= fpdL << 5; inputSampleL = fpdL * 1.18e-17; }
            if (std::abs (inputSampleR) < 1.18e-23) { fpdR ^= fpdR << 13; fpdR ^= fpdR >> 17; fpdR ^= fpdR << 5; inputSampleR = fpdR * 1.18e-17; }

            inputSampleL *= driveIn;
            inputSampleR *= driveIn;

            // ==============================================================
            //  Elliptical EQ (Sidepass) - M/S processing
            // ==============================================================
            double mid  = inputSampleL + inputSampleR;
            double side = inputSampleL - inputSampleR;
            double temp = side;

            if (fpFlip)
            {
                iirA = (iirA * (1.0 - iirSide)) + (temp * iirSide);
                temp = iirA;
            }
            else
            {
                iirB = (iirB * (1.0 - iirSide)) + (temp * iirSide);
                temp = iirB;
            }
            iirC = (iirC * (1.0 - iirSide)) + (temp * iirSide);
            temp = iirC;

            side -= std::sin (temp);
            fpFlip = !fpFlip;

            inputSampleL = (mid + side) / 2.0;
            inputSampleR = (mid - side) / 2.0;

            double drySampleL = inputSampleL;
            double drySampleR = inputSampleR;

            // ==============================================================
            //  Air3 - treble extraction (predictive differencing)
            // ==============================================================
            // Left
            air[pvSL4] = air[pvAL4] - air[pvAL3]; air[pvSL3] = air[pvAL3] - air[pvAL2];
            air[pvSL2] = air[pvAL2] - air[pvAL1]; air[pvSL1] = air[pvAL1] - inputSampleL;
            air[accSL3] = air[pvSL4] - air[pvSL3]; air[accSL2] = air[pvSL3] - air[pvSL2];
            air[accSL1] = air[pvSL2] - air[pvSL1];
            air[acc2SL2] = air[accSL3] - air[accSL2]; air[acc2SL1] = air[accSL2] - air[accSL1];
            air[outAL] = -(air[pvAL1] + air[pvSL3] + air[acc2SL2] - ((air[acc2SL2] + air[acc2SL1]) * 0.5));
            air[gainAL] *= 0.5; air[gainAL] += std::abs (drySampleL - air[outAL]) * 0.5;
            if (air[gainAL] > 0.3 * std::sqrt (overallscale)) air[gainAL] = 0.3 * std::sqrt (overallscale);
            air[pvAL4] = air[pvAL3]; air[pvAL3] = air[pvAL2];
            air[pvAL2] = air[pvAL1]; air[pvAL1] = (air[gainAL] * air[outAL]) + drySampleL;
            double midL = drySampleL - ((air[outAL] * 0.5) + (drySampleL * (0.457 - (0.017 * overallscale))));
            temp = (midL + air[gndavgL]) * 0.5; air[gndavgL] = midL; midL = temp;
            double trebleL = drySampleL - midL;

            // Right
            air[pvSR4] = air[pvAR4] - air[pvAR3]; air[pvSR3] = air[pvAR3] - air[pvAR2];
            air[pvSR2] = air[pvAR2] - air[pvAR1]; air[pvSR1] = air[pvAR1] - inputSampleR;
            air[accSR3] = air[pvSR4] - air[pvSR3]; air[accSR2] = air[pvSR3] - air[pvSR2];
            air[accSR1] = air[pvSR2] - air[pvSR1];
            air[acc2SR2] = air[accSR3] - air[accSR2]; air[acc2SR1] = air[accSR2] - air[accSR1];
            air[outAR] = -(air[pvAR1] + air[pvSR3] + air[acc2SR2] - ((air[acc2SR2] + air[acc2SR1]) * 0.5));
            air[gainAR] *= 0.5; air[gainAR] += std::abs (drySampleR - air[outAR]) * 0.5;
            if (air[gainAR] > 0.3 * std::sqrt (overallscale)) air[gainAR] = 0.3 * std::sqrt (overallscale);
            air[pvAR4] = air[pvAR3]; air[pvAR3] = air[pvAR2];
            air[pvAR2] = air[pvAR1]; air[pvAR1] = (air[gainAR] * air[outAR]) + drySampleR;
            double midR = drySampleR - ((air[outAR] * 0.5) + (drySampleR * (0.457 - (0.017 * overallscale))));
            temp = (midR + air[gndavgR]) * 0.5; air[gndavgR] = midR; midR = temp;
            double trebleR = drySampleR - midR;

            // ==============================================================
            //  KalmanM - mid/bass split
            // ==============================================================
            // Left
            temp = midL;
            kalM[prevSlewL3] += kalM[prevSampL3] - kalM[prevSampL2]; kalM[prevSlewL3] *= 0.5;
            kalM[prevSlewL2] += kalM[prevSampL2] - kalM[prevSampL1]; kalM[prevSlewL2] *= 0.5;
            kalM[prevSlewL1] += kalM[prevSampL1] - midL;             kalM[prevSlewL1] *= 0.5;
            kalM[accSlewL2] += kalM[prevSlewL3] - kalM[prevSlewL2]; kalM[accSlewL2] *= 0.5;
            kalM[accSlewL1] += kalM[prevSlewL2] - kalM[prevSlewL1]; kalM[accSlewL1] *= 0.5;
            kalM[accSlewL3] += (kalM[accSlewL2] - kalM[accSlewL1]); kalM[accSlewL3] *= 0.5;
            kalM[kalOutL] += kalM[prevSampL1] + kalM[prevSlewL2] + kalM[accSlewL3]; kalM[kalOutL] *= 0.5;
            kalM[kalGainL] += std::abs (temp - kalM[kalOutL]) * kalMid * 8.0; kalM[kalGainL] *= 0.5;
            if (kalM[kalGainL] > kalMid * 0.5) kalM[kalGainL] = kalMid * 0.5;
            kalM[kalOutL] += (temp * (1.0 - (0.68 + (kalMid * 0.157))));
            kalM[prevSampL3] = kalM[prevSampL2]; kalM[prevSampL2] = kalM[prevSampL1];
            kalM[prevSampL1] = (kalM[kalGainL] * kalM[kalOutL]) + ((1.0 - kalM[kalGainL]) * temp);
            double bassL = (kalM[kalOutL] + kalM[kalAvgL]) * 0.5;
            kalM[kalAvgL] = kalM[kalOutL];
            midL -= bassL;

            // Right
            temp = midR;
            kalM[prevSlewR3] += kalM[prevSampR3] - kalM[prevSampR2]; kalM[prevSlewR3] *= 0.5;
            kalM[prevSlewR2] += kalM[prevSampR2] - kalM[prevSampR1]; kalM[prevSlewR2] *= 0.5;
            kalM[prevSlewR1] += kalM[prevSampR1] - midR;             kalM[prevSlewR1] *= 0.5;
            kalM[accSlewR2] += kalM[prevSlewR3] - kalM[prevSlewR2]; kalM[accSlewR2] *= 0.5;
            kalM[accSlewR1] += kalM[prevSlewR2] - kalM[prevSlewR1]; kalM[accSlewR1] *= 0.5;
            kalM[accSlewR3] += (kalM[accSlewR2] - kalM[accSlewR1]); kalM[accSlewR3] *= 0.5;
            kalM[kalOutR] += kalM[prevSampR1] + kalM[prevSlewR2] + kalM[accSlewR3]; kalM[kalOutR] *= 0.5;
            kalM[kalGainR] += std::abs (temp - kalM[kalOutR]) * kalMid * 8.0; kalM[kalGainR] *= 0.5;
            if (kalM[kalGainR] > kalMid * 0.5) kalM[kalGainR] = kalMid * 0.5;
            kalM[kalOutR] += (temp * (1.0 - (0.68 + (kalMid * 0.157))));
            kalM[prevSampR3] = kalM[prevSampR2]; kalM[prevSampR2] = kalM[prevSampR1];
            kalM[prevSampR1] = (kalM[kalGainR] * kalM[kalOutR]) + ((1.0 - kalM[kalGainR]) * temp);
            double bassR = (kalM[kalOutR] + kalM[kalAvgR]) * 0.5;
            kalM[kalAvgR] = kalM[kalOutR];
            midR -= bassR;

            // ==============================================================
            //  KalmanS - bass/sub split
            // ==============================================================
            // Left
            temp = bassL;
            kalS[prevSlewL3] += kalS[prevSampL3] - kalS[prevSampL2]; kalS[prevSlewL3] *= 0.5;
            kalS[prevSlewL2] += kalS[prevSampL2] - kalS[prevSampL1]; kalS[prevSlewL2] *= 0.5;
            kalS[prevSlewL1] += kalS[prevSampL1] - bassL;             kalS[prevSlewL1] *= 0.5;
            kalS[accSlewL2] += kalS[prevSlewL3] - kalS[prevSlewL2]; kalS[accSlewL2] *= 0.5;
            kalS[accSlewL1] += kalS[prevSlewL2] - kalS[prevSlewL1]; kalS[accSlewL1] *= 0.5;
            kalS[accSlewL3] += (kalS[accSlewL2] - kalS[accSlewL1]); kalS[accSlewL3] *= 0.5;
            kalS[kalOutL] += kalS[prevSampL1] + kalS[prevSlewL2] + kalS[accSlewL3]; kalS[kalOutL] *= 0.5;
            kalS[kalGainL] += std::abs (temp - kalS[kalOutL]) * kalSub * 8.0; kalS[kalGainL] *= 0.5;
            if (kalS[kalGainL] > kalSub * 0.5) kalS[kalGainL] = kalSub * 0.5;
            kalS[kalOutL] += (temp * (1.0 - (0.68 + (kalSub * 0.157))));
            kalS[prevSampL3] = kalS[prevSampL2]; kalS[prevSampL2] = kalS[prevSampL1];
            kalS[prevSampL1] = (kalS[kalGainL] * kalS[kalOutL]) + ((1.0 - kalS[kalGainL]) * temp);
            double subL = (kalS[kalOutL] + kalS[kalAvgL]) * 0.5;
            kalS[kalAvgL] = kalS[kalOutL];
            bassL -= subL;

            // Right
            temp = bassR;
            kalS[prevSlewR3] += kalS[prevSampR3] - kalS[prevSampR2]; kalS[prevSlewR3] *= 0.5;
            kalS[prevSlewR2] += kalS[prevSampR2] - kalS[prevSampR1]; kalS[prevSlewR2] *= 0.5;
            kalS[prevSlewR1] += kalS[prevSampR1] - bassR;             kalS[prevSlewR1] *= 0.5;
            kalS[accSlewR2] += kalS[prevSlewR3] - kalS[prevSlewR2]; kalS[accSlewR2] *= 0.5;
            kalS[accSlewR1] += kalS[prevSlewR2] - kalS[prevSlewR1]; kalS[accSlewR1] *= 0.5;
            kalS[accSlewR3] += (kalS[accSlewR2] - kalS[accSlewR1]); kalS[accSlewR3] *= 0.5;
            kalS[kalOutR] += kalS[prevSampR1] + kalS[prevSlewR2] + kalS[accSlewR3]; kalS[kalOutR] *= 0.5;
            kalS[kalGainR] += std::abs (temp - kalS[kalOutR]) * kalSub * 8.0; kalS[kalGainR] *= 0.5;
            if (kalS[kalGainR] > kalSub * 0.5) kalS[kalGainR] = kalSub * 0.5;
            kalS[kalOutR] += (temp * (1.0 - (0.68 + (kalSub * 0.157))));
            kalS[prevSampR3] = kalS[prevSampR2]; kalS[prevSampR2] = kalS[prevSampR1];
            kalS[prevSampR1] = (kalS[kalGainR] * kalS[kalOutR]) + ((1.0 - kalS[kalGainR]) * temp);
            double subR = (kalS[kalOutR] + kalS[kalAvgR]) * 0.5;
            kalS[kalAvgR] = kalS[kalOutR];
            bassR -= subR;

            // ==============================================================
            //  Recombine: sub + zoomed bass + zoomed mid + zoomed treble
            // ==============================================================
            inputSampleL = subL * subGain;
            inputSampleR = subR * subGain;

            applyZoom (bassL, bassZoom);
            applyZoom (bassR, bassZoom);
            inputSampleL += bassL * bassGain;
            inputSampleR += bassR * bassGain;

            applyZoom (midL, midZoom);
            applyZoom (midR, midZoom);
            inputSampleL += midL * midGain;
            inputSampleR += midR * midGain;

            applyZoom (trebleL, trebleZoom);
            applyZoom (trebleR, trebleZoom);
            inputSampleL += trebleL * trebleGain;
            inputSampleR += trebleR * trebleGain;

            // Output drive compensation
            inputSampleL *= driveOut;
            inputSampleR *= driveOut;

            // ==============================================================
            //  Zero-latency soft clip (replaces ClipOnly2)
            //  sin()-based, same curve used in Console
            // ==============================================================
            if (inputSampleL > 1.57079633)       inputSampleL = 1.0;
            else if (inputSampleL < -1.57079633) inputSampleL = -1.0;
            else                                 inputSampleL = std::sin (inputSampleL);

            if (inputSampleR > 1.57079633)       inputSampleR = 1.0;
            else if (inputSampleR < -1.57079633) inputSampleR = -1.0;
            else                                 inputSampleR = std::sin (inputSampleR);

            // ==============================================================
            //  Sinew - adaptive slew limiter (Glue control)
            // ==============================================================
            if (depthSinew > 0.0001)
            {
                temp = inputSampleL;
                double sinew = threshSinew * std::cos (lastSinewL * lastSinewL);
                if (inputSampleL - lastSinewL > sinew)    temp = lastSinewL + sinew;
                if (-(inputSampleL - lastSinewL) > sinew) temp = lastSinewL - sinew;
                lastSinewL = temp;
                inputSampleL = (inputSampleL * (1.0 - depthSinew)) + (lastSinewL * depthSinew);

                temp = inputSampleR;
                sinew = threshSinew * std::cos (lastSinewR * lastSinewR);
                if (inputSampleR - lastSinewR > sinew)    temp = lastSinewR + sinew;
                if (-(inputSampleR - lastSinewR) > sinew) temp = lastSinewR - sinew;
                lastSinewR = temp;
                inputSampleR = (inputSampleR * (1.0 - depthSinew)) + (lastSinewR * depthSinew);
            }

            // Write output
            dataL[i] = (float)inputSampleL;
            if (dataR)
                dataR[i] = (float)inputSampleR;
        }
    }

    // ==========================================================================
    //  Parameter access
    // ==========================================================================
    void   setParams (const Params& p) { params = p; }
    Params getParams() const           { return params; }
    void   setBypassed (bool b)        { bypassed = b; }
    bool   isBypassed() const          { return bypassed; }

private:
    // ==========================================================================
    //  Zoom waveshaper (bidirectional)
    // ==========================================================================
    static void applyZoom (double& sample, double zoom)
    {
        if (zoom > 0.0)
        {
            double closer = sample * 1.57079633;
            closer = juce::jlimit (-1.57079633, 1.57079633, closer);
            sample = (sample * (1.0 - zoom)) + (std::sin (closer) * zoom);
        }
        else if (zoom < 0.0)
        {
            double farther = std::abs (sample) * 1.57079633;
            if (farther > 1.57079633) farther = 1.0;
            else farther = 1.0 - std::cos (farther);
            if (sample > 0.0)
                sample = (sample * (1.0 + zoom)) - (farther * zoom * 1.57079633);
            else
                sample = (sample * (1.0 + zoom)) + (farther * zoom * 1.57079633);
        }
    }

    // ==========================================================================
    //  Air3 state enums
    // ==========================================================================
    enum
    {
        pvAL1, pvSL1, accSL1, acc2SL1,
        pvAL2, pvSL2, accSL2, acc2SL2,
        pvAL3, pvSL3, accSL3,
        pvAL4, pvSL4, gndavgL, outAL, gainAL,
        pvAR1, pvSR1, accSR1, acc2SR1,
        pvAR2, pvSR2, accSR2, acc2SR2,
        pvAR3, pvSR3, accSR3,
        pvAR4, pvSR4, gndavgR, outAR, gainAR,
        air_total
    };
    double air[air_total] = {};

    // ==========================================================================
    //  Kalman filter state enums
    // ==========================================================================
    enum
    {
        prevSampL1, prevSlewL1, accSlewL1,
        prevSampL2, prevSlewL2, accSlewL2,
        prevSampL3, prevSlewL3, accSlewL3,
        kalGainL, kalOutL, kalAvgL,
        prevSampR1, prevSlewR1, accSlewR1,
        prevSampR2, prevSlewR2, accSlewR2,
        prevSampR3, prevSlewR3, accSlewR3,
        kalGainR, kalOutR, kalAvgR,
        kal_total
    };
    double kalM[kal_total] = {};
    double kalS[kal_total] = {};

    // ==========================================================================
    //  State
    // ==========================================================================
    Params params;
    bool bypassed = false;
    double sampleRate = 44100.0;

    double iirA = 0.0, iirB = 0.0, iirC = 0.0;
    bool fpFlip = true;

    double lastSinewL = 0.0, lastSinewR = 0.0;

    uint32_t fpdL = 1557111, fpdR = 7891233;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MasterProcessor)
};
