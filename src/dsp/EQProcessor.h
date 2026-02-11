#pragma once

#include <juce_dsp/juce_dsp.h>

// ==============================================================================
//  9-Band Parametric EQ Processor (All Bell Filters)
// ==============================================================================
class EQProcessor
{
public:
    static constexpr int kNumBands = 9;

    struct BandParams
    {
        float frequency = 1000.0f;  // 20Hz - 20kHz
        float gainDb    = 0.0f;     // -inf (silence) to +30dB, 0 = unity
        float q         = 1.0f;     // 0.1 - 10.0

        bool operator==(const BandParams& other) const
        {
            return frequency == other.frequency &&
                   gainDb    == other.gainDb &&
                   q         == other.q;
        }
        bool operator!=(const BandParams& other) const { return !(*this == other); }
    };

    struct Params
    {
        BandParams bands[kNumBands];

        bool operator==(const Params& other) const
        {
            for (int i = 0; i < kNumBands; ++i)
                if (bands[i] != other.bands[i])
                    return false;
            return true;
        }
        bool operator!=(const Params& other) const { return !(*this == other); }
    };

    EQProcessor();

    void prepare(const juce::dsp::ProcessSpec& spec);
    void process(juce::dsp::ProcessContextReplacing<float>& context);
    void reset();

    void setParams(const Params& params);
    Params getParams() const;

    void setBandParams(int bandIndex, const BandParams& params);
    BandParams getBandParams(int bandIndex) const;

    void setBypassed(bool shouldBeBypassed);
    bool isBypassed() const;

    // Individual setters for convenience
    void setBandFrequency(int band, float freq);
    void setBandGain(int band, float gainDb);
    void setBandQ(int band, float q);

    float getBandFrequency(int band) const;
    float getBandGain(int band) const;
    float getBandQ(int band) const;

private:
    void updateFilters();
    void updateBandFilter(int bandIndex);

    Params params;
    double sampleRate = 44100.0;
    bool bypassed = false;

    // Stereo filters for each band
    juce::dsp::IIR::Filter<float> filters[kNumBands][2];

    // Default frequencies for 9 bands (logarithmically spaced)
    static constexpr float kDefaultFrequencies[kNumBands] = {
        31.0f, 63.0f, 125.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f
    };
};
