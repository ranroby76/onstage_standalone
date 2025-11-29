#include "ExciterProcessor.h"

ExciterProcessor::ExciterProcessor()
{
}

void ExciterProcessor::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    
    highPassFilter.prepare(spec);
    driveGain.prepare(spec);
    driveGain.setRampDurationSeconds(0.05);
    
    wetBuffer.setSize(spec.numChannels, spec.maximumBlockSize);
    
    updateFilter();
}

void ExciterProcessor::reset()
{
    highPassFilter.reset();
    driveGain.reset();
    wetBuffer.clear();
}

void ExciterProcessor::setParams(const Params& newParams)
{
    if (params != newParams)
    {
        params = newParams;
        updateFilter();
        
        // Convert dB to linear gain for the drive
        driveGain.setGainDecibels(params.amount);
    }
}

void ExciterProcessor::updateFilter()
{
    if (sampleRate <= 0.0) return;
    
    *highPassFilter.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, params.frequency);
}

void ExciterProcessor::process(juce::dsp::ProcessContextReplacing<float>& context)
{
    if (bypassed || params.mix <= 0.001f)
        return;

    const auto& inputBlock = context.getInputBlock();
    auto& outputBlock = context.getOutputBlock();
    const int numSamples = (int)outputBlock.getNumSamples();
    const int numChannels = (int)outputBlock.getNumChannels();

    // 1. Prepare Wet Buffer (Copy Input)
    // We need a scratch buffer because we are splitting the signal
    if (wetBuffer.getNumSamples() < numSamples)
        wetBuffer.setSize(numChannels, numSamples, true, false, true);

    juce::dsp::AudioBlock<float> wetBlock(wetBuffer.getArrayOfWritePointers(), numChannels, numSamples);
    wetBlock.copyFrom(inputBlock);

    juce::dsp::ProcessContextReplacing<float> wetContext(wetBlock);

    // 2. Filter: Keep only high frequencies
    highPassFilter.process(wetContext);

    // 3. Drive: Boost level into saturation
    driveGain.process(wetContext);

    // 4. Saturate: Generate Harmonics using tanh (Soft Clipping)
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = wetBlock.getChannelPointer(ch);
        for (int i = 0; i < numSamples; ++i)
        {
            // Tanh generates odd harmonics
            // Rectification (std::abs) would generate even harmonics, 
            // but tanh is standard for basic exciters.
            data[i] = std::tanh(data[i]);
        }
    }

    // 5. Mix: Add Wet ("Air") back to Dry
    // Exciter is typically additive.
    float mix = params.mix;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* src = inputBlock.getChannelPointer(ch);
        auto* wet = wetBlock.getChannelPointer(ch);
        auto* dst = outputBlock.getChannelPointer(ch);

        for (int i = 0; i < numSamples; ++i)
        {
            // Original Dry + (Excited Highs * Mix)
            dst[i] = src[i] + (wet[i] * mix);
        }
    }
}