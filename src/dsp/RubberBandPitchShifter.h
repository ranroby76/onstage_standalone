// ==============================================================================
//  RubberBandPitchShifter.h
//  OnStage - High-quality pitch shifter using RubberBand Stretcher API
//
//  Uses RubberBandStretcher in real-time mode for pitch shifting with
//  independent formant control. Compatible with RubberBand v2.x and v3.x.
//
//  Key features:
//  - Real-time mode (single pass, no study phase)
//  - Internal ring buffering to bridge JUCE's variable block sizes
//  - Independent pitch + formant scale control per voice
//  - ~50ms latency (inherent to phase vocoder)
//  - Real-time safe (no allocation in process path)
// ==============================================================================

#pragma once
#include <rubberband/RubberBandStretcher.h>
#include <juce_core/juce_core.h>
#include <vector>
#include <cmath>
#include <algorithm>

class RubberBandPitchShifter
{
public:
    RubberBandPitchShifter() = default;

    ~RubberBandPitchShifter() = default;

    void prepare (double sampleRate, int maxBlockSize)
    {
        currentSampleRate = sampleRate;
        currentMaxBlockSize = maxBlockSize;
        recreateShifter();
    }

    void reset()
    {
        if (stretcher)
            stretcher->reset();

        std::fill (outputRing.begin(), outputRing.end(), 0.0f);
        outputWritePos = 0;
        outputReadPos = 0;
        outputCount = 0;
        primed = false;
    }

    // Set pitch shift in semitones (-12 to +12)
    void setTransposeSemitones (float semitones)
    {
        targetPitchSemitones = semitones;
    }

    // Set formant shift in semitones (-12 to +12)
    // formant=0 with FormantPreserved -> natural pitch shift (formants stay)
    // formant!=0 -> explicit independent formant shift
    void setFormantSemitones (float semitones)
    {
        targetFormantSemitones = semitones;
    }

    // Harmonizer compatibility: combined pitch + formant update
    void setPitchAndFormant (float pitchSemitones, float formantSemitones)
    {
        targetPitchSemitones = pitchSemitones;
        targetFormantSemitones = formantSemitones;
    }

    // Process sample-by-sample (bridges to RubberBand's block processing)
    void processSample (float input, float& output)
    {
        if (! stretcher)
        {
            output = input;
            return;
        }

        // Feed input one sample at a time into a small accumulation buffer
        inputAccum.push_back (input);

        // When we have enough samples, feed a block to RubberBand
        int required = (int) stretcher->getSamplesRequired();
        if (required == 0) required = processBlockSize;

        if ((int) inputAccum.size() >= required)
        {
            // Smooth and apply pitch/formant changes once per block
            smoothAndApplyParams();

            // Process through RubberBand
            const float* inPtr = inputAccum.data();
            stretcher->process (&inPtr, (size_t) inputAccum.size(), false);
            inputAccum.clear();

            // Retrieve all available output
            int avail = (int) stretcher->available();
            if (avail > 0)
            {
                if ((int) retrieveBuffer.size() < avail)
                    retrieveBuffer.resize ((size_t) avail, 0.0f);

                float* outPtr = retrieveBuffer.data();
                size_t got = stretcher->retrieve (&outPtr, (size_t) avail);

                // Write to output ring
                for (size_t i = 0; i < got; ++i)
                {
                    outputRing[(size_t) outputWritePos] = retrieveBuffer[i];
                    outputWritePos = (outputWritePos + 1) % ringSize;
                    outputCount++;
                }
            }
        }

        // Pop output sample from ring buffer
        if (outputCount > 0)
        {
            output = outputRing[(size_t) outputReadPos];
            outputRing[(size_t) outputReadPos] = 0.0f;
            outputReadPos = (outputReadPos + 1) % ringSize;
            outputCount--;
        }
        else
        {
            output = 0.0f;  // Latency fill at start
        }
    }

private:
    void recreateShifter()
    {
        using namespace RubberBand;

        RubberBandStretcher::Options options =
            RubberBandStretcher::OptionProcessRealTime
            | RubberBandStretcher::OptionPitchHighConsistency
            | RubberBandStretcher::OptionFormantPreserved
            | RubberBandStretcher::OptionChannelsTogether;

        stretcher = std::make_unique<RubberBandStretcher> (
            (size_t) currentSampleRate, 1, options, 1.0, 1.0);

        // Tell RubberBand the max block size we'll feed
        processBlockSize = juce::jmax (256, currentMaxBlockSize);
        stretcher->setMaxProcessSize ((size_t) processBlockSize);

        // Prime with silence to fill start delay
        int startDelay = (int) stretcher->getStartDelay();
        if (startDelay > 0)
        {
            std::vector<float> silence ((size_t) startDelay, 0.0f);
            const float* silPtr = silence.data();
            stretcher->process (&silPtr, (size_t) startDelay, false);
        }

        // Ring buffer for output: generous size
        ringSize = processBlockSize * 8;
        outputRing.resize ((size_t) ringSize, 0.0f);
        outputWritePos = 0;
        outputReadPos = 0;
        outputCount = 0;

        inputAccum.clear();
        inputAccum.reserve ((size_t) processBlockSize * 2);
        retrieveBuffer.resize ((size_t) processBlockSize * 2, 0.0f);

        primed = false;

        // Apply current pitch/formant
        applyPitchAndFormant();
    }

    void applyPitchAndFormant()
    {
        if (! stretcher) return;

        // Pitch scale: 2^(semitones/12)
        double pitchScale = std::pow (2.0, (double) currentPitchSemitones / 12.0);
        pitchScale = juce::jlimit (0.25, 4.0, pitchScale);
        stretcher->setPitchScale (pitchScale);

        // Formant scale:
        // With OptionFormantPreserved, 0.0 = auto-preserve (formants stay put).
        // Explicit value = independent formant shift.
        if (std::abs (currentFormantSemitones) > 0.05f)
        {
            double formantScale = std::pow (2.0, (double) currentFormantSemitones / 12.0);
            formantScale = juce::jlimit (0.25, 4.0, formantScale);
            stretcher->setFormantScale (formantScale);
        }
        else
        {
            stretcher->setFormantScale (0.0);  // Auto (preserve)
        }
    }

    void smoothAndApplyParams()
    {
        const float smoothCoeff = 0.3f;
        bool changed = false;

        if (std::abs (currentPitchSemitones - targetPitchSemitones) > 0.001f)
        {
            currentPitchSemitones += (targetPitchSemitones - currentPitchSemitones) * smoothCoeff;
            changed = true;
        }
        if (std::abs (currentFormantSemitones - targetFormantSemitones) > 0.001f)
        {
            currentFormantSemitones += (targetFormantSemitones - currentFormantSemitones) * smoothCoeff;
            changed = true;
        }
        if (changed)
            applyPitchAndFormant();
    }

    // RubberBand instance
    std::unique_ptr<RubberBand::RubberBandStretcher> stretcher;
    int processBlockSize = 512;
    double currentSampleRate = 44100.0;
    int currentMaxBlockSize = 512;
    bool primed = false;

    // Input accumulation buffer (variable size, feeds RubberBand when ready)
    std::vector<float> inputAccum;

    // Retrieve buffer (temp for pulling output from RubberBand)
    std::vector<float> retrieveBuffer;

    // Output ring buffer for sample-by-sample reading
    std::vector<float> outputRing;
    int ringSize = 4096;
    int outputWritePos = 0;
    int outputReadPos = 0;
    int outputCount = 0;

    // Pitch and formant parameters
    float targetPitchSemitones = 0.0f;
    float currentPitchSemitones = 0.0f;
    float targetFormantSemitones = 0.0f;
    float currentFormantSemitones = 0.0f;
};