#pragma once
#include <juce_dsp/juce_dsp.h>

class CompressorProcessor
{
public:
    struct Params
    {
        float thresholdDb { -18.0f };
        float ratio { 3.0f };
        float attackMs { 8.0f };
        float releaseMs { 120.0f };
        float makeupDb { 0.0f };
        
        bool operator==(const Params& other) const
        {
            return thresholdDb == other.thresholdDb &&
                   ratio == other.ratio &&
                   attackMs == other.attackMs &&
                   releaseMs == other.releaseMs &&
                   makeupDb == other.makeupDb;
        }
        
        bool operator!=(const Params& other) const { return !(*this == other); }
    };

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        comp.reset(); makeup.reset();
        comp.prepare (spec);
        makeup.prepare (spec);
        applyParams();
        isPrepared = true;
        currentInputLevel = 0.0f;
    }

    void reset()
    {
        comp.reset();
        makeup.reset();
        currentInputLevel = 0.0f;
    }

    void setParams (const Params& p)
    {
        params = p;
        if (isPrepared)
            applyParams();
    }

    Params getParams() const { return params; }

    template <typename Context>
    void process (Context&& ctx)
    {
        if (bypassed || !isPrepared)
            return;

        // Capture input level for visualization
        auto& block = ctx.getOutputBlock();
        float sumSquares = 0.0f;
        int totalSamples = 0;
        
        for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
        {
            auto* data = block.getChannelPointer(ch);
            for (size_t i = 0; i < block.getNumSamples(); ++i)
            {
                sumSquares += data[i] * data[i];
                totalSamples++;
            }
        }
        
        if (totalSamples > 0)
        {
            float rms = std::sqrt(sumSquares / totalSamples);
            currentInputLevel = juce::Decibels::gainToDecibels(rms + 1e-6f);
        }

        comp.process (ctx);
        makeup.process (ctx);
    }

    void setBypassed(bool shouldBypass) { bypassed = shouldBypass; }
    bool isBypassed() const { return bypassed; }
    
    // NEW: Get current input level for visualization
    float getCurrentInputLevelDb() const { return currentInputLevel; }
    
    // NEW: Calculate theoretical gain reduction at given input level
    float getGainReductionDb(float inputDb) const
    {
        if (inputDb <= params.thresholdDb)
            return 0.0f;
        
        float overThreshold = inputDb - params.thresholdDb;
        return overThreshold * (1.0f - (1.0f / params.ratio));
    }

private:
    void applyParams()
    {
        comp.setThreshold (params.thresholdDb);
        comp.setRatio (params.ratio);
        comp.setAttack (params.attackMs / 1000.0f);
        comp.setRelease (params.releaseMs / 1000.0f);
        makeup.setGainDecibels (params.makeupDb);
    }

    Params params;
    bool bypassed = false;
    bool isPrepared = false;
    juce::dsp::Compressor<float> comp;
    juce::dsp::Gain<float> makeup;
    std::atomic<float> currentInputLevel { 0.0f };
};
