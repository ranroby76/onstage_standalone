#pragma once
#include <juce_dsp/juce_dsp.h>

class DelayProcessor
{
public:
    struct Params
    {
        float delayMs = 350.0f;
        
        // "Ratio": Loudness of first repeat (0.0 to 1.0) relative to Dry
        float ratio = 0.5f; 
        
        // "Stage": Gain reduction per repeat (0.0 = no reduction, 1.0 = full kill)
        float stage = 0.25f; 
        
        // "Mix": Master volume for the wet signal (0.0 to 1.0)
        float mix = 1.0f;

        float stereoWidth = 1.0f;
        float lowCutHz = 200.0f;
        float highCutHz = 8000.0f;
        
        bool operator==(const Params& other) const
        {
            return delayMs == other.delayMs &&
                   ratio == other.ratio &&
                   stage == other.stage &&
                   mix == other.mix &&
                   stereoWidth == other.stereoWidth &&
                   lowCutHz == other.lowCutHz &&
                   highCutHz == other.highCutHz;
        }
        
        bool operator!=(const Params& other) const { return !(*this == other); }
    };

    void prepare (double sampleRate, int samplesPerBlock, int numChannels)
    {
        sRate = sampleRate;
        juce::dsp::ProcessSpec spec { sampleRate,
            static_cast<juce::uint32>(samplesPerBlock),
            static_cast<juce::uint32>(numChannels) };

        delayL.reset (new juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> (maxDelaySamples));
        delayR.reset (new juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> (maxDelaySamples));
        delayL->prepare (spec);
        delayR->prepare (spec);

        lowCutL.prepare(spec);
        lowCutR.prepare(spec);
        highCutL.prepare(spec);
        highCutR.prepare(spec);

        updateFilters();
        reset();
        applyParams();
    }

    void reset()
    {
        if (delayL) delayL->reset();
        if (delayR) delayR->reset();

        lowCutL.reset();
        lowCutR.reset();
        highCutL.reset();
        highCutR.reset();
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        if (bypassed)
            return;

        auto n = buffer.getNumSamples();
        auto* l = buffer.getWritePointer (0);
        auto* r = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

        // Feedback calculation
        const float feedbackGain = juce::jlimit(0.0f, 1.0f, 1.0f - params.stage);
        
        // Combined Output Gain = Ratio (First Repeat) * Mix (Master Wet)
        const float outputGain = params.ratio * params.mix;

        for (int i = 0; i < n; ++i)
        {
            const float inL = l[i];
            const float inR = r ? r[i] : inL;

            // 1. Read from Delay Line
            const float dl = delayL->popSample (0, delaySamples);
            const float dr = delayR->popSample (0, delaySamples);

            // 2. Calculate Feedback Signal (for the NEXT repeat)
            float fbL = dl * feedbackGain;
            float fbR = dr * feedbackGain;

            // 3. Apply Filters to Feedback
            fbL = lowCutL.processSample(fbL);
            fbL = highCutL.processSample(fbL);
            fbR = lowCutR.processSample(fbR);
            fbR = highCutR.processSample(fbR);

            // 4. Apply Stereo Width to Feedback
            const float mid = (fbL + fbR) * 0.5f;
            const float side = (fbL - fbR) * 0.5f * params.stereoWidth;
            fbL = mid + side;
            fbR = mid - side;

            // 5. Write Feedback + Input back into Delay Line
            delayL->pushSample (0, inL + fbL);
            delayR->pushSample (0, inR + fbR);

            // 6. Output Mix: Dry + (Delayed * OutputGain)
            l[i] = inL + (dl * outputGain);
            if (r) r[i] = inR + (dr * outputGain);
        }
    }

    void setParams(const Params& p)
    {
        params = p;
        applyParams();
        updateFilters();
    }

    Params getParams() const { return params; }

    void setBypassed(bool shouldBypass) { bypassed = shouldBypass; }
    bool isBypassed() const { return bypassed; }

private:
    void applyParams()
    {
        delaySamples = static_cast<int> (std::round (params.delayMs * 0.001 * sRate));
        delaySamples = juce::jlimit (1, maxDelaySamples - 1, delaySamples);
    }

    void updateFilters()
    {
        auto lowCutCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass(sRate, params.lowCutHz, 0.707f);
        lowCutL.coefficients = lowCutCoeffs;
        lowCutR.coefficients = lowCutCoeffs;

        auto highCutCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass(sRate, params.highCutHz, 0.707f);
        highCutL.coefficients = highCutCoeffs;
        highCutR.coefficients = highCutCoeffs;
    }

    static constexpr int maxDelaySamples = 96000 * 4;
    double sRate = 44100.0;
    
    std::unique_ptr<juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear>> delayL, delayR;
    juce::dsp::IIR::Filter<float> lowCutL, lowCutR;
    juce::dsp::IIR::Filter<float> highCutL, highCutR;

    Params params;
    int delaySamples = 44100 / 2;
    bool bypassed = false;
};