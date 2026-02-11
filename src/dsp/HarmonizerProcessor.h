// ==============================================================================
//  HarmonizerProcessor.h
//  OnStage - 4-voice harmonizer with RubberBand pitch shift + formant control
//
//  Uses RubberBandLiveShifter for high-quality pitch shifting with
//  independent formant control per voice. Replaces the old
//  SimplePitchShifter + FormantShifter combo.
// ==============================================================================

#pragma once
#include <juce_dsp/juce_dsp.h>
#include "RubberBandPitchShifter.h"

class HarmonizerProcessor
{
public:
    struct Params
    {
        bool enabled = true;
        float wetDb = 0.0f; 
        float glideMs = 50.0f;
        
        struct Voice
        {
            bool enabled = false;
            float semitones = 0.0f;       // -12 to +12
            float pan = 0.0f;             // -1.0 (left) to +1.0 (right)
            float gainDb = 0.0f;          // -inf to 0dB
            float delayMs = 0.0f;         // 0 to 200ms
            float formant = 0.0f;         // -12 to +12 semitones (formant shift)
            
            bool operator==(const Voice& other) const {
                return enabled == other.enabled && 
                       semitones == other.semitones &&
                       pan == other.pan && 
                       gainDb == other.gainDb && 
                       delayMs == other.delayMs &&
                       formant == other.formant;
            }
        };

        Voice voices[4];
        
        bool operator==(const Params& other) const {
            return enabled == other.enabled && 
                   wetDb == other.wetDb && 
                   glideMs == other.glideMs &&
                   voices[0] == other.voices[0] && 
                   voices[1] == other.voices[1] &&
                   voices[2] == other.voices[2] && 
                   voices[3] == other.voices[3];
        }
        bool operator!=(const Params& other) const { return !(*this == other); }
    };

    HarmonizerProcessor()
    {
        // Initialize with sensible defaults for Boney-M style trio
        params.voices[0].enabled = false;
        params.voices[0].semitones = 3.0f;   // Minor 3rd
        params.voices[0].pan = -0.4f;
        params.voices[0].formant = 2.0f;     // Slight formant up
        
        params.voices[1].enabled = false;
        params.voices[1].semitones = 7.0f;   // Perfect 5th
        params.voices[1].pan = 0.4f;
        params.voices[1].formant = -1.0f;    // Slight formant down
        
        params.voices[2].enabled = false;
        params.voices[2].semitones = -4.0f;  // Major 3rd down
        params.voices[2].pan = -0.7f;
        params.voices[2].formant = -3.0f;    // More masculine
        
        params.voices[3].enabled = false;
        params.voices[3].semitones = 12.0f;  // Octave up
        params.voices[3].pan = 0.0f;
        params.voices[3].formant = 4.0f;     // More feminine
    }

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        maxBlockSize = (int)spec.maximumBlockSize;
        
        for (int i = 0; i < 4; ++i)
        {
            rbShifters[i].prepare(sampleRate, maxBlockSize);
            currentPitchShift[i] = 0.0f;
            currentFormantShift[i] = 0.0f;
            
            int maxDelaySamples = (int)(0.2 * sampleRate) + 1;
            delayBuffers[i].setSize(1, maxDelaySamples);
            delayBuffers[i].clear();
            delayWritePos[i] = 0;
        }
        
        wetBuffer.setSize(2, spec.maximumBlockSize);
        tempBuffer.setSize(1, spec.maximumBlockSize);
    }

    void reset()
    {
        for (int i = 0; i < 4; ++i) 
        {
            rbShifters[i].reset();
            currentPitchShift[i] = 0.0f;
            currentFormantShift[i] = 0.0f;
            delayBuffers[i].clear();
            delayWritePos[i] = 0;
        }
    }

    void setParams(const Params& p) { params = p; }
    Params getParams() const { return params; }
    void setBypassed(bool shouldBypass) { bypassed = shouldBypass; }
    bool isBypassed() const { return bypassed; }

    template <typename Context>
    void process(Context&& ctx)
    {
        if (!params.enabled || bypassed) return;
        
        auto& block = ctx.getOutputBlock();
        auto* channelData = block.getChannelPointer(0);
        int numSamples = (int)block.getNumSamples();

        // Smooth pitch and formant transitions
        float targetShift[4];
        float targetFormant[4];
        for (int v = 0; v < 4; ++v)
        {
            targetShift[v] = params.voices[v].enabled ? params.voices[v].semitones : 0.0f;
            targetFormant[v] = params.voices[v].enabled ? params.voices[v].formant : 0.0f;
        }

        float glideCoeff = 1.0f - std::exp(-1.0f / (params.glideMs * 0.001f * sampleRate / numSamples));
        for (int v = 0; v < 4; ++v)
        {
            currentPitchShift[v] += (targetShift[v] - currentPitchShift[v]) * glideCoeff;
            currentFormantShift[v] += (targetFormant[v] - currentFormantShift[v]) * glideCoeff;
        }

        if (wetBuffer.getNumSamples() < numSamples)
            wetBuffer.setSize(2, numSamples, true, false, true);
        if (tempBuffer.getNumSamples() < numSamples)
            tempBuffer.setSize(1, numSamples, true, false, true);
        wetBuffer.clear();
        
        float* wetLeft = wetBuffer.getWritePointer(0);
        float* wetRight = wetBuffer.getWritePointer(1);

        for (int v = 0; v < 4; ++v)
        {
            if (!params.voices[v].enabled) continue;
            
            // Set pitch and formant for RubberBand shifter
            // RubberBand with FormantPreserved: formants stay at original position by default.
            // formant param = 0 means preserve formants (natural sound)
            // formant param != 0 means shift formants by that amount from original
            rbShifters[v].setTransposeSemitones(currentPitchShift[v]);
            rbShifters[v].setFormantSemitones(currentFormantShift[v]);
            
            float gain = juce::Decibels::decibelsToGain(params.voices[v].gainDb);
            float pan = params.voices[v].pan;
            
            float leftGain = gain * std::sqrt(0.5f * (1.0f - pan));
            float rightGain = gain * std::sqrt(0.5f * (1.0f + pan));
            
            int delaySamples = (int)(params.voices[v].delayMs * 0.001f * sampleRate);
            int maxDelay = delayBuffers[v].getNumSamples();
            
            for (int i = 0; i < numSamples; ++i)
            {
                // Voice delay
                delayBuffers[v].setSample(0, delayWritePos[v], channelData[i]);
                
                int readPos = (delayWritePos[v] - delaySamples + maxDelay) % maxDelay;
                float delayedInput = delayBuffers[v].getSample(0, readPos);
                
                // RubberBand pitch + formant shift (single processor handles both)
                float output;
                rbShifters[v].processSample(delayedInput, output);
                
                // Safety: clamp NaN/inf
                if (std::isnan(output) || std::isinf(output))
                    output = 0.0f;
                
                wetLeft[i] += output * leftGain;
                wetRight[i] += output * rightGain;
                
                delayWritePos[v] = (delayWritePos[v] + 1) % maxDelay;
            }
        }

        float wetGain = juce::Decibels::decibelsToGain(params.wetDb);
        
        if (block.getNumChannels() == 1)
        {
            for (int i = 0; i < numSamples; ++i)
                channelData[i] += (wetLeft[i] + wetRight[i]) * 0.5f * wetGain;
        }
        else
        {
            auto* leftChannel = block.getChannelPointer(0);
            auto* rightChannel = block.getChannelPointer(1);
            for (int i = 0; i < numSamples; ++i)
            {
                leftChannel[i] += wetLeft[i] * wetGain;
                rightChannel[i] += wetRight[i] * wetGain;
            }
        }
    }

private:
    Params params;
    bool bypassed = false;
    double sampleRate = 44100.0;
    int maxBlockSize = 512;
    
    RubberBandPitchShifter rbShifters[4];     // RubberBand pitch + formant shifting
    float currentPitchShift[4] = {0, 0, 0, 0};
    float currentFormantShift[4] = {0, 0, 0, 0};
    
    juce::AudioBuffer<float> delayBuffers[4];
    int delayWritePos[4] = {0, 0, 0, 0};
    
    juce::AudioBuffer<float> wetBuffer;
    juce::AudioBuffer<float> tempBuffer;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HarmonizerProcessor)
};
