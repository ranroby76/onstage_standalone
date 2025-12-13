#include "ReverbProcessor.h"
#include "BinaryData.h" 

ReverbProcessor::ReverbProcessor()
{
}

void ReverbProcessor::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    
    convolution.prepare(spec);
    
    // Load IR *after* prepare to ensure sample rate is known for resampling
    if (params.irFilePath.isNotEmpty())
    {
        loadExternalIR(juce::File(params.irFilePath));
    }
    else
    {
        loadEmbeddedIR();
    }
    
    lowCutFilter.prepare(spec);
    highCutFilter.prepare(spec);
    
    updateFilters();

    dryBuffer.setSize(spec.numChannels, spec.maximumBlockSize);
}

void ReverbProcessor::reset()
{
    convolution.reset();
    lowCutFilter.reset();
    highCutFilter.reset();
}

void ReverbProcessor::loadEmbeddedIR()
{
    if (BinaryData::ir_wavSize > 0)
    {
        convolution.loadImpulseResponse(
            BinaryData::ir_wav,
            BinaryData::ir_wavSize,
            juce::dsp::Convolution::Stereo::yes,
            juce::dsp::Convolution::Trim::no,
            0,
            juce::dsp::Convolution::Normalise::yes
        );
        convolution.reset(); // CRITICAL: Reset state after load
        currentIrName = "Default (Internal)";
    }
}

void ReverbProcessor::loadExternalIR(const juce::File& file)
{
    if (file.existsAsFile())
    {
        convolution.loadImpulseResponse(
            file,
            juce::dsp::Convolution::Stereo::yes,
            juce::dsp::Convolution::Trim::no,
            0,
            juce::dsp::Convolution::Normalise::yes
        );
        convolution.reset(); // CRITICAL: Reset state after load
        currentIrName = file.getFileNameWithoutExtension();
    }
    else
    {
        loadEmbeddedIR();
        currentIrName = "File Not Found (Default)";
    }
}

juce::String ReverbProcessor::getCurrentIrName() const
{
    return currentIrName;
}

void ReverbProcessor::process(juce::AudioBuffer<float>& buffer)
{
    if (bypassed)
        return;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    // Ensure scratch buffer size
    if (dryBuffer.getNumSamples() < numSamples)
        dryBuffer.setSize(numChannels, numSamples, true, false, true);

    // 1. Save Dry Signal
    for (int ch = 0; ch < numChannels; ++ch)
        dryBuffer.copyFrom(ch, 0, buffer.getReadPointer(ch), numSamples);

    // 2. Process Convolution (Replaces buffer with Wet signal)
    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    convolution.process(context);

    // 3. Process Post-EQ on Wet
    lowCutFilter.process(context);
    highCutFilter.process(context);

    // 4. Apply Wet Gain to the Wet signal
    buffer.applyGain(0, numSamples, params.wetGain);

    // 5. Add Dry Signal back
    for (int ch = 0; ch < numChannels; ++ch)
    {
        buffer.addFrom(ch, 0, dryBuffer.getReadPointer(ch), numSamples);
    }
}

void ReverbProcessor::setParams(const Params& newParams)
{
    bool irChanged = (params.irFilePath != newParams.irFilePath);
    params = newParams;
    
    if (irChanged)
    {
        if (params.irFilePath.isNotEmpty())
            loadExternalIR(juce::File(params.irFilePath));
        else
            loadEmbeddedIR();
    }

    updateFilters();
}

void ReverbProcessor::updateFilters()
{
    if (sampleRate <= 0.0) return;

    *lowCutFilter.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(
        sampleRate, params.lowCutHz, 0.707f);

    *highCutFilter.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(
        sampleRate, params.highCutHz, 0.707f);
}