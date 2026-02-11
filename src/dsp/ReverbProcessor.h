// ==============================================================================
//  ReverbProcessor.h
//  OnStage - Reverb with 4 modes: Hall, Plate, Ambiance, IR (Convolution)
//
//  Hall:     Large cinematic space, smooth and expansive (FDN)
//  Plate:    Bright and airy, quick buildup with presence (allpass network)
//  Ambiance: Intimate and natural, subtle early reflections
//  IR:       Convolution reverb with embedded/external IR + duck & gate
//
//  All modes include duck feature for vocal clarity
// ==============================================================================

#pragma once
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <random>
#include "BinaryData.h"

class ReverbProcessor
{
public:
    // ==============================================================================
    // Reverb Types
    // ==============================================================================
    enum class Type
    {
        Hall = 0,
        Plate,
        Ambiance,
        IR
    };

    // ==============================================================================
    // Parameters
    // ==============================================================================
    struct Params
    {
        Type type = Type::Hall;
        
        // Common parameters (all modes)
        float mix = 0.35f;              // Dry/wet mix (0-1)
        float preDelay = 20.0f;         // Pre-delay in ms (0-100)  [algo only]
        float decay = 2.5f;             // Decay time in seconds    [algo only]
        float lowCut = 80.0f;           // High-pass filter Hz (20-500)
        float highCut = 12000.0f;       // Low-pass filter Hz (2000-20000)
        float duck = 0.0f;              // Duck amount when input present (0-1)
        
        // Hall-specific
        float hallDiffusion = 0.8f;
        float hallModulation = 0.3f;
        float hallWidth = 1.0f;
        
        // Plate-specific
        float plateDamping = 0.5f;
        float plateBrightness = 0.6f;
        float plateDensity = 0.7f;
        
        // Ambiance-specific
        float ambSize = 0.4f;
        float ambEarlyLate = 0.6f;
        float ambLiveliness = 0.5f;
        
        // IR-specific
        juce::String irFilePath;            // Empty = use embedded IR
        float gateThreshold = -60.0f;       // dB, gate closes below this (default: effectively off)
        float gateSpeed = 0.0f;             // 0 = gate off, 1 = fast fade
        
        bool operator==(const Params& other) const
        {
            return type == other.type && mix == other.mix && preDelay == other.preDelay &&
                   decay == other.decay && lowCut == other.lowCut && highCut == other.highCut &&
                   duck == other.duck && hallDiffusion == other.hallDiffusion &&
                   hallModulation == other.hallModulation && hallWidth == other.hallWidth &&
                   plateDamping == other.plateDamping && plateBrightness == other.plateBrightness &&
                   plateDensity == other.plateDensity && ambSize == other.ambSize &&
                   ambEarlyLate == other.ambEarlyLate && ambLiveliness == other.ambLiveliness &&
                   irFilePath == other.irFilePath && gateThreshold == other.gateThreshold &&
                   gateSpeed == other.gateSpeed;
        }
        bool operator!=(const Params& other) const { return !(*this == other); }
    };

    ReverbProcessor() { initDelayLines(); }

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        
        // Pre-delay line (up to 100ms)
        int preDelaySize = (int)(sampleRate * 0.1) + 1;
        preDelayL.resize(preDelaySize, 0.0f);
        preDelayR.resize(preDelaySize, 0.0f);
        preDelayWritePos = 0;
        
        // Initialize all delay lines
        initDelayLines();
        updateDelayTimes();
        
        // Mono filters for algorithmic input
        juce::dsp::ProcessSpec monoSpec = spec;
        monoSpec.numChannels = 1;
        
        inputLowCut.prepare(monoSpec);
        inputHighCut.prepare(monoSpec);
        
        // Stereo output filters (separate L/R states)
        outputLowCutL.prepare(monoSpec);
        outputHighCutL.prepare(monoSpec);
        outputLowCutR.prepare(monoSpec);
        outputHighCutR.prepare(monoSpec);
        
        // Damping filters for each delay line
        for (auto& f : dampingFilters)
            f.prepare(monoSpec);
        
        // IR convolution
        juce::dsp::ProcessSpec stereoSpec = spec;
        stereoSpec.numChannels = 2;
        convolution.prepare(stereoSpec);
        
        // IR stereo filters
        irLowCutL.prepare(monoSpec);
        irHighCutL.prepare(monoSpec);
        irLowCutR.prepare(monoSpec);
        irHighCutR.prepare(monoSpec);
        
        // IR buffers
        irDryBuffer.setSize(2, (int)spec.maximumBlockSize);
        irWetBuffer.setSize(2, (int)spec.maximumBlockSize);
        
        updateFilters();
        
        // Load IR if in IR mode
        if (params.type == Type::IR)
        {
            if (params.irFilePath.isNotEmpty())
                loadExternalIR(juce::File(params.irFilePath));
            else
                loadEmbeddedIR();
        }
        
        reset();
    }

    void reset()
    {
        std::fill(preDelayL.begin(), preDelayL.end(), 0.0f);
        std::fill(preDelayR.begin(), preDelayR.end(), 0.0f);
        preDelayWritePos = 0;
        
        for (auto& dl : delayLines)
            std::fill(dl.begin(), dl.end(), 0.0f);
        for (auto& pos : delayWritePos)
            pos = 0;
        
        for (auto& ap : allpassLines)
            std::fill(ap.begin(), ap.end(), 0.0f);
        for (auto& pos : allpassWritePos)
            pos = 0;
        
        inputLowCut.reset();
        inputHighCut.reset();
        outputLowCutL.reset();
        outputHighCutL.reset();
        outputLowCutR.reset();
        outputHighCutR.reset();
        
        for (auto& f : dampingFilters)
            f.reset();
        
        convolution.reset();
        irLowCutL.reset();
        irHighCutL.reset();
        irLowCutR.reset();
        irHighCutR.reset();
        
        inputEnvelope = 0.0f;
        duckGain = 1.0f;
        gateGain = 1.0f;
        modPhase = 0.0f;
    }

    void process(juce::AudioBuffer<float>& buffer)
    {
        if (bypassed)
            return;

        if (params.type == Type::IR)
            processIR(buffer);
        else
            processAlgorithmic(buffer);
    }

    void setParams(const Params& newParams)
    {
        bool typeChanged = (params.type != newParams.type);
        bool filterChanged = (params.lowCut != newParams.lowCut || 
                              params.highCut != newParams.highCut ||
                              params.plateDamping != newParams.plateDamping);
        bool irChanged = (params.irFilePath != newParams.irFilePath);
        bool switchedToIR = (newParams.type == Type::IR && params.type != Type::IR);
        
        params = newParams;
        
        if (typeChanged)
            updateDelayTimes();
        if (filterChanged)
            updateFilters();
        
        // Handle IR loading
        if (irChanged || switchedToIR)
        {
            if (params.irFilePath.isNotEmpty())
                loadExternalIR(juce::File(params.irFilePath));
            else
                loadEmbeddedIR();
        }
    }

    Params getParams() const { return params; }
    void setBypassed(bool shouldBypass) { bypassed = shouldBypass; }
    bool isBypassed() const { return bypassed; }
    
    // For metering
    float getCurrentDecayLevel() const { return decayLevel; }
    
    // IR info
    juce::String getCurrentIrName() const { return currentIrName; }
    
    // IR loading
    void loadEmbeddedIR()
    {
        convolution.loadImpulseResponse(BinaryData::ir_wav, BinaryData::ir_wavSize,
            juce::dsp::Convolution::Stereo::yes,
            juce::dsp::Convolution::Trim::no,
            0,
            juce::dsp::Convolution::Normalise::yes);
        currentIrName = "Default (Internal)";
    }

    void loadExternalIR(const juce::File& file)
    {
        if (file.existsAsFile())
        {
            convolution.loadImpulseResponse(file,
                juce::dsp::Convolution::Stereo::yes,
                juce::dsp::Convolution::Trim::no,
                0,
                juce::dsp::Convolution::Normalise::yes);
            currentIrName = file.getFileNameWithoutExtension();
        }
        else
        {
            loadEmbeddedIR();
        }
    }

private:
    // ==============================================================================
    // Algorithmic processing (Hall, Plate, Ambiance)
    // ==============================================================================
    void processAlgorithmic(juce::AudioBuffer<float>& buffer)
    {
        const int numSamples = buffer.getNumSamples();
        const int numChannels = juce::jmin(2, buffer.getNumChannels());
        
        int preDelaySamples = (int)(params.preDelay * 0.001f * sampleRate);
        preDelaySamples = juce::jlimit(0, (int)preDelayL.size() - 1, preDelaySamples);
        
        // Envelope follower for ducking
        float envAttack = std::exp(-1.0f / (0.005f * (float)sampleRate));
        float envRelease = std::exp(-1.0f / (0.15f * (float)sampleRate));
        
        for (int i = 0; i < numSamples; ++i)
        {
            float inL = buffer.getSample(0, i);
            float inR = (numChannels > 1) ? buffer.getSample(1, i) : inL;
            float inMono = (inL + inR) * 0.5f;
            
            // Input envelope for ducking
            float inputLevel = std::abs(inMono);
            if (inputLevel > inputEnvelope)
                inputEnvelope = envAttack * inputEnvelope + (1.0f - envAttack) * inputLevel;
            else
                inputEnvelope = envRelease * inputEnvelope + (1.0f - envRelease) * inputLevel;
            
            // Calculate duck gain
            float targetDuck = 1.0f - (params.duck * juce::jlimit(0.0f, 1.0f, inputEnvelope * 10.0f));
            duckGain = duckGain * 0.99f + targetDuck * 0.01f;
            
            // Filter input
            float filteredMono = inMono;
            filteredMono = inputLowCut.processSample(filteredMono);
            filteredMono = inputHighCut.processSample(filteredMono);
            
            // Write to pre-delay
            preDelayL[preDelayWritePos] = inL;
            preDelayR[preDelayWritePos] = inR;
            
            // Read from pre-delay
            int preDelayReadPos = preDelayWritePos - preDelaySamples;
            if (preDelayReadPos < 0) preDelayReadPos += (int)preDelayL.size();
            float preDelayedL = preDelayL[preDelayReadPos];
            float preDelayedR = preDelayR[preDelayReadPos];
            float preDelayedMono = (preDelayedL + preDelayedR) * 0.5f;
            
            preDelayWritePos = (preDelayWritePos + 1) % (int)preDelayL.size();
            
            // Process based on type
            float wetL = 0.0f, wetR = 0.0f;
            
            switch (params.type)
            {
                case Type::Hall:
                    processHall(preDelayedMono, preDelayedL, preDelayedR, wetL, wetR);
                    break;
                case Type::Plate:
                    processPlate(preDelayedMono, wetL, wetR);
                    break;
                case Type::Ambiance:
                    processAmbiance(preDelayedL, preDelayedR, wetL, wetR);
                    break;
                default:
                    break;
            }
            
            // Apply ducking
            wetL *= duckGain;
            wetR *= duckGain;
            
            // Output filtering — both filters on BOTH channels (FIX: was L=lowcut, R=highcut)
            wetL = outputLowCutL.processSample(wetL);
            wetL = outputHighCutL.processSample(wetL);
            wetR = outputLowCutR.processSample(wetR);
            wetR = outputHighCutR.processSample(wetR);
            
            // Mix dry/wet
            float outL = inL * (1.0f - params.mix) + wetL * params.mix;
            float outR = inR * (1.0f - params.mix) + wetR * params.mix;
            
            buffer.setSample(0, i, outL);
            if (numChannels > 1)
                buffer.setSample(1, i, outR);
        }
    }

    // ==============================================================================
    // IR Convolution processing
    // ==============================================================================
    void processIR(juce::AudioBuffer<float>& buffer)
    {
        const int numSamples = buffer.getNumSamples();
        const int numChannels = juce::jmin(2, buffer.getNumChannels());
        
        // Ensure buffers large enough
        if (irDryBuffer.getNumSamples() < numSamples)
        {
            irDryBuffer.setSize(numChannels, numSamples, true, false, true);
            irWetBuffer.setSize(numChannels, numSamples, true, false, true);
        }
        
        // Copy dry signal
        for (int ch = 0; ch < numChannels; ++ch)
            irDryBuffer.copyFrom(ch, 0, buffer.getReadPointer(ch), numSamples);
        
        // Process convolution reverb
        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> ctx(block);
        convolution.process(ctx);
        
        // Apply filters to wet signal (both filters, both channels)
        for (int i = 0; i < numSamples; ++i)
        {
            float wL = buffer.getSample(0, i);
            float wR = (numChannels > 1) ? buffer.getSample(1, i) : wL;
            
            wL = irLowCutL.processSample(wL);
            wL = irHighCutL.processSample(wL);
            wR = irLowCutR.processSample(wR);
            wR = irHighCutR.processSample(wR);
            
            buffer.setSample(0, i, wL);
            if (numChannels > 1)
                buffer.setSample(1, i, wR);
        }
        
        // Copy filtered wet signal
        for (int ch = 0; ch < numChannels; ++ch)
            irWetBuffer.copyFrom(ch, 0, buffer.getReadPointer(ch), numSamples);
        
        // ======================================================================
        // DUCK AND GATE PROCESSING
        // ======================================================================

        // FIX: Gate is fully bypassed when gateSpeed is effectively zero
        bool gateActive = (params.gateSpeed > 0.01f);

        float gateThreshLinear = gateActive
            ? juce::Decibels::decibelsToGain(params.gateThreshold)
            : 0.0f;

        float gateReleaseTimeMs = gateActive
            ? juce::jmap(params.gateSpeed, 0.01f, 1.0f, 2000.0f, 50.0f)
            : 2000.0f;
        float gateReleaseCoeff = std::exp(-1.0f / (float)(sampleRate * gateReleaseTimeMs / 1000.0));
        float gateAttackTimeMs = 10.0f;
        float gateAttackCoeff = std::exp(-1.0f / (float)(sampleRate * gateAttackTimeMs / 1000.0));
        
        float envAttack = std::exp(-1.0f / (0.005f * (float)sampleRate));
        float envRelease = std::exp(-1.0f / (0.15f * (float)sampleRate));
        
        for (int i = 0; i < numSamples; ++i)
        {
            // Input level (from dry signal)
            float inputLevel = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                inputLevel = std::max(inputLevel, std::abs(irDryBuffer.getSample(ch, i)));
            
            // Envelope follower
            if (inputLevel > inputEnvelope)
                inputEnvelope = envAttack * inputEnvelope + (1.0f - envAttack) * inputLevel;
            else
                inputEnvelope = envRelease * inputEnvelope + (1.0f - envRelease) * inputLevel;
            
            // DUCK: reduce reverb when singer is active
            float targetDuckGain = 1.0f - (params.duck * juce::jlimit(0.0f, 1.0f, inputEnvelope * 10.0f));
            float duckCoeff = (targetDuckGain < duckGain) ? 0.99f : 0.995f;
            duckGain = duckCoeff * duckGain + (1.0f - duckCoeff) * targetDuckGain;
            
            // GATE: only active when gateSpeed > 0.01  (FIX)
            if (gateActive)
            {
                float targetGateGain = (inputEnvelope > gateThreshLinear) ? 1.0f : 0.0f;
                float gateCoeff = (targetGateGain > gateGain) ? gateAttackCoeff : gateReleaseCoeff;
                gateGain = gateCoeff * gateGain + (1.0f - gateCoeff) * targetGateGain;
            }
            else
            {
                gateGain = 1.0f;  // Gate fully open when disabled
            }
            
            // Combine — IR convolution spreads energy over time, reducing peak level.
            // 3.5x wet boost compensates to match algorithmic reverb perceived levels.
            float combinedWetGain = params.mix * duckGain * gateGain * 3.5f;
            
            for (int ch = 0; ch < numChannels; ++ch)
            {
                float drySample = irDryBuffer.getSample(ch, i);
                float wetSample = irWetBuffer.getSample(ch, i);
                buffer.setSample(ch, i, drySample * (1.0f - params.mix) + wetSample * combinedWetGain);
            }
        }
        
        // Metering for visualization
        float wetLevel = 0.0f;
        for (int i = 0; i < numSamples; ++i)
            wetLevel = std::max(wetLevel, std::abs(buffer.getSample(0, i)));
        decayLevel = decayLevel * 0.99f + wetLevel * 0.01f;
    }

    // ==============================================================================
    // Hall Reverb - FDN with modulation
    // ==============================================================================
    void processHall(float inMono, float inL, float inR, float& outL, float& outR)
    {
        // Modulation LFO
        modPhase += (0.5f + params.hallModulation * 1.5f) / (float)sampleRate;
        if (modPhase >= 1.0f) modPhase -= 1.0f;
        float mod = std::sin(modPhase * juce::MathConstants<float>::twoPi);
        
        // Calculate feedback based on decay — capped conservatively
        float feedback = std::pow(0.001f, 1.0f / (params.decay * (float)sampleRate / (float)hallDelayTimes[0]));
        feedback = juce::jlimit(0.0f, 0.93f, feedback);
        
        // FDN with 8 delay lines
        std::array<float, 8> fdnIn, fdnOut;
        
        // Read from delay lines with modulation using LINEAR INTERPOLATION
        // (integer truncation caused clicks when mod changed sign)
        for (int j = 0; j < 8; ++j)
        {
            float modAmount = (j % 2 == 0) ? mod : -mod;
            float modSamples = modAmount * params.hallModulation * 10.0f;
            
            float readPosF = (float)delayWritePos[j] - (float)hallDelayTimes[j] - modSamples;
            int dlSize = (int)delayLines[j].size();
            while (readPosF < 0.0f) readPosF += (float)dlSize;
            
            int readPos0 = (int)readPosF % dlSize;
            int readPos1 = (readPos0 + 1) % dlSize;
            float frac = readPosF - std::floor(readPosF);
            
            fdnOut[j] = delayLines[j][readPos0] * (1.0f - frac) + delayLines[j][readPos1] * frac;
        }
        
        // Householder mixing matrix (preserves energy)
        float sum = 0.0f;
        for (int k = 0; k < 8; ++k)
            sum += fdnOut[k];
        for (int j = 0; j < 8; ++j)
            fdnIn[j] = fdnOut[j] - sum * 0.25f;
        
        // Apply diffusion through allpass filters
        float diffusedL = processAllpass(0, inMono, params.hallDiffusion * 0.65f);
        float diffusedR = processAllpass(1, inMono, params.hallDiffusion * 0.65f);
        
        // Write back with input and feedback — soft clip to prevent runaway
        for (int j = 0; j < 8; ++j)
        {
            float input = (j < 4) ? diffusedL : diffusedR;
            float damped = dampingFilters[j].processSample(fdnIn[j] * feedback);
            float value = input * 0.2f + damped;
            delayLines[j][delayWritePos[j]] = std::tanh(value);
            delayWritePos[j] = (delayWritePos[j] + 1) % (int)delayLines[j].size();
        }
        
        // Sum outputs with stereo spread
        outL = outR = 0.0f;
        float width = params.hallWidth;
        for (int j = 0; j < 4; ++j)
        {
            outL += fdnOut[j] * (0.5f + width * 0.5f);
            outL += fdnOut[j + 4] * (0.5f - width * 0.5f) * 0.3f;
        }
        for (int j = 4; j < 8; ++j)
        {
            outR += fdnOut[j] * (0.5f + width * 0.5f);
            outR += fdnOut[j - 4] * (0.5f - width * 0.5f) * 0.3f;
        }
        
        outL *= 0.35f;
        outR *= 0.35f;
        
        decayLevel = decayLevel * 0.999f + std::abs(outL + outR) * 0.001f;
    }

    // ==============================================================================
    // Plate Reverb - Dense allpass network (FIX: reduced feedback to prevent oscillation)
    // ==============================================================================
    void processPlate(float inMono, float& outL, float& outR)
    {
        // Plate uses dense allpass chains for quick buildup
        float feedback = std::pow(0.001f, 1.0f / (params.decay * 0.7f * (float)sampleRate / (float)plateDelayTimes[0]));
        feedback = juce::jlimit(0.0f, 0.88f, feedback);  // FIX: capped at 0.88 (was 0.95)
        
        // Multiple allpass stages for density — decorrelated L/R paths
        float ap1 = processAllpass(2, inMono, 0.5f + params.plateDensity * 0.2f);
        float ap2 = processAllpass(3, ap1, 0.5f + params.plateDensity * 0.2f);
        float ap3 = processAllpass(4, ap2, 0.4f + params.plateDensity * 0.2f);
        float ap4 = processAllpass(5, ap1, 0.4f + params.plateDensity * 0.2f);  // Separate R path
        
        // Read from plate delay lines
        std::array<float, 4> plateOut;
        for (int j = 0; j < 4; ++j)
        {
            int readPos = delayWritePos[j] - plateDelayTimes[j];
            while (readPos < 0) readPos += (int)delayLines[j].size();
            plateOut[j] = delayLines[j][readPos % (int)delayLines[j].size()];
        }
        
        // FIX: Reduced cross-feedback (was 0.3, now 0.12) to prevent self-oscillation
        float xfb = 0.12f;
        float fb0 = plateOut[0] + plateOut[2] * xfb;
        float fb1 = plateOut[1] + plateOut[3] * xfb;
        float fb2 = plateOut[2] + plateOut[0] * xfb;
        float fb3 = plateOut[3] + plateOut[1] * xfb;
        
        // Apply damping
        for (int j = 0; j < 4; ++j)
            plateOut[j] = dampingFilters[j].processSample(plateOut[j]);
        
        // FIX: Split L/R input (was ap3 into all 4 — correlated excitation), soft clip
        delayLines[0][delayWritePos[0]] = std::tanh(ap3 * 0.4f + fb0 * feedback);
        delayLines[1][delayWritePos[1]] = std::tanh(ap4 * 0.4f + fb1 * feedback);
        delayLines[2][delayWritePos[2]] = std::tanh(ap4 * 0.3f + fb2 * feedback);
        delayLines[3][delayWritePos[3]] = std::tanh(ap3 * 0.3f + fb3 * feedback);
        
        for (int j = 0; j < 4; ++j)
            delayWritePos[j] = (delayWritePos[j] + 1) % (int)delayLines[j].size();
        
        // Output with brightness shimmer
        outL = (plateOut[0] + plateOut[2] * 0.7f) * 0.5f;
        outR = (plateOut[1] + plateOut[3] * 0.7f) * 0.5f;
        
        // Add brightness (subtle high-frequency emphasis)
        if (params.plateBrightness > 0.5f)
        {
            float shimmer = (params.plateBrightness - 0.5f) * 2.0f;
            outL += (plateOut[0] - outL) * shimmer * 0.3f;
            outR += (plateOut[1] - outR) * shimmer * 0.3f;
        }
        
        decayLevel = decayLevel * 0.999f + std::abs(outL + outR) * 0.001f;
    }

    // ==============================================================================
    // Ambiance Reverb - Early reflections focused (FIX: increased gain levels)
    // ==============================================================================
    void processAmbiance(float inL, float inR, float& outL, float& outR)
    {
        float sizeScale = 0.3f + params.ambSize * 0.7f;
        
        // Early reflections (tap delays)
        float earlyL = 0.0f, earlyR = 0.0f;
        
        for (int j = 0; j < 6; ++j)
        {
            int tapTime = (int)(earlyTapTimes[j] * sizeScale);
            int readPos = (int)preDelayWritePos - tapTime;
            if (readPos < 0) readPos += (int)preDelayL.size();
            readPos = readPos % (int)preDelayL.size();
            
            float tapGain = earlyTapGains[j] * params.ambLiveliness;
            if (j % 2 == 0)
            {
                earlyL += preDelayL[readPos] * tapGain;
                earlyR += preDelayR[readPos] * tapGain * 0.7f;
            }
            else
            {
                earlyR += preDelayR[readPos] * tapGain;
                earlyL += preDelayL[readPos] * tapGain * 0.7f;
            }
        }
        
        // Late reverb (simple feedback network)
        float lateDecay = params.decay * 0.5f;
        float fb = std::pow(0.001f, 1.0f / (lateDecay * (float)sampleRate / (float)ambDelayTimes[0]));
        fb = juce::jlimit(0.0f, 0.85f, fb);
        
        // Read late reverb
        float lateL = 0.0f, lateR = 0.0f;
        for (int j = 0; j < 4; ++j)
        {
            int delayTime = (int)(ambDelayTimes[j] * sizeScale);
            delayTime = juce::jlimit(1, (int)delayLines[j].size() - 1, delayTime);
            int readPos = delayWritePos[j] - delayTime;
            if (readPos < 0) readPos += (int)delayLines[j].size();
            
            float sample = delayLines[j][readPos];
            if (j < 2)
                lateL += sample;
            else
                lateR += sample;
        }
        lateL *= 0.5f;
        lateR *= 0.5f;
        
        // Write to delay lines — CRITICAL: total linear gain per line MUST be < 1.0
        // At fb=0.85 max:  0.2 + 0.65*0.85 + 0.05*0.85 = 0.795 — safe margin
        delayLines[0][delayWritePos[0]] = std::tanh(inL * 0.2f + lateL * fb * 0.65f + lateR * fb * 0.05f);
        delayLines[1][delayWritePos[1]] = std::tanh(inL * 0.12f + lateL * fb * 0.35f);
        delayLines[2][delayWritePos[2]] = std::tanh(inR * 0.2f + lateR * fb * 0.65f + lateL * fb * 0.05f);
        delayLines[3][delayWritePos[3]] = std::tanh(inR * 0.12f + lateR * fb * 0.35f);
        
        for (int j = 0; j < 4; ++j)
            delayWritePos[j] = (delayWritePos[j] + 1) % (int)delayLines[j].size();
        
        // Mix early/late based on parameter
        // Boost late output to compensate for conservative feedback gains
        float earlyMix = 1.0f - params.ambEarlyLate;
        float lateMix = params.ambEarlyLate;
        
        outL = earlyL * earlyMix + lateL * lateMix * 1.4f;
        outR = earlyR * earlyMix + lateR * lateMix * 1.4f;
        
        decayLevel = decayLevel * 0.999f + std::abs(outL + outR) * 0.001f;
    }

    // ==============================================================================
    // Allpass filter processing
    // ==============================================================================
    float processAllpass(int index, float input, float coeff)
    {
        int size = (int)allpassLines[index].size();
        int readPos = allpassWritePos[index] - allpassDelays[index];
        while (readPos < 0) readPos += size;
        
        float delayed = allpassLines[index][readPos % size];
        float output = delayed - coeff * input;
        allpassLines[index][allpassWritePos[index]] = input + coeff * delayed;
        allpassWritePos[index] = (allpassWritePos[index] + 1) % size;
        
        return output;
    }

    // ==============================================================================
    // Initialization
    // ==============================================================================
    void initDelayLines()
    {
        // Maximum delay line size for 10s decay at 192kHz
        int maxSize = 192000 * 2;
        
        for (auto& dl : delayLines)
            dl.resize(maxSize, 0.0f);
        for (auto& pos : delayWritePos)
            pos = 0;
        
        // Allpass delays (prime numbers for good diffusion)
        baseAllpassDelays = { 142, 107, 379, 277, 419, 307, 167, 97 };
        allpassDelays = baseAllpassDelays;
        for (int i = 0; i < 8; ++i)
        {
            allpassLines[i].resize(2048, 0.0f);
            allpassWritePos[i] = 0;
        }
        
        // Base delay times at 44100 Hz (mutually prime for rich texture)
        baseHallDelayTimes = { 1557, 1617, 1491, 1422, 1277, 1356, 1188, 1116 };
        basePlateDelayTimes = { 1051, 1123, 1213, 1327 };
        baseAmbDelayTimes = { 683, 751, 827, 911 };
        
        hallDelayTimes = baseHallDelayTimes;
        plateDelayTimes = basePlateDelayTimes;
        ambDelayTimes = baseAmbDelayTimes;
        
        // Early reflection tap times and gains
        earlyTapTimes = { 23, 41, 67, 89, 127, 173 };
        earlyTapGains = { 0.8f, 0.7f, 0.5f, 0.4f, 0.3f, 0.2f };
    }

    // FIX: Always scale from BASE times (was scaling in-place — accumulated errors)
    void updateDelayTimes()
    {
        if (sampleRate <= 0.0) return;
        
        float scale = (float)(sampleRate / 44100.0);
        
        for (int i = 0; i < 8; ++i)
        {
            hallDelayTimes[i] = (int)(baseHallDelayTimes[i] * scale);
            allpassDelays[i] = (int)(baseAllpassDelays[i] * scale);
        }
        for (int i = 0; i < 4; ++i)
        {
            plateDelayTimes[i] = (int)(basePlateDelayTimes[i] * scale);
            ambDelayTimes[i] = (int)(baseAmbDelayTimes[i] * scale);
        }
    }

    void updateFilters()
    {
        if (sampleRate <= 0.0) return;
        
        auto lowCutCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass(
            sampleRate, params.lowCut, 0.707f);
        auto highCutCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass(
            sampleRate, params.highCut, 0.707f);
        
        *inputLowCut.coefficients = *lowCutCoeffs;
        *inputHighCut.coefficients = *highCutCoeffs;
        
        *outputLowCutL.coefficients = *lowCutCoeffs;
        *outputHighCutL.coefficients = *highCutCoeffs;
        *outputLowCutR.coefficients = *lowCutCoeffs;
        *outputHighCutR.coefficients = *highCutCoeffs;
        
        *irLowCutL.coefficients = *lowCutCoeffs;
        *irHighCutL.coefficients = *highCutCoeffs;
        *irLowCutR.coefficients = *lowCutCoeffs;
        *irHighCutR.coefficients = *highCutCoeffs;
        
        // Damping filters (low-pass for high-frequency absorption)
        float dampFreq = 4000.0f + (1.0f - params.plateDamping) * 12000.0f;
        auto dampCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass(
            sampleRate, dampFreq, 0.5f);
        for (auto& f : dampingFilters)
            *f.coefficients = *dampCoeffs;
    }

    // ==============================================================================
    // Members
    // ==============================================================================
    Params params;
    bool bypassed = false;
    double sampleRate = 44100.0;
    
    // Pre-delay
    std::vector<float> preDelayL, preDelayR;
    int preDelayWritePos = 0;
    
    // FDN delay lines (8 for hall, 4 used for plate/ambiance)
    std::array<std::vector<float>, 8> delayLines;
    std::array<int, 8> delayWritePos = {};
    
    // Allpass diffusers
    std::array<std::vector<float>, 8> allpassLines;
    std::array<int, 8> allpassWritePos = {};
    std::array<int, 8> allpassDelays = {};
    std::array<int, 8> baseAllpassDelays = {};
    
    // Base delay times (at 44100 Hz) — FIX: stored separately to prevent accumulation
    std::array<int, 8> baseHallDelayTimes = {};
    std::array<int, 4> basePlateDelayTimes = {};
    std::array<int, 4> baseAmbDelayTimes = {};
    
    // Scaled delay times for current sample rate
    std::array<int, 8> hallDelayTimes = {};
    std::array<int, 4> plateDelayTimes = {};
    std::array<int, 4> ambDelayTimes = {};
    
    // Early reflection taps
    std::array<int, 6> earlyTapTimes = {};
    std::array<float, 6> earlyTapGains = {};
    
    // Input filters (mono — for algo reverb mono input)
    juce::dsp::IIR::Filter<float> inputLowCut, inputHighCut;
    
    // Output filters — FIX: separate L/R instances (was single filter per type)
    juce::dsp::IIR::Filter<float> outputLowCutL, outputHighCutL;
    juce::dsp::IIR::Filter<float> outputLowCutR, outputHighCutR;
    
    // Damping filters for delay lines
    std::array<juce::dsp::IIR::Filter<float>, 8> dampingFilters;
    
    // IR Convolution
    juce::dsp::Convolution convolution;
    juce::AudioBuffer<float> irDryBuffer, irWetBuffer;
    juce::String currentIrName { "Default (Internal)" };
    
    // IR stereo filters
    juce::dsp::IIR::Filter<float> irLowCutL, irHighCutL;
    juce::dsp::IIR::Filter<float> irLowCutR, irHighCutR;
    
    // Ducking / gate state (shared by all modes)
    float inputEnvelope = 0.0f;
    float duckGain = 1.0f;
    float gateGain = 1.0f;
    
    // Modulation
    float modPhase = 0.0f;
    
    // Metering
    float decayLevel = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReverbProcessor)
};
