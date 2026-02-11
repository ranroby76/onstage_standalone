// ==============================================================================
//  SaturationProcessor.h
//  OnStage - Multimode Saturation (Tape, Tube, Digital)
//
//  Tape: Rich harmonic saturation modeled after vintage tape machines
//  Tube: Based on Culture Vulture - organic, harmonically rich
//  Digital: Bitcrushing and sample rate reduction for lo-fi grit
// ==============================================================================

#pragma once
#include <juce_dsp/juce_dsp.h>

class SaturationProcessor
{
public:
    // ==============================================================================
    // Saturation Modes
    // ==============================================================================
    enum class Mode
    {
        Tape = 0,    // Vintage tape - warmth and glue
        Tube,        // Culture Vulture style - harmonic richness
        Digital      // Bitcrusher - lo-fi grit
    };

    struct Params
    {
        Mode mode = Mode::Tape;
        
        float drive = 0.5f;          // 0-1: Amount of saturation/distortion
        float tone = 0.5f;           // 0-1: Tonal character (mode-dependent)
        float mix = 1.0f;            // 0-1: Dry/wet mix
        float outputDb = 0.0f;       // -12 to +12 dB: Output level compensation
        
        // Tape-specific
        float tapeCompression = 0.5f;  // 0-1: Soft compression amount
        float tapeBias = 0.5f;         // 0-1: High frequency bias
        
        // Tube-specific
        float tubeOddEven = 0.5f;      // 0=even harmonics, 1=odd harmonics
        float tubeBias = 0.5f;         // 0=triode, 1=pentode character
        
        // Digital-specific
        float bitDepth = 16.0f;        // 2-16 bits
        float sampleRateDiv = 1.0f;    // 1-64x sample rate reduction
        
        bool operator==(const Params& other) const
        {
            return mode == other.mode &&
                   drive == other.drive &&
                   tone == other.tone &&
                   mix == other.mix &&
                   outputDb == other.outputDb &&
                   tapeCompression == other.tapeCompression &&
                   tapeBias == other.tapeBias &&
                   tubeOddEven == other.tubeOddEven &&
                   tubeBias == other.tubeBias &&
                   bitDepth == other.bitDepth &&
                   sampleRateDiv == other.sampleRateDiv;
        }
        
        bool operator!=(const Params& other) const { return !(*this == other); }
    };

    SaturationProcessor() = default;

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        
        // Prepare filters for tone shaping
        toneFilterL.prepare(spec);
        toneFilterR.prepare(spec);
        highShelfL.prepare(spec);
        highShelfR.prepare(spec);
        
        // Prepare output gain
        outputGain.prepare(spec);
        outputGain.setRampDurationSeconds(0.02);
        
        // Sample & hold state for digital mode
        sampleHoldCounter = 0;
        lastSampleL = 0.0f;
        lastSampleR = 0.0f;
        
        updateFilters();
    }

    void reset()
    {
        toneFilterL.reset();
        toneFilterR.reset();
        highShelfL.reset();
        highShelfR.reset();
        outputGain.reset();
        sampleHoldCounter = 0;
        lastSampleL = 0.0f;
        lastSampleR = 0.0f;
    }

    void process(juce::AudioBuffer<float>& buffer)
    {
        if (bypassed)
            return;

        const int numSamples = buffer.getNumSamples();
        const int numChannels = juce::jmin(2, buffer.getNumChannels());

        // Store dry signal for mix
        juce::AudioBuffer<float> dryBuffer;
        if (params.mix < 0.999f)
        {
            dryBuffer.setSize(numChannels, numSamples, false, false, true);
            for (int ch = 0; ch < numChannels; ++ch)
                dryBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);
        }

        // Process based on mode
        switch (params.mode)
        {
            case Mode::Tape:    processTape(buffer);    break;
            case Mode::Tube:    processTube(buffer);    break;
            case Mode::Digital: processDigital(buffer); break;
        }

        // Apply output gain
        outputGain.setGainDecibels(params.outputDb);
        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> context(block);
        outputGain.process(context);

        // Mix dry/wet
        if (params.mix < 0.999f)
        {
            float wet = params.mix;
            float dry = 1.0f - wet;
            
            for (int ch = 0; ch < numChannels; ++ch)
            {
                float* out = buffer.getWritePointer(ch);
                const float* dryData = dryBuffer.getReadPointer(ch);
                
                for (int i = 0; i < numSamples; ++i)
                    out[i] = out[i] * wet + dryData[i] * dry;
            }
        }
    }

    void setParams(const Params& p)
    {
        bool modeChanged = (params.mode != p.mode);
        params = p;
        updateFilters();
        
        if (modeChanged)
            reset();
    }

    Params getParams() const { return params; }

    void setBypassed(bool shouldBypass) { bypassed = shouldBypass; }
    bool isBypassed() const { return bypassed; }

private:
    // ==============================================================================
    // TAPE MODE - Vintage tape machine saturation
    // Warm, smooth analog feel with soft compression and harmonic glue
    // ==============================================================================
    void processTape(juce::AudioBuffer<float>& buffer)
    {
        const int numSamples = buffer.getNumSamples();
        const int numChannels = juce::jmin(2, buffer.getNumChannels());
        
        // Drive amount (exponential scaling for natural feel)
        float driveGain = 1.0f + params.drive * 8.0f;  // 1x to 9x gain into saturation
        float compression = params.tapeCompression;
        float bias = params.tapeBias;
        
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* data = buffer.getWritePointer(ch);
            auto& toneFilter = (ch == 0) ? toneFilterL : toneFilterR;
            auto& highShelf = (ch == 0) ? highShelfL : highShelfR;
            
            for (int i = 0; i < numSamples; ++i)
            {
                float x = data[i] * driveGain;
                
                // Tape hysteresis simulation (soft asymmetric clipping)
                // Even harmonics from asymmetry
                float asymmetry = 0.1f + bias * 0.2f;
                if (x > 0.0f)
                    x = std::tanh(x * (1.0f + asymmetry));
                else
                    x = std::tanh(x * (1.0f - asymmetry));
                
                // Tape compression (soft knee)
                if (compression > 0.01f)
                {
                    float absX = std::abs(x);
                    float compGain = 1.0f / (1.0f + compression * absX * 2.0f);
                    x *= compGain;
                }
                
                // High frequency loss (tape head rolloff)
                x = highShelf.processSample(x);
                
                // Apply tone filter
                x = toneFilter.processSample(x);
                
                // Normalize output
                data[i] = x * 0.7f;
            }
        }
    }

    // ==============================================================================
    // TUBE MODE - Culture Vulture style saturation
    // Organic, harmonically rich saturation from gentle to extreme
    // ==============================================================================
    void processTube(juce::AudioBuffer<float>& buffer)
    {
        const int numSamples = buffer.getNumSamples();
        const int numChannels = juce::jmin(2, buffer.getNumChannels());
        
        float driveGain = 1.0f + params.drive * 15.0f;  // More gain range for tube
        float oddEven = params.tubeOddEven;
        float bias = params.tubeBias;
        
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* data = buffer.getWritePointer(ch);
            auto& toneFilter = (ch == 0) ? toneFilterL : toneFilterR;
            
            for (int i = 0; i < numSamples; ++i)
            {
                float x = data[i] * driveGain;
                
                // Tube saturation with variable harmonics
                float saturated;
                
                if (bias < 0.5f)
                {
                    // Triode character - softer, rounder
                    // More even harmonics (warm)
                    float triodeFactor = 1.0f - bias * 2.0f;
                    
                    // Even harmonic generation (asymmetric waveshaping)
                    float even = x + 0.25f * x * x - 0.1f * x * x * x;
                    
                    // Odd harmonic generation (symmetric waveshaping)
                    float odd = std::tanh(x * 1.5f);
                    
                    // Blend based on oddEven parameter
                    saturated = even * (1.0f - oddEven) * triodeFactor + 
                                odd * oddEven +
                                std::tanh(x) * (1.0f - triodeFactor);
                }
                else
                {
                    // Pentode character - harder, more aggressive
                    // More odd harmonics (edgy)
                    float pentodeFactor = (bias - 0.5f) * 2.0f;
                    
                    // Hard clipping component
                    float hard = juce::jlimit(-1.0f, 1.0f, x * 1.2f);
                    
                    // Soft saturation
                    float soft = std::tanh(x * 2.0f) * 0.8f;
                    
                    // Crossover distortion simulation
                    float crossover = 0.0f;
                    if (std::abs(x) < 0.1f)
                        crossover = x * 3.0f * pentodeFactor;
                    
                    saturated = soft * (1.0f - pentodeFactor * 0.5f) + 
                                hard * pentodeFactor * 0.5f +
                                crossover;
                }
                
                // Tube warmth (second-order harmonics)
                float warmth = saturated * saturated * 0.15f * (1.0f - oddEven);
                saturated += warmth;
                
                // Apply tone filter
                saturated = toneFilter.processSample(saturated);
                
                // Output with slight compression
                data[i] = std::tanh(saturated * 0.9f) * 0.75f;
            }
        }
    }

    // ==============================================================================
    // DIGITAL MODE - Bitcrusher / Sample Rate Reduction
    // Lo-fi grit and modern textures
    // ==============================================================================
    void processDigital(juce::AudioBuffer<float>& buffer)
    {
        const int numSamples = buffer.getNumSamples();
        const int numChannels = juce::jmin(2, buffer.getNumChannels());
        
        // Bit depth quantization
        int bits = juce::jlimit(2, 16, (int)params.bitDepth);
        float quantLevels = std::pow(2.0f, (float)bits);
        float quantStep = 2.0f / quantLevels;
        
        // Sample rate reduction
        int sampleHoldRate = juce::jlimit(1, 64, (int)params.sampleRateDiv);
        
        // Drive adds pre-gain and creates more aliasing
        float driveGain = 1.0f + params.drive * 4.0f;
        
        for (int i = 0; i < numSamples; ++i)
        {
            // Sample & hold
            sampleHoldCounter++;
            
            if (sampleHoldCounter >= sampleHoldRate)
            {
                sampleHoldCounter = 0;
                
                for (int ch = 0; ch < numChannels; ++ch)
                {
                    float x = buffer.getSample(ch, i) * driveGain;
                    
                    // Pre-clip
                    x = juce::jlimit(-1.0f, 1.0f, x);
                    
                    // Bit reduction (quantization)
                    x = std::floor(x / quantStep + 0.5f) * quantStep;
                    
                    // Store for sample & hold
                    if (ch == 0)
                        lastSampleL = x;
                    else
                        lastSampleR = x;
                }
            }
            
            // Apply held samples
            buffer.setSample(0, i, lastSampleL);
            if (numChannels > 1)
                buffer.setSample(1, i, lastSampleR);
        }
        
        // Apply tone filter (acts as anti-aliasing or adds more grit)
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* data = buffer.getWritePointer(ch);
            auto& toneFilter = (ch == 0) ? toneFilterL : toneFilterR;
            
            for (int i = 0; i < numSamples; ++i)
                data[i] = toneFilter.processSample(data[i]);
        }
    }

    // ==============================================================================
    // Filter Updates
    // ==============================================================================
    void updateFilters()
    {
        if (sampleRate <= 0.0)
            return;

        // Tone filter varies by mode
        float toneFreq;
        float toneQ = 0.707f;
        
        switch (params.mode)
        {
            case Mode::Tape:
                // Low-pass that sweeps from 2kHz to 15kHz
                toneFreq = 2000.0f + params.tone * 13000.0f;
                *toneFilterL.coefficients = *juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, toneFreq, toneQ);
                *toneFilterR.coefficients = *toneFilterL.coefficients;
                
                // High shelf for tape head rolloff
                {
                    float shelfGain = -3.0f - (1.0f - params.tapeBias) * 6.0f;  // -3 to -9 dB
                    *highShelfL.coefficients = *juce::dsp::IIR::Coefficients<float>::makeHighShelf(sampleRate, 8000.0f, 0.707f, 
                        juce::Decibels::decibelsToGain(shelfGain));
                    *highShelfR.coefficients = *highShelfL.coefficients;
                }
                break;
                
            case Mode::Tube:
                // Presence peak that sweeps from 1kHz to 8kHz
                toneFreq = 1000.0f + params.tone * 7000.0f;
                toneQ = 1.0f + params.tone;  // Narrower Q at higher frequencies
                *toneFilterL.coefficients = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, toneFreq, toneQ, 
                    juce::Decibels::decibelsToGain(3.0f));
                *toneFilterR.coefficients = *toneFilterL.coefficients;
                break;
                
            case Mode::Digital:
                // Low-pass that can act as anti-aliasing or be wide open
                // At tone=0: 1kHz (dark, lo-fi), at tone=1: 20kHz (bright, aliased)
                toneFreq = 1000.0f + params.tone * 19000.0f;
                *toneFilterL.coefficients = *juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, toneFreq, 0.5f);
                *toneFilterR.coefficients = *toneFilterL.coefficients;
                break;
        }
    }

    Params params;
    bool bypassed = false;
    double sampleRate = 44100.0;

    // Filters
    juce::dsp::IIR::Filter<float> toneFilterL, toneFilterR;
    juce::dsp::IIR::Filter<float> highShelfL, highShelfR;
    
    // Output gain
    juce::dsp::Gain<float> outputGain;
    
    // Digital mode state
    int sampleHoldCounter = 0;
    float lastSampleL = 0.0f;
    float lastSampleR = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SaturationProcessor)
};
