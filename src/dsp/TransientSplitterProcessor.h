// ==============================================================================
//  TransientSplitterProcessor.h
//  OnStage — Transient/Sustain splitter DSP
//
//  Adapted from Colosseum full AudioProcessor version.
//  DSP-only class (no AudioProcessor base) for OnStage EffectNode wrapping.
//  2-in, 4-out: Transient L/R (ch 0-1), Sustain L/R (ch 2-3)
//  Zero latency, real-time envelope follower based detection.
// ==============================================================================

#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>

class TransientSplitterProcessor
{
public:
    TransientSplitterProcessor() = default;

    void prepare (double sampleRate, int samplesPerBlock);
    void reset();

    // Expects a buffer with >= 4 channels.
    // Input on channels 0,1.  Output: Transient on 0,1 — Sustain on 2,3.
    void process (juce::AudioBuffer<float>& buffer);

    void getState (juce::MemoryBlock& dest) const;
    void setState (const void* data, int size);

    // =========================================================================
    //  Parameters (all atomic for real-time safety)
    // =========================================================================

    // Detection
    std::atomic<float> sensitivity   { 0.5f };       // 0–1, how aggressively transients are detected
    std::atomic<float> decay         { 50.0f };       // ms, how long signal stays classified as transient
    std::atomic<float> holdTime      { 10.0f };       // ms, minimum transient gate open time
    std::atomic<float> smoothing     { 2.0f };        // ms, crossfade smoothness at split boundary

    // Frequency focus (detection sidechain filter)
    std::atomic<float> focusHPFreq   { 20.0f };       // Hz, high-pass on detection (20 = off)
    std::atomic<float> focusLPFreq   { 20000.0f };    // Hz, low-pass on detection (20k = off)

    // Output
    std::atomic<float> transientGainDb { 0.0f };      // dB, -60 to +12
    std::atomic<float> sustainGainDb   { 0.0f };      // dB, -60 to +12
    std::atomic<float> balance         { 0.0f };      // -1 (all transient) to +1 (all sustain), 0 = clean split

    // Modes
    std::atomic<bool>  stereoLinked  { true };         // true = mono detection, false = independent L/R
    std::atomic<bool>  gateMode      { false };        // true = hard gate, false = proportional split
    std::atomic<bool>  invertMode    { false };        // true = swap transient/sustain outputs

    // Metering (read-only, updated from audio thread)
    std::atomic<float> transientRmsL    { 0.0f };
    std::atomic<float> transientRmsR    { 0.0f };
    std::atomic<float> sustainRmsL      { 0.0f };
    std::atomic<float> sustainRmsR      { 0.0f };
    std::atomic<float> transientActivity { 0.0f };    // 0–1, current transient detection level

private:
    double currentSampleRate = 44100.0;

    // Pre-allocated temp buffer — avoids stack allocation in processBlock
    juce::AudioBuffer<float> tempBuffer;

    // Envelope followers (per channel)
    float fastEnvL = 0.0f, fastEnvR = 0.0f;
    float slowEnvL = 0.0f, slowEnvR = 0.0f;
    float gateL = 0.0f,    gateR = 0.0f;
    float smoothGateL = 0.0f, smoothGateR = 0.0f;

    // Hold counters (samples remaining)
    int holdCounterL = 0, holdCounterR = 0;

    // Detection sidechain filters (per channel)
    juce::dsp::IIR::Filter<float> detHPFilterL, detHPFilterR;
    juce::dsp::IIR::Filter<float> detLPFilterL, detLPFilterR;

    float lastHPFreq = 20.0f;
    float lastLPFreq = 20000.0f;

    void updateDetectionFilters();

    float msToCoeff (float ms) const
    {
        if (ms <= 0.0f) return 0.0f;
        return std::exp (-1.0f / (float)(currentSampleRate * ms * 0.001));
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransientSplitterProcessor)
};
