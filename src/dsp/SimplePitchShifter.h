#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include <juce_core/juce_core.h>

class SimplePitchShifter
{
public:
    // FIX: Initialize buffer in constructor to prevent garbage noise (Right Meter 100% bug)
    SimplePitchShifter()
    {
        buffer.resize(8192, 0.0f);
    }

    void prepare(double sampleRate, int maxBlockSize)
    {
        // 8192 samples is ~180ms at 44.1k, plenty of room for the window
        bufferSize = 8192;
        if (buffer.size() != bufferSize)
            buffer.resize(bufferSize, 0.0f);
        
        // Window size determines grain length. 
        // 4096 (~90ms) is good for polyphonic material.
        windowSize = 4096;
        writePos = 0;
        phasor = 0.0f;
        currentRatio = 1.0f;
        targetRatio = 1.0f;
    }

    void reset()
    {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        writePos = 0;
        phasor = 0.0f;
    }

    void setPitchSemitones(float semitones)
    {
        // Calculate ratio: 2^(semitones/12)
        targetRatio = std::pow(2.0f, semitones / 12.0f);
    }

    // Harmonizer compatibility alias
    void setTransposeSemitones(float semitones)
    {
        setPitchSemitones(semitones);
    }

    void processSample(float input, float& output)
    {
        // Safety check to prevent crash if prepare wasn't called
        if (buffer.empty()) {
            output = input;
            return;
        }

        // 1. Parameter Smoothing
        currentRatio = 0.999f * currentRatio + 0.001f * targetRatio;

        // 2. Input
        buffer[writePos] = input;

        // 3. Update Phasor
        double step = (1.0 - (double)currentRatio) / (double)windowSize;
        phasor += (float)step;

        if (phasor >= 1.0f) phasor -= 1.0f;
        if (phasor < 0.0f)  phasor += 1.0f;

        // 4. Calculate Delay Taps
        float delayA = phasor * (windowSize - 1);
        float delayB = std::fmod(phasor + 0.5f, 1.0f) * (windowSize - 1);

        // 5. Read
        float sampleA = readBuffer(writePos - delayA);
        float sampleB = readBuffer(writePos - delayB);

        // 6. Windowing
        float gainA = 1.0f - 2.0f * std::abs(phasor - 0.5f);
        float gainB = 1.0f - 2.0f * std::abs(std::fmod(phasor + 0.5f, 1.0f) - 0.5f);

        // 7. Output
        output = (sampleA * gainA) + (sampleB * gainB);

        // 8. Advance
        writePos = (writePos + 1) % bufferSize;
    }

    void processBlock(float* data, int numSamples)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            float out;
            processSample(data[i], out);
            data[i] = out;
        }
    }

private:
    float readBuffer(float position)
    {
        while (position < 0.0f) position += bufferSize;
        while (position >= bufferSize) position -= bufferSize;

        int idxA = (int)position;
        int idxB = (idxA + 1) % bufferSize;
        float frac = position - idxA;

        // Safety mask
        idxA = idxA & (bufferSize - 1);
        idxB = idxB & (bufferSize - 1);

        return buffer[idxA] * (1.0f - frac) + buffer[idxB] * frac;
    }

    std::vector<float> buffer;
    int bufferSize = 8192;
    int windowSize = 4096;
    int writePos = 0;
    float phasor = 0.0f;
    float currentRatio = 1.0f;
    float targetRatio = 1.0f;
};