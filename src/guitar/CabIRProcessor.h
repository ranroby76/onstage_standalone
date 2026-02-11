// ==============================================================================
//  CabIRProcessor.h
//  OnStage - Guitar Cabinet Impulse Response (Convolution)
//
//  Loads a .wav IR file and applies convolution for realistic cabinet/mic
//  emulation. Uses juce::dsp::Convolution which handles background loading
//  and crossfading between IRs automatically.
//
//  Parameters:
//  - IR File: Path to .wav impulse response file
//  - Mix: Dry/wet blend (0..1)
//  - Level: Output gain (0..2)
//  - HighCut: Post-convolution LP filter to tame fizz (1k..20k Hz)
//  - LowCut: Post-convolution HP filter to tighten low end (20..500 Hz)
// ==============================================================================

#pragma once
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_formats/juce_audio_formats.h>

class CabIRProcessor
{
public:
    struct Params
    {
        float mix       = 1.0f;      // 0..1
        float level     = 1.0f;      // 0..2
        float highCutHz = 12000.0f;  // 1000..20000
        float lowCutHz  = 80.0f;     // 20..500

        bool operator== (const Params& o) const
        {
            return mix == o.mix && level == o.level
                && highCutHz == o.highCutHz && lowCutHz == o.lowCutHz;
        }
        bool operator!= (const Params& o) const { return !(*this == o); }
    };

    CabIRProcessor() = default;

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        numChannels = (int) spec.numChannels;
        maxBlockSize = (int) spec.maximumBlockSize;

        convolution.prepare (spec);

        for (int ch = 0; ch < 2; ++ch)
        {
            highCutFilter[ch].prepare (spec);
            lowCutFilter[ch].prepare (spec);
        }

        // Pre-allocate dry buffer for mix blending
        dryBuffer.setSize (2, maxBlockSize);
        dryBuffer.clear();

        applyParams();
        isPrepared = true;
    }

    void reset()
    {
        convolution.reset();
        for (int ch = 0; ch < 2; ++ch)
        {
            highCutFilter[ch].reset();
            lowCutFilter[ch].reset();
        }
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        if (bypassed || !isPrepared) return;

        const int numSamples = buffer.getNumSamples();
        const int channels   = juce::jmin (2, buffer.getNumChannels());
        const float wet = params.mix;
        const float dry = 1.0f - params.mix;

        // Store dry signal for mix blending
        for (int ch = 0; ch < channels; ++ch)
            dryBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);

        // Apply convolution
        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> context (block);
        convolution.process (context);

        // Post-convolution filters + mix
        for (int ch = 0; ch < channels; ++ch)
        {
            float* wetData = buffer.getWritePointer (ch);
            const float* dryData = dryBuffer.getReadPointer (ch);

            for (int i = 0; i < numSamples; ++i)
            {
                float w = wetData[i];
                w = highCutFilter[ch].processSample (w);
                w = lowCutFilter[ch].processSample (w);
                wetData[i] = (w * wet + dryData[i] * dry) * params.level;
            }
        }
    }

    // --- IR loading ---

    void loadIRFromFile (const juce::File& file)
    {
        if (! file.existsAsFile()) return;

        currentIRFile = file;
        currentIRName = file.getFileNameWithoutExtension();

        convolution.loadImpulseResponse (
            file,
            juce::dsp::Convolution::Stereo::yes,
            juce::dsp::Convolution::Trim::yes,
            0,    // use full IR length
            juce::dsp::Convolution::Normalise::yes
        );
    }

    void loadIRFromMemory (const void* data, size_t dataSize)
    {
        currentIRFile = juce::File();
        currentIRName = "Built-in IR";

        convolution.loadImpulseResponse (
            data, dataSize,
            juce::dsp::Convolution::Stereo::yes,
            juce::dsp::Convolution::Trim::yes,
            0,
            juce::dsp::Convolution::Normalise::yes
        );
    }

    bool hasIRLoaded() const { return currentIRName.isNotEmpty(); }
    juce::String getIRName() const { return currentIRName; }
    juce::File getIRFile() const { return currentIRFile; }

    // --- Params ---

    void setParams (const Params& p)
    {
        params = p;
        if (isPrepared) applyParams();
    }

    Params getParams() const { return params; }
    void setBypassed (bool b) { bypassed = b; }
    bool isBypassed() const { return bypassed; }

private:
    void applyParams()
    {
        if (sampleRate <= 0.0) return;

        auto hcCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass (
            sampleRate,
            juce::jlimit (1000.0f, 20000.0f, params.highCutHz),
            0.707f);

        auto lcCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass (
            sampleRate,
            juce::jlimit (20.0f, 500.0f, params.lowCutHz),
            0.707f);

        for (int ch = 0; ch < 2; ++ch)
        {
            highCutFilter[ch].coefficients = hcCoeffs;
            lowCutFilter[ch].coefficients = lcCoeffs;
        }
    }

    Params params;
    double sampleRate = 44100.0;
    int numChannels = 2;
    int maxBlockSize = 512;
    bool bypassed = false;
    bool isPrepared = false;

    juce::dsp::Convolution convolution;
    juce::dsp::IIR::Filter<float> highCutFilter[2];
    juce::dsp::IIR::Filter<float> lowCutFilter[2];

    juce::AudioBuffer<float> dryBuffer;

    juce::File currentIRFile;
    juce::String currentIRName;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CabIRProcessor)
};