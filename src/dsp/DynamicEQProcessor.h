#pragma once
#include <juce_dsp/juce_dsp.h>

class DynamicEQProcessor
{
public:
    struct BandParams
    {
        float duckBandHz = 1000.0f;
        float q = 2.0f;
        float shape = 0.5f;
        float threshold = -30.0f;
        float ratio = 4.0f;
        float attack = 10.0f;
        float release = 150.0f;
    };

    DynamicEQProcessor() = default;

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        numChannels = spec.numChannels;

        // Prepare Filters for Analysis (Sidechain) - 2 Bands x Stereo
        for (int b = 0; b < 2; ++b)
        {
            for (int ch = 0; ch < 2; ++ch)
            {
                duckFilters[b][ch].prepare(spec);
                duckFilters[b][ch].reset();
                
                // Prepare Filters for Processing (Backing Track) - 2 Bands x Stereo
                processFilters[b][ch].prepare(spec);
                processFilters[b][ch].reset();
            }
        }
        
        updateFilterCoefficients();
        
        for (int b = 0; b < 2; ++b)
        {
            envelopeLevels[b] = 0.0f;
            rmsHistoryIndex[b] = 0;
            for (int i = 0; i < RMS_WINDOW_SIZE; ++i)
                rmsHistory[b][i] = 0.0f;
            updateEnvelopeCoefficients(b);
            currentGainReductionDbs[b] = 0.0f;
        }
    }

    void reset()
    {
        for (int b = 0; b < 2; ++b)
        {
            for (int ch = 0; ch < 2; ++ch)
            {
                duckFilters[b][ch].reset();
                processFilters[b][ch].reset();
            }
            envelopeLevels[b] = 0.0f;
            rmsHistoryIndex[b] = 0;
            for (int i = 0; i < RMS_WINDOW_SIZE; ++i)
                rmsHistory[b][i] = 0.0f;
            currentGainReductionDbs[b] = 0.0f;
        }
    }

    void process(juce::AudioBuffer<float>& backingTracks, 
                 const juce::AudioBuffer<float>& vocalSidechain)
    {
        if (bypassed)
        {
            currentGainReductionDbs[0] = 0.0f;
            currentGainReductionDbs[1] = 0.0f;
            return;
        }

        const int numSamples = backingTracks.getNumSamples();
        const int numChannels = juce::jmin(2, backingTracks.getNumChannels());

        // Temporary buffers for spectral processing (Parallel)
        juce::AudioBuffer<float> wetBuffer(numChannels, numSamples);
        wetBuffer.clear();

        for (int bandIdx = 0; bandIdx < 2; ++bandIdx)
        {
            // ================================================================
            // 1. ENHANCED RMS-BASED VOCAL ENERGY DETECTION
            // ================================================================
            float vocalDb = 0.0f;
            {
                // Calculate RMS energy with moving average for smooth detection
                float sumSquares = 0.0f;
                for (int ch = 0; ch < vocalSidechain.getNumChannels(); ++ch)
                {
                    const float* data = vocalSidechain.getReadPointer(ch);
                    for (int i = 0; i < numSamples; ++i)
                        sumSquares += data[i] * data[i];
                }
                
                // RMS calculation
                float currentRms = std::sqrt(sumSquares / (numSamples * vocalSidechain.getNumChannels()));
                
                // Moving average for smooth RMS (no latency - just smoothing)
                rmsHistory[bandIdx][rmsHistoryIndex[bandIdx]] = currentRms;
                rmsHistoryIndex[bandIdx] = (rmsHistoryIndex[bandIdx] + 1) % RMS_WINDOW_SIZE;
                
                float avgRms = 0.0f;
                for (int i = 0; i < RMS_WINDOW_SIZE; ++i)
                    avgRms += rmsHistory[bandIdx][i];
                avgRms /= RMS_WINDOW_SIZE;
                
                vocalDb = juce::Decibels::gainToDecibels(avgRms + 1e-6f);
            }

            // ================================================================
            // 2. MUSICAL DUCKING CURVE (Smooth & Natural)
            // ================================================================
            float gainReductionDb = 0.0f;
            if (vocalDb > bandParams[bandIdx].threshold)
            {
                float overThresholdDb = vocalDb - bandParams[bandIdx].threshold;
                
                // Smooth logarithmic compression curve instead of linear
                float compressedDb = overThresholdDb * (1.0f - (1.0f / bandParams[bandIdx].ratio));
                
                // Apply smooth "knee" using tanh for musical transition
                float knee = 3.0f; // Soft knee width in dB
                float normalizedOver = overThresholdDb / knee;
                float smoothFactor = (std::tanh(normalizedOver) + 1.0f) * 0.5f; // 0 to 1
                
                gainReductionDb = compressedDb * smoothFactor;
            }
            
            currentGainReductionDbs[bandIdx] = gainReductionDb;
            float targetGainReduction = juce::Decibels::decibelsToGain(-gainReductionDb);
            
            // Smooth Envelope
            float* envPtr = &envelopeLevels[bandIdx];
            for (int i = 0; i < numSamples; ++i)
            {
                if (targetGainReduction < *envPtr)
                    *envPtr += (targetGainReduction - *envPtr) * attackCoeffs[bandIdx];
                else
                    *envPtr += (targetGainReduction - *envPtr) * releaseCoeffs[bandIdx];
            }

            // 3. Process Backing Track for this band (Spectral Subtraction)
            juce::AudioBuffer<float> bandBuffer(numChannels, numSamples);
            
            for (int ch = 0; ch < numChannels; ++ch)
            {
                // Filter the backing track
                float* writePtr = bandBuffer.getWritePointer(ch);
                const float* readPtr = backingTracks.getReadPointer(ch);
                
                juce::FloatVectorOperations::copy(writePtr, readPtr, numSamples);
                
                // FIX: Use writable pointer for AudioBuffer view
                juce::AudioBuffer<float> channelView(&writePtr, 1, numSamples);
                juce::dsp::AudioBlock<float> block(channelView);
                juce::dsp::ProcessContextReplacing<float> context(block);
                processFilters[bandIdx][ch].process(context);
                
                // Apply Envelope (Ducking) to the isolated band
                for (int i = 0; i < numSamples; ++i)
                {
                    float attenuation = (1.0f - envelopeLevels[bandIdx]) * calculateFrequencyGain(bandParams[bandIdx].shape);
                    writePtr[i] *= attenuation; 
                }
                
                wetBuffer.addFrom(ch, 0, writePtr, numSamples);
            }
        }

        // ================================================================
        // 4. STEREO-AWARE DUCKING (Mid/Side Processing)
        // ================================================================
        const float* wetL = wetBuffer.getReadPointer(0);
        const float* wetR = numChannels > 1 ? wetBuffer.getReadPointer(1) : wetL;
        
        float* dryL = backingTracks.getWritePointer(0);
        float* dryR = numChannels > 1 ? backingTracks.getWritePointer(1) : dryL;
        
        for (int i = 0; i < numSamples; ++i)
        {
            // Get original backing track
            float origL = dryL[i];
            float origR = numChannels > 1 ? dryR[i] : origL;
            
            // Get ducked frequencies
            float duckL = wetL[i];
            float duckR = numChannels > 1 ? wetR[i] : duckL;
            
            // Convert to Mid/Side for smart ducking
            float origMid = (origL + origR) * 0.5f;
            float origSide = (origL - origR) * 0.5f;
            
            float duckMid = (duckL + duckR) * 0.5f;
            float duckSide = (duckL - duckR) * 0.5f;
            
            // Duck SIDES more aggressively (vocals are mostly center)
            // This creates space without losing fullness
            float midDuck = duckMid * 0.7f;     // 70% ducking on center
            float sideDuck = duckSide * 1.5f;   // 150% ducking on sides
            
            // Subtract ducked energy
            float resultMid = origMid - midDuck;
            float resultSide = origSide - sideDuck;
            
            // Convert back to L/R
            float resultL = resultMid + resultSide;
            float resultR = resultMid - resultSide;
            
            dryL[i] = resultL;
            if (numChannels > 1)
                dryR[i] = resultR;
        }
    }

    void setParams(int bandIndex, const BandParams& newParams)
    {
        if (bandIndex >= 0 && bandIndex < 2)
        {
            bandParams[bandIndex] = newParams;
            updateFilterCoefficients();
            updateEnvelopeCoefficients(bandIndex);
        }
    }

    BandParams getParams(int bandIndex) const 
    { 
        if (bandIndex >= 0 && bandIndex < 2) return bandParams[bandIndex];
        return BandParams(); 
    }
    
    void setBypassed(bool shouldBypass) { bypassed = shouldBypass; }
    bool isBypassed() const { return bypassed; }
    
    float getCurrentGainReductionDb(int bandIndex) const 
    { 
        if (bandIndex >= 0 && bandIndex < 2) return currentGainReductionDbs[bandIndex];
        return 0.0f;
    }

private:
    void updateFilterCoefficients()
    {
        if (sampleRate <= 0.0) return;

        for (int b = 0; b < 2; ++b)
        {
            // Bell Filter (Peaking)
            auto coeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
                sampleRate, 
                bandParams[b].duckBandHz, 
                bandParams[b].q, 
                1.0f
            );

            for (int ch = 0; ch < 2; ++ch)
            {
                duckFilters[b][ch].coefficients = coeffs;
                processFilters[b][ch].coefficients = coeffs;
            }
        }
    }

    void updateEnvelopeCoefficients(int bandIndex)
    {
        if (sampleRate <= 0.0) return;

        float attackSec = bandParams[bandIndex].attack * 0.001f;
        float releaseSec = bandParams[bandIndex].release * 0.001f;

        attackCoeffs[bandIndex] = 1.0f - std::exp(-1.0f / (attackSec * sampleRate));
        releaseCoeffs[bandIndex] = 1.0f - std::exp(-1.0f / (releaseSec * sampleRate));
    }

    float calculateFrequencyGain(float shape) const
    {
        // Shape: 0.0 = Gentle, 1.0 = Aggressive
        return juce::jmap(shape, 0.0f, 1.0f, 0.3f, 1.0f);
    }

    BandParams bandParams[2];
    bool bypassed = false;
    double sampleRate = 44100.0;
    int numChannels = 2;

    // 2 Bands x Stereo Filters
    juce::dsp::IIR::Filter<float> duckFilters[2][2];
    juce::dsp::IIR::Filter<float> processFilters[2][2];

    float envelopeLevels[2] = {0.0f, 0.0f};
    float attackCoeffs[2] = {0.0f, 0.0f};
    float releaseCoeffs[2] = {0.0f, 0.0f};
    
    float currentGainReductionDbs[2] = {0.0f, 0.0f};
    
    // RMS smoothing (no latency - just averaging recent history)
    static constexpr int RMS_WINDOW_SIZE = 4; // 4 blocks ~= 20-40ms smooth at typical buffer sizes
    float rmsHistory[2][RMS_WINDOW_SIZE];
    int rmsHistoryIndex[2] = {0, 0};
};