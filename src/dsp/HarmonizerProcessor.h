#pragma once
#include <juce_dsp/juce_dsp.h>
#include "SimplePitchShifter.h" // Include the shared class

class HarmonizerProcessor
{
public:
    struct Params
    {
        bool enabled = true;
        float wetDb = 0.0f; 
        float glideMs = 50.0f;
        bool useDiatonicMode = false;
        int keyRoot = 0;
        bool isMinorScale = false;
        struct Voice
        {
            bool enabled = false;
            int steps = 0; 
            float fixedSemitones = 0.0f;
            float centsDetune = 0.0f;
            float gainDb = 0.0f;
            float pan = 0.0f;
            float delayMs = 0.0f;
            
            bool operator==(const Voice& other) const {
                return enabled == other.enabled && steps == other.steps && fixedSemitones == other.fixedSemitones &&
                       centsDetune == other.centsDetune && gainDb == other.gainDb;
            }
        };

        Voice voices[2];
        bool operator==(const Params& other) const {
            return enabled == other.enabled && wetDb == other.wetDb && voices[0] == other.voices[0] && voices[1] == other.voices[1];
        }
        bool operator!=(const Params& other) const { return !(*this == other);
        }
    };

    HarmonizerProcessor()
    {
        params.voices[0].enabled = false;
        params.voices[0].fixedSemitones = 3.0f;
        params.voices[1].enabled = false;
        params.voices[1].fixedSemitones = 7.0f;
        params.voices[1].gainDb = -3.0f;
    }

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        for (int i = 0; i < 2; ++i)
            pitchShifters[i].prepare(sampleRate, (int)spec.maximumBlockSize);
        currentPitchShift[0] = 0.0f;
        currentPitchShift[1] = 0.0f;
        
        wetBuffer.setSize(1, spec.maximumBlockSize);
    }

    void reset()
    {
        for (int i = 0; i < 2; ++i) pitchShifters[i].reset();
        currentPitchShift[0] = 0.0f;
        currentPitchShift[1] = 0.0f;
    }

    void setParams(const Params& p) { params = p;
    }
    Params getParams() const { return params;
    }
    void setBypassed(bool shouldBypass) { bypassed = shouldBypass;
    }
    bool isBypassed() const { return bypassed;
    }

    template <typename Context>
    void process(Context&& ctx)
    {
        if (!params.enabled || bypassed) return;
        auto& block = ctx.getOutputBlock();
        auto* channelData = block.getChannelPointer(0);
        int numSamples = (int)block.getNumSamples();

        float targetShift[2];
        for (int v = 0; v < 2; ++v)
            targetShift[v] = params.voices[v].enabled ?
            params.voices[v].fixedSemitones + params.voices[v].centsDetune * 0.01f : 0.0f;

        float glideCoeff = 1.0f - std::exp(-1.0f / (params.glideMs * 0.001f * sampleRate / numSamples));
        for (int v = 0; v < 2; ++v)
            currentPitchShift[v] += (targetShift[v] - currentPitchShift[v]) * glideCoeff;
        if (wetBuffer.getNumSamples() < numSamples)
            wetBuffer.setSize(1, numSamples, true, false, true);
        juce::FloatVectorOperations::clear(wetBuffer.getWritePointer(0), numSamples);
        float* wetData = wetBuffer.getWritePointer(0);

        for (int v = 0; v < 2; ++v)
        {
            if (!params.voices[v].enabled) continue;
            pitchShifters[v].setTransposeSemitones(currentPitchShift[v]);
            float gain = juce::Decibels::decibelsToGain(params.voices[v].gainDb);

            for (int i = 0; i < numSamples; ++i)
            {
                float shifted;
                pitchShifters[v].processSample(channelData[i], shifted);
                wetData[i] += shifted * gain;
            }
        }

        // MIX: Additive instead of replacement
        float wetGain = juce::Decibels::decibelsToGain(params.wetDb);
        for (int i = 0; i < numSamples; ++i)
        {
            // Add harmonies on top of dry signal
            channelData[i] += wetData[i] * wetGain;
        }
    }

private:
    Params params;
    bool bypassed = false;
    double sampleRate = 44100.0;
    SimplePitchShifter pitchShifters[2];
    float currentPitchShift[2] = {0.0f, 0.0f};
    
    juce::AudioBuffer<float> wetBuffer; 
};