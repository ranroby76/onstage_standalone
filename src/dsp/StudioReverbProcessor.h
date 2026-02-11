// ==============================================================================
//  StudioReverbProcessor.h
//  OnStage — Studio Reverb (Multi-Model, Airwindows backends)
//
//  Models:
//    0 = Room    (Verbity2)   — 3 params + dry/wet
//    1 = Chamber (Chamber2)   — 3 params + dry/wet
//    2 = Space   (Galactic3)  — 5 params + dry/wet
//    3 = Plate   (kPlateD)    — 4 params + dry/wet
//
//  Dry/Wet are independent gain controls (0–1 each) handled in the wrapper.
//  DSP processors always run at 100% wet internally.
// ==============================================================================

#pragma once
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>

#include "RoomReverbProcessor.h"
#include "ChamberReverbProcessor.h"
#include "SpaceReverbProcessor.h"
#include "PlateReverbProcessor.h"

class StudioReverbProcessor
{
public:
    // ==============================================================================
    enum class ReverbModel { Room = 0, Chamber, Space, Plate, NumModels };

    static constexpr int getNumModels() { return (int)ReverbModel::NumModels; }

    static const char* getModelName(int i)
    {
        constexpr const char* names[] = { "Room", "Chamber", "Space", "Plate" };
        return (i >= 0 && i < getNumModels()) ? names[i] : "Unknown";
    }

    // ==============================================================================
    //  Unified Params — holds all per-model parameters + independent dry/wet
    // ==============================================================================
    struct Params
    {
        // Independent dry/wet gains (shared across all models)
        float dry = 1.0f;    // 0–1: dry signal level
        float wet = 0.5f;    // 0–1: wet (reverb) signal level

        // Room (Verbity2)
        float roomSize   = 0.5f;    // A: 0–1
        float roomSustain= 0.5f;    // B: 0–1
        float roomMulch  = 0.5f;    // C: 0–1

        // Chamber (Chamber2)
        float chamberDelay = 0.34f; // A: 0–1
        float chamberRegen = 0.31f; // B: 0–1
        float chamberThick = 0.28f; // C: 0–1

        // Space (Galactic3)
        float spaceReplace    = 0.5f;  // A: 0–1
        float spaceBrightness = 0.5f;  // B: 0–1
        float spaceDetune     = 0.5f;  // C: 0–1
        float spaceDerez      = 0.0f;  // D: 0–1
        float spaceBigness    = 0.5f;  // E: 0–1

        // Plate (kPlateD)
        float plateInputPad = 1.0f;   // A: 0–1
        float plateDamping  = 0.5f;   // B: 0–1
        float plateLowCut   = 1.0f;   // C: 0–1
        float platePredelay = 0.0f;   // D: 0–1

        bool operator==(const Params& o) const
        {
            return dry == o.dry && wet == o.wet &&
                   roomSize == o.roomSize && roomSustain == o.roomSustain &&
                   roomMulch == o.roomMulch &&
                   chamberDelay == o.chamberDelay && chamberRegen == o.chamberRegen &&
                   chamberThick == o.chamberThick &&
                   spaceReplace == o.spaceReplace && spaceBrightness == o.spaceBrightness &&
                   spaceDetune == o.spaceDetune && spaceDerez == o.spaceDerez &&
                   spaceBigness == o.spaceBigness &&
                   plateInputPad == o.plateInputPad && plateDamping == o.plateDamping &&
                   plateLowCut == o.plateLowCut && platePredelay == o.platePredelay;
        }
        bool operator!=(const Params& o) const { return !(*this == o); }
    };

    // ==============================================================================
    StudioReverbProcessor() = default;

    void setModel(int i)
    {
        auto m = static_cast<ReverbModel>(juce::jlimit(0, getNumModels() - 1, i));
        if (m != currentModel)
        {
            currentModel = m;
            switch (m)
            {
                case ReverbModel::Room:    roomProc.reset();    break;
                case ReverbModel::Chamber: chamberProc.reset(); break;
                case ReverbModel::Space:   spaceProc.reset();   break;
                case ReverbModel::Plate:   plateProc.reset();   break;
                default: break;
            }
        }
    }

    int getModelIndex() const { return (int)currentModel; }

    Params getParams() const { return params; }
    void   setParams(const Params& p)
    {
        params = p;
        pushParamsToActiveModel();
    }

    bool  isBypassed() const   { return bypassed; }
    void  setBypassed(bool b)  { bypassed = b; }

    float getCurrentDecayLevel() const
    {
        switch (currentModel)
        {
            case ReverbModel::Room:    return roomProc.getCurrentDecayLevel();
            case ReverbModel::Chamber: return chamberProc.getCurrentDecayLevel();
            case ReverbModel::Space:   return spaceProc.getCurrentDecayLevel();
            case ReverbModel::Plate:   return plateProc.getCurrentDecayLevel();
            default: return 0.0f;
        }
    }

    // ==============================================================================
    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        maxBlockSize = (int)spec.maximumBlockSize;
        dryBuffer.setSize(2, maxBlockSize);
        roomProc.prepare(spec);
        chamberProc.prepare(spec);
        spaceProc.prepare(spec);
        plateProc.prepare(spec);
    }

    void reset()
    {
        roomProc.reset();
        chamberProc.reset();
        spaceProc.reset();
        plateProc.reset();
    }

    void process(juce::AudioBuffer<float>& buffer)
    {
        if (bypassed) return;

        pushParamsToActiveModel();

        const int numSamples  = buffer.getNumSamples();
        const int numChannels = juce::jmin(2, buffer.getNumChannels());

        // Ensure dry buffer is big enough
        if (dryBuffer.getNumSamples() < numSamples)
            dryBuffer.setSize(2, numSamples, false, false, true);

        // Save dry signal
        for (int ch = 0; ch < numChannels; ++ch)
            dryBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

        // Process reverb (DSP outputs 100% wet)
        switch (currentModel)
        {
            case ReverbModel::Room:    roomProc.process(buffer);    break;
            case ReverbModel::Chamber: chamberProc.process(buffer); break;
            case ReverbModel::Space:   spaceProc.process(buffer);   break;
            case ReverbModel::Plate:   plateProc.process(buffer);   break;
            default: break;
        }

        // Mix: output = dry * dryGain + wet * wetGain
        const float dryGain = params.dry;
        const float wetGain = params.wet;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* out      = buffer.getWritePointer(ch);
            const float* dr = dryBuffer.getReadPointer(ch);
            for (int i = 0; i < numSamples; ++i)
                out[i] = dr[i] * dryGain + out[i] * wetGain;
        }
    }

private:
    void pushParamsToActiveModel()
    {
        switch (currentModel)
        {
            case ReverbModel::Room:
            {
                RoomReverbProcessor::Params rp;
                rp.roomSize = params.roomSize;
                rp.sustain  = params.roomSustain;
                rp.mulch    = params.roomMulch;
                rp.wetness  = 1.0f;  // always full wet — mixing in wrapper
                roomProc.setParams(rp);
                break;
            }
            case ReverbModel::Chamber:
            {
                ChamberReverbProcessor::Params cp;
                cp.delay   = params.chamberDelay;
                cp.regen   = params.chamberRegen;
                cp.thick   = params.chamberThick;
                cp.wet     = 1.0f;  // always full wet
                chamberProc.setParams(cp);
                break;
            }
            case ReverbModel::Space:
            {
                SpaceReverbProcessor::Params sp;
                sp.replace    = params.spaceReplace;
                sp.brightness = params.spaceBrightness;
                sp.detune     = params.spaceDetune;
                sp.derez      = params.spaceDerez;
                sp.bigness    = params.spaceBigness;
                sp.dryWet     = 1.0f;  // always full wet
                spaceProc.setParams(sp);
                break;
            }
            case ReverbModel::Plate:
            {
                PlateReverbProcessor::Params pp;
                pp.inputPad = params.plateInputPad;
                pp.damping  = params.plateDamping;
                pp.lowCut   = params.plateLowCut;
                pp.predelay = params.platePredelay;
                pp.wetness  = 1.0f;  // always full wet
                plateProc.setParams(pp);
                break;
            }
            default: break;
        }
    }

    // ==============================================================================
    ReverbModel currentModel = ReverbModel::Room;
    Params params;
    double sampleRate = 0.0;
    int maxBlockSize = 512;
    bool bypassed = false;

    juce::AudioBuffer<float> dryBuffer;  // saved dry signal for parallel mixing

    RoomReverbProcessor    roomProc;
    ChamberReverbProcessor chamberProc;
    SpaceReverbProcessor   spaceProc;
    PlateReverbProcessor   plateProc;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StudioReverbProcessor)
};
