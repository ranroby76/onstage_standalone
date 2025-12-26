#pragma once
#include <juce_dsp/juce_dsp.h>
#include "SimplePitchShifter.h"

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
            float semitones = 0.0f;      // -12 to +12
            float pan = 0.0f;             // -1.0 (left) to +1.0 (right)
            float gainDb = 0.0f;          // -inf to 0dB
            float delayMs = 0.0f;         // 0 to 200ms
            
            bool operator==(const Voice& other) const {
                return enabled == other.enabled && 
                       semitones == other.semitones &&
                       pan == other.pan && 
                       gainDb == other.gainDb && 
                       delayMs == other.delayMs;
            }
        };

        Voice voices[4];  // 4 voices now!
        
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
        // Initialize with sensible defaults
        params.voices[0].enabled = false;
        params.voices[0].semitones = 3.0f;   // Minor 3rd
        params.voices[0].pan = -0.3f;        // Slight left
        
        params.voices[1].enabled = false;
        params.voices[1].semitones = 7.0f;   // Perfect 5th
        params.voices[1].pan = 0.3f;         // Slight right
        
        params.voices[2].enabled = false;
        params.voices[2].semitones = -4.0f;  // Major 3rd down
        params.voices[2].pan = -0.6f;        // More left
        
        params.voices[3].enabled = false;
        params.voices[3].semitones = 12.0f;  // Octave up
        params.voices[3].pan = 0.0f;         // Center
    }

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        maxBlockSize = (int)spec.maximumBlockSize;
        
        for (int i = 0; i < 4; ++i)
        {
            pitchShifters[i].prepare(sampleRate, maxBlockSize);
            currentPitchShift[i] = 0.0f;
            
            // Prepare delay buffers (200ms max at 96kHz = 19200 samples)
            int maxDelaySamples = (int)(0.2 * sampleRate) + 1;
            delayBuffers[i].setSize(1, maxDelaySamples);
            delayBuffers[i].clear();
            delayWritePos[i] = 0;
        }
        
        wetBuffer.setSize(2, spec.maximumBlockSize);  // Stereo now!
    }

    void reset()
    {
        for (int i = 0; i < 4; ++i) 
        {
            pitchShifters[i].reset();
            currentPitchShift[i] = 0.0f;
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
        auto* channelData = block.getChannelPointer(0);  // Mono input
        int numSamples = (int)block.getNumSamples();

        // Smooth pitch transitions
        float targetShift[4];
        for (int v = 0; v < 4; ++v)
            targetShift[v] = params.voices[v].enabled ? params.voices[v].semitones : 0.0f;

        float glideCoeff = 1.0f - std::exp(-1.0f / (params.glideMs * 0.001f * sampleRate / numSamples));
        for (int v = 0; v < 4; ++v)
            currentPitchShift[v] += (targetShift[v] - currentPitchShift[v]) * glideCoeff;

        // Clear wet buffer (stereo)
        if (wetBuffer.getNumSamples() < numSamples)
            wetBuffer.setSize(2, numSamples, true, false, true);
        wetBuffer.clear();
        
        float* wetLeft = wetBuffer.getWritePointer(0);
        float* wetRight = wetBuffer.getWritePointer(1);

        // Process each voice
        for (int v = 0; v < 4; ++v)
        {
            if (!params.voices[v].enabled) continue;
            
            pitchShifters[v].setTransposeSemitones(currentPitchShift[v]);
            float gain = juce::Decibels::decibelsToGain(params.voices[v].gainDb);
            float pan = params.voices[v].pan;  // -1 to +1
            
            // Calculate stereo gains from pan
            float leftGain = gain * std::sqrt(0.5f * (1.0f - pan));
            float rightGain = gain * std::sqrt(0.5f * (1.0f + pan));
            
            // Calculate delay in samples
            int delaySamples = (int)(params.voices[v].delayMs * 0.001f * sampleRate);
            int maxDelay = delayBuffers[v].getNumSamples();
            
            for (int i = 0; i < numSamples; ++i)
            {
                // Write input to delay buffer
                delayBuffers[v].setSample(0, delayWritePos[v], channelData[i]);
                
                // Read delayed signal
                int readPos = (delayWritePos[v] - delaySamples + maxDelay) % maxDelay;
                float delayedInput = delayBuffers[v].getSample(0, readPos);
                
                // Pitch shift the delayed signal
                float shifted;
                pitchShifters[v].processSample(delayedInput, shifted);
                
                // Add to wet buffer with pan
                wetLeft[i] += shifted * leftGain;
                wetRight[i] += shifted * rightGain;
                
                // Advance delay write position
                delayWritePos[v] = (delayWritePos[v] + 1) % maxDelay;
            }
        }

        // MIX: Add harmonies on top of dry signal
        float wetGain = juce::Decibels::decibelsToGain(params.wetDb);
        
        // If input is mono, duplicate to stereo with wet signal
        if (block.getNumChannels() == 1)
        {
            // Expand to stereo
            for (int i = 0; i < numSamples; ++i)
            {
                channelData[i] += (wetLeft[i] + wetRight[i]) * 0.5f * wetGain;
            }
        }
        else
        {
            // Stereo mix
            auto* rightChannel = block.getChannelPointer(1);
            for (int i = 0; i < numSamples; ++i)
            {
                channelData[i] += wetLeft[i] * wetGain;
                rightChannel[i] += wetRight[i] * wetGain;
            }
        }
    }

private:
    Params params;
    bool bypassed = false;
    double sampleRate = 44100.0;
    int maxBlockSize = 512;
    
    SimplePitchShifter pitchShifters[4];
    float currentPitchShift[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    
    juce::AudioBuffer<float> wetBuffer;  // Stereo wet buffer
    
    // Delay buffers for each voice
    juce::AudioBuffer<float> delayBuffers[4];
    int delayWritePos[4] = {0, 0, 0, 0};
};