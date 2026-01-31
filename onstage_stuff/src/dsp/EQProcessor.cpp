#include "EQProcessor.h"

EQProcessor::EQProcessor()
    : lowFreq(100.0f)
    , midFreq(1000.0f)
    , highFreq(10000.0f)
    , lowGain(0.0f)
    , midGain(0.0f)
    , highGain(0.0f)
    , lowQ(0.707f)
    , midQ(0.707f)
    , highQ(0.707f)
    , sampleRate(44100.0)
    , bypassed(false)
{
}

void EQProcessor::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    for (int ch = 0; ch < 2; ++ch)
    {
        lowShelf[ch].prepare(spec);
        midPeak[ch].prepare(spec);
        highShelf[ch].prepare(spec);
    }
    
    updateFilters();
}

void EQProcessor::process(juce::dsp::ProcessContextReplacing<float>& context)
{
    if (bypassed)
        return;
    
    auto& outputBlock = context.getOutputBlock();
    const int numChannels = static_cast<int>(outputBlock.getNumChannels());
    const int numSamples = static_cast<int>(outputBlock.getNumSamples());

    for (int channel = 0; channel < numChannels && channel < 2; ++channel)
    {
        auto* channelData = outputBlock.getChannelPointer(channel);
        for (int sample = 0; sample < numSamples; ++sample)
        {
            float input = channelData[sample];
            float output = lowShelf[channel].processSample(input);
            output = midPeak[channel].processSample(output);
            output = highShelf[channel].processSample(output);
            
            channelData[sample] = output;
        }
    }
}

void EQProcessor::reset()
{
    for (int ch = 0; ch < 2; ++ch)
    {
        lowShelf[ch].reset();
        midPeak[ch].reset();
        highShelf[ch].reset();
    }
}

void EQProcessor::setParams(const Params& p)
{
    lowFreq = p.lowFreq;
    midFreq = p.midFreq;
    highFreq = p.highFreq;
    lowGain = p.lowGain;
    midGain = p.midGain;
    highGain = p.highGain;
    lowQ = p.lowQ;
    midQ = p.midQ;
    highQ = p.highQ;
    updateFilters();
}

EQProcessor::Params EQProcessor::getParams() const
{
    Params p;
    p.lowFreq = lowFreq; p.midFreq = midFreq; p.highFreq = highFreq;
    p.lowGain = lowGain; p.midGain = midGain; p.highGain = highGain;
    p.lowQ = lowQ; p.midQ = midQ; p.highQ = highQ;
    return p;
}

void EQProcessor::setBypassed(bool shouldBeBypassed) { bypassed = shouldBeBypassed; }
bool EQProcessor::isBypassed() const { return bypassed; }

void EQProcessor::setLowFrequency(float freq) { lowFreq = juce::jlimit(20.0f, 2000.0f, freq); updateFilters(); }
void EQProcessor::setMidFrequency(float freq) { midFreq = juce::jlimit(20.0f, 10000.0f, freq); updateFilters(); }
void EQProcessor::setHighFrequency(float freq) { highFreq = juce::jlimit(2000.0f, 20000.0f, freq); updateFilters(); }
void EQProcessor::setLowGain(float gain) { lowGain = juce::jlimit(-24.0f, 24.0f, gain); updateFilters(); }
void EQProcessor::setMidGain(float gain) { midGain = juce::jlimit(-24.0f, 24.0f, gain); updateFilters(); }
void EQProcessor::setHighGain(float gain) { highGain = juce::jlimit(-24.0f, 24.0f, gain); updateFilters(); }
void EQProcessor::setLowQ(float q) { lowQ = juce::jlimit(0.1f, 10.0f, q); updateFilters(); }
void EQProcessor::setMidQ(float q) { midQ = juce::jlimit(0.1f, 10.0f, q); updateFilters(); }
void EQProcessor::setHighQ(float q) { highQ = juce::jlimit(0.1f, 10.0f, q); updateFilters(); }

float EQProcessor::getLowFrequency() const { return lowFreq; }
float EQProcessor::getMidFrequency() const { return midFreq; }
float EQProcessor::getHighFrequency() const { return highFreq; }
float EQProcessor::getLowGain() const { return lowGain; }
float EQProcessor::getMidGain() const { return midGain; }
float EQProcessor::getHighGain() const { return highGain; }
float EQProcessor::getLowQ() const { return lowQ; }
float EQProcessor::getMidQ() const { return midQ; }
float EQProcessor::getHighQ() const { return highQ; }

void EQProcessor::updateFilters()
{
    if (sampleRate <= 0.0)
        return;

    // FIX: Assign smart pointers directly (avoids crash on null dereference)
    auto lowCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowShelf(
        sampleRate, lowFreq, lowQ, juce::Decibels::decibelsToGain(lowGain)
    );
    auto midCoeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
        sampleRate, midFreq, midQ, juce::Decibels::decibelsToGain(midGain)
    );
    auto highCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighShelf(
        sampleRate, highFreq, highQ, juce::Decibels::decibelsToGain(highGain)
    );

    for (int ch = 0; ch < 2; ++ch)
    {
        lowShelf[ch].coefficients = lowCoeffs;
        midPeak[ch].coefficients = midCoeffs;
        highShelf[ch].coefficients = highCoeffs;
    }
}