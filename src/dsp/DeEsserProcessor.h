// ==============================================================================
//  DeEsserProcessor.h
//  OnStage - De-Esser for reducing sibilance (s, z, sh sounds)
//
//  Features:
//  - Frequency-selective compression targeting sibilant range (2-16 kHz)
//  - Wideband mode: Reduces entire signal when sibilance detected
//  - Split-band mode: Only reduces the sibilant frequencies
//  - Fast attack for catching transients, smooth release
//  - Listen mode for monitoring what's being reduced
// ==============================================================================

#pragma once
#include <juce_dsp/juce_dsp.h>

class DeEsserProcessor
{
public:
    // ==============================================================================
    // De-Esser Modes
    // ==============================================================================
    enum class Mode
    {
        Wideband = 0,   // Reduces entire signal (more transparent)
        SplitBand       // Only reduces sibilant frequencies (more precise)
    };

    struct Params
    {
        Mode mode = Mode::SplitBand;
        
        float frequency = 7000.0f;      // Center frequency for sibilance detection (2000-16000 Hz)
        float bandwidth = 1.5f;         // Q factor for detection band (0.5 to 4.0)
        float threshold = -20.0f;       // Threshold in dB (-60 to 0)
        float reduction = 6.0f;         // Max reduction in dB (0 to 20)
        float attack = 0.5f;            // Attack time in ms (0.1 to 10)
        float release = 50.0f;          // Release time in ms (10 to 200)
        float range = 1.0f;             // Frequency range multiplier (0.5 to 2.0) - widens detection
        bool listenMode = false;        // When true, outputs only the sibilant frequencies
        
        bool operator==(const Params& other) const
        {
            return mode == other.mode &&
                   frequency == other.frequency &&
                   bandwidth == other.bandwidth &&
                   threshold == other.threshold &&
                   reduction == other.reduction &&
                   attack == other.attack &&
                   release == other.release &&
                   range == other.range &&
                   listenMode == other.listenMode;
        }
        
        bool operator!=(const Params& other) const { return !(*this == other); }
    };

    DeEsserProcessor() = default;

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        
        // Detection filters (bandpass for sibilance detection)
        detectionFilterL.prepare(spec);
        detectionFilterR.prepare(spec);
        
        // Processing filters for split-band mode
        // High-pass to isolate sibilant frequencies
        splitHighL.prepare(spec);
        splitHighR.prepare(spec);
        // Low-pass for the non-sibilant part
        splitLowL.prepare(spec);
        splitLowR.prepare(spec);
        
        // Smoothing filter for gain reduction
        smoothingFilterL.prepare(spec);
        smoothingFilterR.prepare(spec);
        
        updateFilters();
        reset();
    }

    void reset()
    {
        detectionFilterL.reset();
        detectionFilterR.reset();
        splitHighL.reset();
        splitHighR.reset();
        splitLowL.reset();
        splitLowR.reset();
        smoothingFilterL.reset();
        smoothingFilterR.reset();
        
        envelopeL = 0.0f;
        envelopeR = 0.0f;
        currentGainReduction = 0.0f;
    }

    void process(juce::AudioBuffer<float>& buffer)
    {
        if (bypassed)
            return;

        const int numSamples = buffer.getNumSamples();
        const int numChannels = juce::jmin(2, buffer.getNumChannels());

        // Calculate envelope coefficients
        float attackCoeff = std::exp(-1.0f / (params.attack * 0.001f * sampleRate));
        float releaseCoeff = std::exp(-1.0f / (params.release * 0.001f * sampleRate));
        
        // Threshold in linear
        float thresholdLin = juce::Decibels::decibelsToGain(params.threshold);
        
        // Max reduction in linear (inverted - this is the minimum gain we'll apply)
        float maxReductionLin = juce::Decibels::decibelsToGain(-params.reduction);
        
        float peakGainReduction = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            float inL = buffer.getSample(0, i);
            float inR = (numChannels > 1) ? buffer.getSample(1, i) : inL;
            
            // --- Detection Stage ---
            // Filter to isolate sibilant frequencies for detection
            float detL = detectionFilterL.processSample(inL);
            float detR = detectionFilterR.processSample(inR);
            
            // Envelope follower on the filtered signal
            float detLevelL = std::abs(detL);
            float detLevelR = std::abs(detR);
            
            // Attack/release envelope
            if (detLevelL > envelopeL)
                envelopeL = attackCoeff * envelopeL + (1.0f - attackCoeff) * detLevelL;
            else
                envelopeL = releaseCoeff * envelopeL + (1.0f - releaseCoeff) * detLevelL;
            
            if (detLevelR > envelopeR)
                envelopeR = attackCoeff * envelopeR + (1.0f - attackCoeff) * detLevelR;
            else
                envelopeR = releaseCoeff * envelopeR + (1.0f - releaseCoeff) * detLevelR;
            
            // --- Gain Calculation ---
            // Calculate gain reduction based on how much envelope exceeds threshold
            float gainL = 1.0f;
            float gainR = 1.0f;
            
            if (envelopeL > thresholdLin)
            {
                // Soft-knee compression above threshold
                float overDb = juce::Decibels::gainToDecibels(envelopeL / thresholdLin);
                float reductionDb = juce::jmin(overDb * 0.8f, params.reduction);  // 0.8 = ratio-like behavior
                gainL = juce::Decibels::decibelsToGain(-reductionDb);
                gainL = juce::jmax(gainL, maxReductionLin);
            }
            
            if (envelopeR > thresholdLin)
            {
                float overDb = juce::Decibels::gainToDecibels(envelopeR / thresholdLin);
                float reductionDb = juce::jmin(overDb * 0.8f, params.reduction);
                gainR = juce::Decibels::decibelsToGain(-reductionDb);
                gainR = juce::jmax(gainR, maxReductionLin);
            }
            
            // Track peak gain reduction for metering
            float currentReduction = juce::jmin(gainL, gainR);
            if (currentReduction < (1.0f - peakGainReduction))
                peakGainReduction = 1.0f - currentReduction;
            
            // --- Output Stage ---
            float outL, outR;
            
            if (params.listenMode)
            {
                // Listen mode: output only the sibilant frequencies
                outL = detL;
                outR = detR;
            }
            else if (params.mode == Mode::Wideband)
            {
                // Wideband: apply gain reduction to entire signal
                outL = inL * gainL;
                outR = inR * gainR;
            }
            else  // SplitBand
            {
                // Split-band: only reduce the high frequencies
                float highL = splitHighL.processSample(inL);
                float highR = splitHighR.processSample(inR);
                float lowL = splitLowL.processSample(inL);
                float lowR = splitLowR.processSample(inR);
                
                // Apply gain reduction only to high band
                outL = lowL + (highL * gainL);
                outR = lowR + (highR * gainR);
            }
            
            buffer.setSample(0, i, outL);
            if (numChannels > 1)
                buffer.setSample(1, i, outR);
        }
        
        // Smooth the gain reduction for display
        currentGainReduction = currentGainReduction * 0.9f + peakGainReduction * 0.1f;
    }

    void setParams(const Params& p)
    {
        bool needsFilterUpdate = (params.frequency != p.frequency ||
                                   params.bandwidth != p.bandwidth ||
                                   params.range != p.range);
        params = p;
        
        if (needsFilterUpdate)
            updateFilters();
    }

    Params getParams() const { return params; }

    void setBypassed(bool shouldBypass) { bypassed = shouldBypass; }
    bool isBypassed() const { return bypassed; }

    // For metering
    float getCurrentGainReductionDb() const 
    { 
        return juce::Decibels::gainToDecibels(1.0f - currentGainReduction); 
    }
    
    float getEnvelopeLevel() const
    {
        return juce::jmax(envelopeL, envelopeR);
    }

private:
    void updateFilters()
    {
        if (sampleRate <= 0.0)
            return;

        float freq = juce::jlimit(2000.0f, 16000.0f, params.frequency);
        float q = juce::jlimit(0.5f, 4.0f, params.bandwidth);
        float range = juce::jlimit(0.5f, 2.0f, params.range);
        
        // Detection filter: bandpass centered on sibilance frequency
        // Wider Q for detection to catch the full sibilant range
        float detectionQ = q / range;  // Wider detection when range is higher
        auto detCoeffs = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, freq, detectionQ);
        *detectionFilterL.coefficients = *detCoeffs;
        *detectionFilterR.coefficients = *detCoeffs;
        
        // Split-band crossover frequency (slightly below the sibilance center)
        float crossoverFreq = freq * 0.7f;
        crossoverFreq = juce::jlimit(1500.0f, 12000.0f, crossoverFreq);
        
        // High-pass for sibilant band
        auto highCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, crossoverFreq, 0.707f);
        *splitHighL.coefficients = *highCoeffs;
        *splitHighR.coefficients = *highCoeffs;
        
        // Low-pass for non-sibilant band (Linkwitz-Riley style crossover)
        auto lowCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, crossoverFreq, 0.707f);
        *splitLowL.coefficients = *lowCoeffs;
        *splitLowR.coefficients = *lowCoeffs;
    }

    Params params;
    bool bypassed = false;
    double sampleRate = 44100.0;

    // Detection filters (bandpass to isolate sibilance)
    juce::dsp::IIR::Filter<float> detectionFilterL, detectionFilterR;
    
    // Split-band filters
    juce::dsp::IIR::Filter<float> splitHighL, splitHighR;
    juce::dsp::IIR::Filter<float> splitLowL, splitLowR;
    
    // Smoothing filters for gain
    juce::dsp::IIR::Filter<float> smoothingFilterL, smoothingFilterR;
    
    // Envelope followers
    float envelopeL = 0.0f;
    float envelopeR = 0.0f;
    
    // For metering
    float currentGainReduction = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeEsserProcessor)
};
