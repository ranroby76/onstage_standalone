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
    }

    void reset()
    {
        comp.reset();
        makeup.reset();
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
            return; // Skip processing if bypassed or not ready

        comp.process (ctx);
        makeup.process (ctx);
    }

    void setBypassed(bool shouldBypass) { bypassed = shouldBypass; }
    bool isBypassed() const { return bypassed; }

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
};