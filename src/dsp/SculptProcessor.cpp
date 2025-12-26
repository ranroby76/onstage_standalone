/*
  ==============================================================================

    SculptProcessor.cpp
    OnStage

  ==============================================================================
*/

#include "SculptProcessor.h"
#include <cmath>

SculptProcessor::SculptProcessor()
{
}

void SculptProcessor::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    
    mudFilter.prepare(spec);
    harshFilter.prepare(spec);
    airFilter.prepare(spec);
    
    mudFilter.reset();
    harshFilter.reset();
    airFilter.reset();
    
    updateFilters();
}

void SculptProcessor::reset()
{
    mudFilter.reset();
    harshFilter.reset();
    airFilter.reset();
}

void SculptProcessor::process(juce::dsp::ProcessContextReplacing<float>& context)
{
    if (bypassed) return;

    auto& block = context.getOutputBlock();
    int numSamples = (int)block.getNumSamples();
    int numChannels = (int)block.getNumChannels();

    // 1. SATURATION (Per Sample) with Mode Selection
    if (params.drive > 0.01f)
    {
        float driveMult = 1.0f + (params.drive * 2.0f); // 1x to 3x gain
        float comp = 1.0f / (1.0f + params.drive * 0.5f); // Compensation

        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* data = block.getChannelPointer(ch);
            
            switch (params.mode)
            {
                case SaturationMode::Tube:
                    // Odd harmonics (3rd, 5th, 7th) - Bright, present
                    for (int i = 0; i < numSamples; ++i)
                    {
                        data[i] = std::tanh(data[i] * driveMult) * comp;
                    }
                    break;
                    
                case SaturationMode::Tape:
                    // Even harmonics (2nd, 4th, 6th) - Warm, smooth
                    for (int i = 0; i < numSamples; ++i)
                    {
                        // Asymmetric transfer function adds even harmonics
                        float input = data[i];
                        float biased = input * driveMult + (0.2f * input * input);
                        data[i] = std::tanh(biased) * comp;
                    }
                    break;
                    
                case SaturationMode::Hybrid:
                    // Blend of tube and tape - Balanced
                    for (int i = 0; i < numSamples; ++i)
                    {
                        float input = data[i];
                        // Tube component (odd harmonics)
                        float tube = std::tanh(input * driveMult);
                        // Tape component (even harmonics)
                        float biased = input * driveMult + (0.15f * input * input);
                        float tape = std::tanh(biased);
                        // 50/50 blend
                        data[i] = (tube * 0.5f + tape * 0.5f) * comp;
                    }
                    break;
            }
        }
    }

    // 2. SCULPTING FILTERS (Block Process)
    mudFilter.process(context);
    harshFilter.process(context);
    airFilter.process(context);
}

void SculptProcessor::setParams(const Params& newParams)
{
    params = newParams;
    updateFilters();
}

void SculptProcessor::updateFilters()
{
    if (sampleRate <= 0.0) return;

    // 1. Mud Cut (Dip at 300Hz)
    // Param 0..1 maps to 0dB .. -12dB
    float mudGainDb = -12.0f * params.mudCut;
    *mudFilter.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(
        sampleRate, MUD_FREQ, 1.5f, juce::Decibels::decibelsToGain(mudGainDb));

    // 2. Harsh Cut (Dip at 3.5kHz)
    // Param 0..1 maps to 0dB .. -12dB
    float harshGainDb = -12.0f * params.harshCut;
    *harshFilter.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(
        sampleRate, HARSH_FREQ, 2.0f, juce::Decibels::decibelsToGain(harshGainDb));

    // 3. Air Boost (Shelf at 12kHz)
    // Param 0..1 maps to 0dB .. +10dB
    float airGainDb = 10.0f * params.air;
    *airFilter.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf(
        sampleRate, AIR_FREQ, 0.7f, juce::Decibels::decibelsToGain(airGainDb));
}