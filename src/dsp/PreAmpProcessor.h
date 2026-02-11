// ==============================================================================
//  PreAmpProcessor.h
//  OnStage — Simple input gain boost (0 to +30 dB)
//
//  Lightweight DSP: smoothed linear gain applied to stereo buffer.
//  No popup panel — controlled via inline slider on the wiring canvas.
// ==============================================================================

#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

class PreAmpProcessor
{
public:
    PreAmpProcessor() = default;

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        smoothedGain.reset (sampleRate, 0.02);   // 20 ms ramp
        smoothedGain.setCurrentAndTargetValue (getLinearGain());
    }

    void reset()
    {
        smoothedGain.setCurrentAndTargetValue (getLinearGain());
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        if (bypassed)
            return;

        smoothedGain.setTargetValue (getLinearGain());

        const int numSamples  = buffer.getNumSamples();
        const int numChannels = buffer.getNumChannels();

        for (int i = 0; i < numSamples; ++i)
        {
            const float g = smoothedGain.getNextValue();
            for (int ch = 0; ch < numChannels; ++ch)
                buffer.getWritePointer (ch)[i] *= g;
        }
    }

    // --- Parameter access -----------------------------------------------------
    void  setGainDb (float db)  { gainDb = juce::jlimit (0.0f, 30.0f, db); }
    float getGainDb() const     { return gainDb; }

    bool bypassed = false;

private:
    float getLinearGain() const
    {
        return juce::Decibels::decibelsToGain (gainDb);
    }

    float  gainDb     = 0.0f;
    double sampleRate = 44100.0;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> smoothedGain { 1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PreAmpProcessor)
};
