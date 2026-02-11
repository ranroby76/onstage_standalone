// ==============================================================================
//  PitchProcessor.h
//  OnStage - Balanced vocal tuner (fast + stable)
//
//  Balanced approach:
//  - Quick note detection (4 frames to lock)
//  - Moderate hysteresis (35 cents, 3 frames to unlock)
//  - Light smoothing for responsiveness
// ==============================================================================

#pragma once
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <array>
#include <vector>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

class PitchProcessor
{
public:
    struct GuitarString
    {
        const char* name;
        float frequency;
        int midiNote;
    };

    struct Params
    {
        float sensitivity { 0.15f };       // YIN threshold
        float referencePitch { 440.0f };
        float gateThreshold { 0.006f };    // Slightly lower gate
    };

    struct PitchInfo
    {
        float frequency { 0.0f };
        float confidence { 0.0f };
        int midiNote { -1 };
        int noteIndex { 0 };
        int octave { 4 };
        float cents { 0.0f };
        bool isActive { false };
        int nearestGuitarString { -1 };
        float stringCents { 0.0f };
    };

    PitchProcessor() = default;

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        
        // Smaller buffer for faster response
        yinBufferSize = 2048;
        yinBuffer.resize(yinBufferSize, 0.0f);
        yinDiff.resize(yinBufferSize / 2, 0.0f);
        yinCMND.resize(yinBufferSize / 2, 0.0f);
        
        inputBuffer.resize(yinBufferSize * 2, 0.0f);
        inputWritePos = 0;
        analysisCounter = 0;
        
        // Smaller history for faster response
        freqHistory.resize(5, 0.0f);
        freqHistoryIdx = 0;
        
        isPrepared = true;
    }

    void reset()
    {
        std::fill(inputBuffer.begin(), inputBuffer.end(), 0.0f);
        std::fill(yinBuffer.begin(), yinBuffer.end(), 0.0f);
        std::fill(freqHistory.begin(), freqHistory.end(), 0.0f);
        
        inputWritePos = 0;
        analysisCounter = 0;
        freqHistoryIdx = 0;
        smoothedFreq = 0.0f;
        lockedNote = -1;
        lockCounter = 0;
        unlockCounter = 0;
    }

    void process(juce::AudioBuffer<float>& buffer)
    {
        if (!isPrepared || bypassed)
            return;

        const int numSamples = buffer.getNumSamples();
        const float* input = buffer.getReadPointer(0);
        
        for (int i = 0; i < numSamples; ++i)
        {
            inputBuffer[inputWritePos] = input[i];
            inputWritePos = (inputWritePos + 1) % (int)inputBuffer.size();
            analysisCounter++;
        }
        
        // More frequent analysis with smaller buffer
        int hopSize = yinBufferSize / 4;
        if (analysisCounter >= hopSize)
        {
            detectPitch();
            analysisCounter = 0;
        }
    }

    void setParams(const Params& p) { params = p; }
    Params getParams() const { return params; }
    void setBypassed(bool b) { bypassed = b; }
    bool isBypassed() const { return bypassed; }
    
    PitchInfo getCurrentPitch() const { return currentPitch; }

    static juce::String getNoteName(int idx)
    {
        static const char* names[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
        return names[((idx % 12) + 12) % 12];
    }

    static constexpr int NUM_GUITAR_STRINGS = 6;
    static const GuitarString guitarStrings[NUM_GUITAR_STRINGS];

private:
    void detectPitch()
    {
        int readPos = (inputWritePos - yinBufferSize + (int)inputBuffer.size()) 
                      % (int)inputBuffer.size();
        
        float rms = 0.0f;
        for (int i = 0; i < yinBufferSize; ++i)
        {
            int idx = (readPos + i) % (int)inputBuffer.size();
            yinBuffer[i] = inputBuffer[idx];
            rms += yinBuffer[i] * yinBuffer[i];
        }
        rms = std::sqrt(rms / yinBufferSize);
        
        if (rms < params.gateThreshold)
        {
            currentPitch.isActive = false;
            return;
        }
        
        // YIN Algorithm
        int halfSize = yinBufferSize / 2;
        
        for (int tau = 0; tau < halfSize; ++tau)
        {
            float sum = 0.0f;
            for (int i = 0; i < halfSize; ++i)
            {
                float delta = yinBuffer[i] - yinBuffer[i + tau];
                sum += delta * delta;
            }
            yinDiff[tau] = sum;
        }
        
        yinCMND[0] = 1.0f;
        float runningSum = 0.0f;
        for (int tau = 1; tau < halfSize; ++tau)
        {
            runningSum += yinDiff[tau];
            yinCMND[tau] = (runningSum > 1e-10f) ? (yinDiff[tau] * tau / runningSum) : 1.0f;
        }
        
        // Frequency range for voice: ~80Hz to ~1000Hz
        int minTau = juce::jmax(2, (int)(sampleRate / 1000.0));
        int maxTau = juce::jmin(halfSize - 1, (int)(sampleRate / 80.0));
        
        // Find ALL local minima below threshold, pick the best one
        struct Candidate {
            int tau;
            float value;
        };
        std::vector<Candidate> candidates;
        
        for (int tau = minTau; tau < maxTau; ++tau)
        {
            if (yinCMND[tau] < params.sensitivity)
            {
                // Find local minimum
                while (tau + 1 < maxTau && yinCMND[tau + 1] < yinCMND[tau])
                    ++tau;
                
                candidates.push_back({tau, yinCMND[tau]});
                
                // Skip past this minimum
                while (tau + 1 < maxTau && yinCMND[tau + 1] >= yinCMND[tau])
                    ++tau;
            }
        }
        
        if (candidates.empty())
        {
            currentPitch.isActive = false;
            return;
        }
        
        // Pick candidate with lowest CMNDF value (most confident)
        auto best = std::min_element(candidates.begin(), candidates.end(),
            [](const Candidate& a, const Candidate& b) { return a.value < b.value; });
        
        int bestTau = best->tau;
        float bestValue = best->value;
        
        // Parabolic interpolation
        float refinedTau = (float)bestTau;
        if (bestTau > 0 && bestTau < halfSize - 1)
        {
            float s0 = yinCMND[bestTau - 1];
            float s1 = yinCMND[bestTau];
            float s2 = yinCMND[bestTau + 1];
            
            float denom = 2.0f * s1 - s2 - s0;
            if (std::abs(denom) > 1e-10f)
            {
                float delta = (s2 - s0) / (2.0f * denom);
                refinedTau += juce::jlimit(-1.0f, 1.0f, delta);
            }
        }
        
        float detectedFreq = (float)sampleRate / refinedTau;
        
        if (detectedFreq < 80.0f || detectedFreq > 1000.0f)
        {
            currentPitch.isActive = false;
            return;
        }
        
        float confidence = 1.0f - bestValue;
        if (confidence < 0.5f)
        {
            currentPitch.isActive = false;
            return;
        }
        
        // =================================================================
        // LIGHT SMOOTHING - just median filter for octave errors
        // =================================================================
        
        freqHistory[freqHistoryIdx] = detectedFreq;
        freqHistoryIdx = (freqHistoryIdx + 1) % (int)freqHistory.size();
        
        // Median filter
        std::vector<float> sorted = freqHistory;
        std::sort(sorted.begin(), sorted.end());
        float medianFreq = sorted[sorted.size() / 2];
        
        // Octave error correction
        float ratio = detectedFreq / medianFreq;
        if (ratio > 1.9f && ratio < 2.1f)
            detectedFreq /= 2.0f;
        else if (ratio > 0.48f && ratio < 0.52f)
            detectedFreq *= 2.0f;
        
        // Light smoothing
        if (smoothedFreq > 0.0f)
            smoothedFreq = smoothedFreq * 0.6f + detectedFreq * 0.4f;
        else
            smoothedFreq = detectedFreq;
        
        // =================================================================
        // FAST NOTE LOCKING
        // =================================================================
        
        float exactMidi = 69.0f + 12.0f * std::log2(smoothedFreq / params.referencePitch);
        int detectedNote = (int)std::round(exactMidi);
        
        float centsFromLocked = 0.0f;
        if (lockedNote >= 0)
            centsFromLocked = (exactMidi - (float)lockedNote) * 100.0f;
        
        // Fast but stable parameters
        const float UNLOCK_THRESHOLD = 35.0f;  // 35 cents to start unlock
        const int FRAMES_TO_LOCK = 4;          // Quick lock
        const int FRAMES_TO_UNLOCK = 3;        // Quick unlock when clearly moved
        
        if (lockedNote < 0)
        {
            // No note locked - lock quickly
            if (detectedNote == pendingNote)
            {
                lockCounter++;
                if (lockCounter >= FRAMES_TO_LOCK)
                {
                    lockedNote = detectedNote;
                    lockCounter = 0;
                }
            }
            else
            {
                pendingNote = detectedNote;
                lockCounter = 1;
            }
        }
        else
        {
            // Have locked note
            if (std::abs(centsFromLocked) > UNLOCK_THRESHOLD)
            {
                unlockCounter++;
                
                if (unlockCounter >= FRAMES_TO_UNLOCK)
                {
                    // Start locking new note
                    if (detectedNote == pendingNote)
                    {
                        lockCounter++;
                        if (lockCounter >= FRAMES_TO_LOCK)
                        {
                            lockedNote = detectedNote;
                            lockCounter = 0;
                            unlockCounter = 0;
                        }
                    }
                    else
                    {
                        pendingNote = detectedNote;
                        lockCounter = 1;
                    }
                }
            }
            else
            {
                // Close to locked note - stay locked, reset counters
                unlockCounter = 0;
                lockCounter = 0;
                pendingNote = lockedNote;
            }
        }
        
        // =================================================================
        // UPDATE OUTPUT
        // =================================================================
        
        if (lockedNote >= 0)
        {
            float displayCents = (exactMidi - (float)lockedNote) * 100.0f;
            displayCents = juce::jlimit(-50.0f, 50.0f, displayCents);
            
            int noteIndex = ((lockedNote % 12) + 12) % 12;
            int octave = (lockedNote / 12) - 1;
            
            currentPitch.frequency = smoothedFreq;
            currentPitch.confidence = confidence;
            currentPitch.midiNote = lockedNote;
            currentPitch.noteIndex = noteIndex;
            currentPitch.octave = octave;
            currentPitch.cents = displayCents;
            currentPitch.isActive = true;
            
            findNearestGuitarString();
        }
    }

    void findNearestGuitarString()
    {
        int nearest = -1;
        float minCentsDiff = 1000.0f;
        
        for (int i = 0; i < NUM_GUITAR_STRINGS; ++i)
        {
            int stringNote = guitarStrings[i].midiNote;
            int diff = currentPitch.midiNote - stringNote;
            
            while (diff > 6) diff -= 12;
            while (diff < -6) diff += 12;
            
            float centsDiff = diff * 100.0f + currentPitch.cents;
            
            if (std::abs(centsDiff) < std::abs(minCentsDiff))
            {
                minCentsDiff = centsDiff;
                nearest = i;
            }
        }
        
        currentPitch.nearestGuitarString = nearest;
        currentPitch.stringCents = minCentsDiff;
    }

    Params params;
    bool bypassed = false;
    bool isPrepared = false;
    double sampleRate = 44100.0;
    
    int yinBufferSize = 2048;
    std::vector<float> yinBuffer;
    std::vector<float> yinDiff;
    std::vector<float> yinCMND;
    
    std::vector<float> inputBuffer;
    int inputWritePos = 0;
    int analysisCounter = 0;
    
    std::vector<float> freqHistory;
    int freqHistoryIdx = 0;
    float smoothedFreq = 0.0f;
    
    int lockedNote = -1;
    int pendingNote = -1;
    int lockCounter = 0;
    int unlockCounter = 0;
    
    PitchInfo currentPitch;
};

inline const PitchProcessor::GuitarString PitchProcessor::guitarStrings[PitchProcessor::NUM_GUITAR_STRINGS] = {
    {"E2",  82.41f, 40}, {"A2", 110.00f, 45}, {"D3", 146.83f, 50},
    {"G3", 196.00f, 55}, {"B3", 246.94f, 59}, {"E4", 329.63f, 64}
};
