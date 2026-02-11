#include "EQProcessor.h"

// Initialize static constexpr
constexpr float EQProcessor::kDefaultFrequencies[EQProcessor::kNumBands];

EQProcessor::EQProcessor()
{
    // Initialize default frequencies (logarithmically spaced)
    for (int i = 0; i < kNumBands; ++i)
    {
        params.bands[i].frequency = kDefaultFrequencies[i];
        params.bands[i].gainDb    = 0.0f;  // Unity
        params.bands[i].q         = 1.0f;
    }
}

void EQProcessor::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;

    for (int band = 0; band < kNumBands; ++band)
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            filters[band][ch].prepare(spec);
        }
    }

    updateFilters();
}

void EQProcessor::process(juce::dsp::ProcessContextReplacing<float>& context)
{
    if (bypassed)
        return;

    auto& outputBlock = context.getOutputBlock();
    const int numChannels = static_cast<int>(outputBlock.getNumChannels());
    const int numSamples  = static_cast<int>(outputBlock.getNumSamples());

    for (int channel = 0; channel < numChannels && channel < 2; ++channel)
    {
        auto* channelData = outputBlock.getChannelPointer(channel);

        for (int sample = 0; sample < numSamples; ++sample)
        {
            float value = channelData[sample];

            // Process through all 9 bands in series
            for (int band = 0; band < kNumBands; ++band)
            {
                // Only process if gain is not at unity (optimization)
                if (std::abs(params.bands[band].gainDb) > 0.01f ||
                    params.bands[band].gainDb < -90.0f)  // Handle silence case
                {
                    value = filters[band][channel].processSample(value);
                }
            }

            channelData[sample] = value;
        }
    }
}

void EQProcessor::reset()
{
    for (int band = 0; band < kNumBands; ++band)
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            filters[band][ch].reset();
        }
    }
}

void EQProcessor::setParams(const Params& newParams)
{
    params = newParams;
    updateFilters();
}

EQProcessor::Params EQProcessor::getParams() const
{
    return params;
}

void EQProcessor::setBandParams(int bandIndex, const BandParams& bandParams)
{
    if (bandIndex >= 0 && bandIndex < kNumBands)
    {
        params.bands[bandIndex] = bandParams;
        updateBandFilter(bandIndex);
    }
}

EQProcessor::BandParams EQProcessor::getBandParams(int bandIndex) const
{
    if (bandIndex >= 0 && bandIndex < kNumBands)
        return params.bands[bandIndex];
    return BandParams();
}

void EQProcessor::setBypassed(bool shouldBeBypassed)
{
    bypassed = shouldBeBypassed;
}

bool EQProcessor::isBypassed() const
{
    return bypassed;
}

void EQProcessor::setBandFrequency(int band, float freq)
{
    if (band >= 0 && band < kNumBands)
    {
        params.bands[band].frequency = juce::jlimit(20.0f, 20000.0f, freq);
        updateBandFilter(band);
    }
}

void EQProcessor::setBandGain(int band, float gainDb)
{
    if (band >= 0 && band < kNumBands)
    {
        // -100 dB = silence, 0 = unity, +30 = max boost
        params.bands[band].gainDb = juce::jlimit(-100.0f, 30.0f, gainDb);
        updateBandFilter(band);
    }
}

void EQProcessor::setBandQ(int band, float q)
{
    if (band >= 0 && band < kNumBands)
    {
        params.bands[band].q = juce::jlimit(0.1f, 10.0f, q);
        updateBandFilter(band);
    }
}

float EQProcessor::getBandFrequency(int band) const
{
    if (band >= 0 && band < kNumBands)
        return params.bands[band].frequency;
    return 1000.0f;
}

float EQProcessor::getBandGain(int band) const
{
    if (band >= 0 && band < kNumBands)
        return params.bands[band].gainDb;
    return 0.0f;
}

float EQProcessor::getBandQ(int band) const
{
    if (band >= 0 && band < kNumBands)
        return params.bands[band].q;
    return 1.0f;
}

void EQProcessor::updateFilters()
{
    for (int band = 0; band < kNumBands; ++band)
    {
        updateBandFilter(band);
    }
}

void EQProcessor::updateBandFilter(int bandIndex)
{
    if (sampleRate <= 0.0 || bandIndex < 0 || bandIndex >= kNumBands)
        return;

    const auto& bp = params.bands[bandIndex];

    // Handle silence case: if gain is very low, set gain to essentially zero
    float linearGain;
    if (bp.gainDb <= -100.0f)
    {
        // Silence - use a very small gain
        linearGain = 0.0001f;
    }
    else
    {
        linearGain = juce::Decibels::decibelsToGain(bp.gainDb);
    }

    // Create bell (peak) filter coefficients
    auto coeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
        sampleRate,
        bp.frequency,
        bp.q,
        linearGain
    );

    for (int ch = 0; ch < 2; ++ch)
    {
        filters[bandIndex][ch].coefficients = coeffs;
    }
}
