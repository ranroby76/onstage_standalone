#pragma once
// ==============================================================================
//  DelayProcessor.h
//  OnStage - Delay Processor wrapping 4 Airwindows-based DSP models
//
//  All models output PURE WET. This wrapper applies:
//    output = dry * input + wet * effect
//  Dry and Wet are the FIRST two params (p[0], p[1]) for every model.
//  Model-specific params follow at p[2..7].
//
//  Based on Airwindows open source code (MIT license) by Chris Johnson
// ==============================================================================

#include <juce_audio_basics/juce_audio_basics.h>
#include "DelayDSP_Oxide.h"
#include "DelayDSP_Warp.h"
#include "DelayDSP_Crystal.h"
#include "DelayDSP_Drift.h"

class DelayProcessor
{
public:
    enum class Type
    {
        Oxide = 0,    // Tape delay
        Warp,         // Pitch delay
        Crystal,      // Pure echo 4-tap
        Drift         // Stereo doubler
    };

    // p[0] = Dry, p[1] = Wet, p[2..7] = model-specific (up to 6 model params)
    static constexpr int MAX_PARAMS = 8;

    struct Params
    {
        Type type = Type::Oxide;
        float p[MAX_PARAMS] = { 0.0f };

        bool operator==(const Params& other) const
        {
            if (type != other.type) return false;
            for (int i = 0; i < MAX_PARAMS; i++)
                if (p[i] != other.p[i]) return false;
            return true;
        }
        bool operator!=(const Params& other) const { return !(*this == other); }
    };

    // ==============================================================================
    // Total param count = 2 (Dry+Wet) + model params
    // ==============================================================================
    static int getNumParams(Type type)
    {
        switch (type) {
            case Type::Oxide:   return 2 + DelayDSP_Oxide::NUM_PARAMS;    // 2+4=6
            case Type::Warp:    return 2 + DelayDSP_Warp::NUM_PARAMS;     // 2+5=7
            case Type::Crystal: return 2 + DelayDSP_Crystal::NUM_PARAMS;  // 2+5=7
            case Type::Drift:   return 2 + DelayDSP_Drift::NUM_PARAMS;    // 2+4=6
        }
        return 2;
    }

    static const char* getParamName(Type type, int index)
    {
        if (index == 0) return "Dry";
        if (index == 1) return "Wet";
        int modelIdx = index - 2;
        switch (type) {
            case Type::Oxide:   return DelayDSP_Oxide::getParamName(modelIdx);
            case Type::Warp:    return DelayDSP_Warp::getParamName(modelIdx);
            case Type::Crystal: return DelayDSP_Crystal::getParamName(modelIdx);
            case Type::Drift:   return DelayDSP_Drift::getParamName(modelIdx);
        }
        return "";
    }

    static const char* getParamSuffix(Type type, int index)
    {
        if (index < 2) return "";
        int modelIdx = index - 2;
        switch (type) {
            case Type::Oxide:   return DelayDSP_Oxide::getParamSuffix(modelIdx);
            case Type::Warp:    return DelayDSP_Warp::getParamSuffix(modelIdx);
            case Type::Crystal: return DelayDSP_Crystal::getParamSuffix(modelIdx);
            case Type::Drift:   return DelayDSP_Drift::getParamSuffix(modelIdx);
        }
        return "";
    }

    static float getDefaultValue(Type type, int index)
    {
        if (index == 0) return 1.0f;  // Dry default = full
        if (index == 1) return 0.5f;  // Wet default = half
        int modelIdx = index - 2;
        switch (type) {
            case Type::Oxide:   return DelayDSP_Oxide::getDefaultValue(modelIdx);
            case Type::Warp:    return DelayDSP_Warp::getDefaultValue(modelIdx);
            case Type::Crystal: return DelayDSP_Crystal::getDefaultValue(modelIdx);
            case Type::Drift:   return DelayDSP_Drift::getDefaultValue(modelIdx);
        }
        return 0.0f;
    }

    static void getParamRange(Type type, int index, double& min, double& max, double& step)
    {
        if (index < 2) { min = 0.0; max = 1.0; step = 0.01; return; }
        int modelIdx = index - 2;
        switch (type) {
            case Type::Oxide:   DelayDSP_Oxide::getParamRange(modelIdx, min, max, step); break;
            case Type::Warp:    DelayDSP_Warp::getParamRange(modelIdx, min, max, step); break;
            case Type::Crystal: DelayDSP_Crystal::getParamRange(modelIdx, min, max, step); break;
            case Type::Drift:   DelayDSP_Drift::getParamRange(modelIdx, min, max, step); break;
        }
    }

    static const char* getTypeName(Type type)
    {
        switch (type) {
            case Type::Oxide:   return "OXIDE";
            case Type::Warp:    return "WARP";
            case Type::Crystal: return "CRYSTAL";
            case Type::Drift:   return "DRIFT";
        }
        return "";
    }

    // ==============================================================================
    // Lifecycle
    // ==============================================================================
    void prepare(double sampleRate, int samplesPerBlock, int /*numChannels*/)
    {
        sRate = sampleRate;
        dspOxide.prepare(sampleRate);
        dspWarp.prepare(sampleRate);
        dspCrystal.prepare(sampleRate);
        dspDrift.prepare(sampleRate);

        // Pre-allocate temp buffer for dry capture
        dryBuffer.setSize(2, samplesPerBlock);
        loadDefaults();
    }

    void reset()
    {
        dspOxide.reset();
        dspWarp.reset();
        dspCrystal.reset();
        dspDrift.reset();
    }

    void process(juce::AudioBuffer<float>& buffer)
    {
        if (bypassed) return;

        const int n = buffer.getNumSamples();
        const int numCh = buffer.getNumChannels();

        // Capture dry signal before DSP modifies the buffer
        dryBuffer.setSize(numCh, n, false, false, true);
        for (int ch = 0; ch < numCh; ++ch)
            dryBuffer.copyFrom(ch, 0, buffer, ch, 0, n);

        auto* l = buffer.getWritePointer(0);
        auto* r = numCh > 1 ? buffer.getWritePointer(1) : nullptr;

        const float dry = params.p[0];
        const float wet = params.p[1];

        // Run DSP — each model overwrites buffer with pure wet signal
        switch (params.type)
        {
            case Type::Oxide:
                dspOxide.process(l, r, n, params.p[2], params.p[3], params.p[4], params.p[5]);
                break;
            case Type::Warp:
                dspWarp.process(l, r, n, params.p[2], params.p[3], params.p[4], params.p[5], params.p[6]);
                break;
            case Type::Crystal:
                dspCrystal.process(l, r, n, params.p[2], params.p[3], params.p[4], params.p[5], params.p[6]);
                break;
            case Type::Drift:
                dspDrift.process(l, r, n, params.p[2], params.p[3], params.p[4], params.p[5]);
                break;
        }

        // Mix: output = dry * original + wet * effect
        const float* dryL = dryBuffer.getReadPointer(0);
        const float* dryR = numCh > 1 ? dryBuffer.getReadPointer(1) : nullptr;

        for (int i = 0; i < n; ++i)
        {
            l[i] = dryL[i] * dry + l[i] * wet;
            if (r && dryR)
                r[i] = dryR[i] * dry + r[i] * wet;
        }
    }

    void setParams(const Params& p)
    {
        if (params.type != p.type)
        {
            params = p;
            switch (params.type) {
                case Type::Oxide:   dspOxide.reset(); dspOxide.prepare(sRate); break;
                case Type::Warp:    dspWarp.reset(); dspWarp.prepare(sRate); break;
                case Type::Crystal: dspCrystal.reset(); dspCrystal.prepare(sRate); break;
                case Type::Drift:   dspDrift.reset(); dspDrift.prepare(sRate); break;
            }
        }
        else
        {
            params = p;
        }
    }

    Params getParams() const { return params; }

    void setBypassed(bool shouldBypass) { bypassed = shouldBypass; }
    bool isBypassed() const { return bypassed; }

private:
    void loadDefaults()
    {
        int total = getNumParams(params.type);
        for (int i = 0; i < MAX_PARAMS; i++)
            params.p[i] = (i < total) ? getDefaultValue(params.type, i) : 0.0f;
    }

    double sRate = 44100.0;
    Params params;
    bool bypassed = false;

    juce::AudioBuffer<float> dryBuffer;

    DelayDSP_Oxide   dspOxide;
    DelayDSP_Warp    dspWarp;
    DelayDSP_Crystal dspCrystal;
    DelayDSP_Drift   dspDrift;
};
