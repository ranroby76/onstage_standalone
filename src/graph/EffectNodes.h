
// D:\Workspace\ONSTAGE_WIRED\src\graph\EffectNodes.h
// ==============================================================================
//  EffectNodes.h
//  OnStage — Node wrappers for all DSP processors
//
//  Each wrapper makes an existing DSP processor usable as a node inside
//  juce::AudioProcessorGraph.  Audio-only (no MIDI pins).
//  DynamicEQ exposes sidechain buses (green pins on canvas).
// ==============================================================================

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "../dsp/EQProcessor.h"
#include "../dsp/CompressorProcessor.h"
#include "../dsp/ExciterProcessor.h"
#include "../dsp/SculptProcessor.h"
#include "../dsp/ReverbProcessor.h"
#include "../dsp/DelayProcessor.h"
#include "../dsp/HarmonizerProcessor.h"
#include "../dsp/DynamicEQProcessor.h"
#include "../dsp/PitchProcessor.h"
#include "../dsp/GateProcessor.h"
#include "../dsp/PreAmpProcessor.h"
#include "../dsp/DeEsserProcessor.h"
#include "../dsp/SaturationProcessor.h"
#include "../dsp/DoublerProcessor.h"
#include "../dsp/RecorderProcessor.h"
#include "../dsp/StudioReverbProcessor.h"
#include "../dsp/MasterProcessor.h"
// #include "../dsp/TunerProcessor.h"   // DISABLED
#include "../dsp/TransientSplitterProcessor.h"

// ==============================================================================
// Base class — shared interface for every effect node
// ==============================================================================
class EffectProcessorNode : public juce::AudioProcessor
{
public:
    EffectProcessorNode (const juce::String& displayName,
                         int mainInChannels, int mainOutChannels,
                         bool withSidechain = false)
        : AudioProcessor (makeBuses (mainInChannels, mainOutChannels, withSidechain)),
          nodeName (displayName),
          hasSidechainBus (withSidechain)
    {}

    // --- AudioProcessor boilerplate ------------------------------------------
    const juce::String getName() const override             { return nodeName; }
    bool   acceptsMidi()  const override                    { return false; }
    bool   producesMidi() const override                    { return false; }
    double getTailLengthSeconds() const override            { return 0.0; }
    int    getNumPrograms() override                        { return 1; }
    int    getCurrentProgram() override                     { return 0; }
    void   setCurrentProgram (int) override                 {}
    const  juce::String getProgramName (int) override       { return {}; }
    void   changeProgramName (int, const juce::String&) override {}

    // We open our own panel windows — not generic JUCE editors
    juce::AudioProcessorEditor* createEditor() override     { return nullptr; }
    bool   hasEditor() const override                       { return false; }

    // State is handled by our own PresetManager, not the graph
    void getStateInformation (juce::MemoryBlock&) override  {}
    void setStateInformation (const void*, int) override    {}

    // --- Subclass contract ---------------------------------------------------
    virtual juce::String getEffectType() const = 0;

    // --- Node category (for canvas theming: "" = studio, "Guitar" = purple) --
    virtual juce::String getNodeCategory() const { return ""; }
    
    // --- Custom node height (override for taller nodes like PreAmp) ----------
    virtual float getCustomNodeHeight() const { return 0.0f; }  // 0 = use default

    // --- Sidechain helpers (mirroring Colosseum's MeteringProcessor) ---------
    bool hasSidechain() const          { return hasSidechainBus; }
    void enableSidechain()             { sidechainActive = true; }
    void disableSidechain()            { sidechainActive = false; }
    bool isSidechainEnabled() const    { return sidechainActive; }

    struct ChannelMapping { int innerChannel; bool isSidechain; };

    ChannelMapping mapInputChannel (int channel) const
    {
        if (hasSidechainBus && channel >= 2)
            return { channel - 2, true };
        return { channel, false };
    }

protected:
    juce::String nodeName;
    bool hasSidechainBus   = false;
    bool sidechainActive   = false;
    double currentSampleRate = 44100.0;
    int    currentBlockSize  = 512;

    static BusesProperties makeBuses (int ins, int outs, bool sc)
    {
        auto props = BusesProperties()
            .withInput  ("Main", juce::AudioChannelSet::canonicalChannelSet (ins),  true)
            .withOutput ("Main", juce::AudioChannelSet::canonicalChannelSet (outs), true);
        if (sc)
            props = props.withInput ("Sidechain", juce::AudioChannelSet::stereo(), true);
        return props;
    }
};

// Guitar nodes (all 12 wrappers + CabIR) — must come after EffectProcessorNode
#include "../guitar/GuitarNodes.h"

// ==============================================================================
//  EQ  (9-band parametric)
// ==============================================================================
class EQProcessorNode : public EffectProcessorNode
{
public:
    EQProcessorNode() : EffectProcessorNode ("EQ", 2, 2) {}
    juce::String getEffectType() const override { return "EQ"; }

    void prepareToPlay (double sr, int bs) override
    {
        currentSampleRate = sr;  currentBlockSize = bs;
        juce::dsp::ProcessSpec spec { sr, (juce::uint32) bs, 2 };
        eq.prepare (spec);
    }
    void releaseResources() override { eq.reset(); }

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        eq.process (ctx);
    }

    bool isBusesLayoutSupported (const BusesLayout& l) const override
    {
        return l.getMainInputChannelSet()  == juce::AudioChannelSet::stereo()
            && l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }

    EQProcessor&       getProcessor()       { return eq; }
    const EQProcessor& getProcessor() const { return eq; }

private:
    EQProcessor eq;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQProcessorNode)
};

// ==============================================================================
//  Gate (noise gate / expander)
// ==============================================================================
class GateProcessorNode : public EffectProcessorNode
{
public:
    GateProcessorNode() : EffectProcessorNode ("Gate", 2, 2) {}
    juce::String getEffectType() const override { return "Gate"; }

    void prepareToPlay (double sr, int bs) override
    {
        currentSampleRate = sr;  currentBlockSize = bs;
        juce::dsp::ProcessSpec spec { sr, (juce::uint32) bs, 2 };
        gate.prepare (spec);
    }
    void releaseResources() override { gate.reset(); }

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        gate.process (buffer);
    }

    bool isBusesLayoutSupported (const BusesLayout& l) const override
    {
        return l.getMainInputChannelSet()  == juce::AudioChannelSet::stereo()
            && l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }

    GateProcessor&       getProcessor()       { return gate; }
    const GateProcessor& getProcessor() const { return gate; }

private:
    GateProcessor gate;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GateProcessorNode)
};

// ==============================================================================
//  Compressor
// ==============================================================================
class CompressorProcessorNode : public EffectProcessorNode
{
public:
    CompressorProcessorNode() : EffectProcessorNode ("Compressor", 2, 2) {}
    juce::String getEffectType() const override { return "Compressor"; }

    void prepareToPlay (double sr, int bs) override
    {
        currentSampleRate = sr;  currentBlockSize = bs;
        juce::dsp::ProcessSpec spec { sr, (juce::uint32) bs, 2 };
        comp.prepare (spec);
    }
    void releaseResources() override { comp.reset(); }

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        comp.process (ctx);
    }

    bool isBusesLayoutSupported (const BusesLayout& l) const override
    {
        return l.getMainInputChannelSet()  == juce::AudioChannelSet::stereo()
            && l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }

    CompressorProcessor&       getProcessor()       { return comp; }
    const CompressorProcessor& getProcessor() const { return comp; }

private:
    CompressorProcessor comp;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompressorProcessorNode)
};

// ==============================================================================
//  Exciter (AIR)
// ==============================================================================
class ExciterProcessorNode : public EffectProcessorNode
{
public:
    ExciterProcessorNode() : EffectProcessorNode ("Exciter", 2, 2) {}
    juce::String getEffectType() const override { return "Exciter"; }

    void prepareToPlay (double sr, int bs) override
    {
        currentSampleRate = sr;  currentBlockSize = bs;
        juce::dsp::ProcessSpec spec { sr, (juce::uint32) bs, 2 };
        exc.prepare (spec);
    }
    void releaseResources() override { exc.reset(); }

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        exc.process (ctx);
    }

    bool isBusesLayoutSupported (const BusesLayout& l) const override
    {
        return l.getMainInputChannelSet()  == juce::AudioChannelSet::stereo()
            && l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }

    ExciterProcessor&       getProcessor()       { return exc; }
    const ExciterProcessor& getProcessor() const { return exc; }

private:
    ExciterProcessor exc;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ExciterProcessorNode)
};

// ==============================================================================
//  Sculpt
// ==============================================================================
class SculptProcessorNode : public EffectProcessorNode
{
public:
    SculptProcessorNode() : EffectProcessorNode ("Sculpt", 2, 2) {}
    juce::String getEffectType() const override { return "Sculpt"; }

    void prepareToPlay (double sr, int bs) override
    {
        currentSampleRate = sr;  currentBlockSize = bs;
        juce::dsp::ProcessSpec spec { sr, (juce::uint32) bs, 2 };
        sculpt.prepare (spec);
    }
    void releaseResources() override { sculpt.reset(); }

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        sculpt.process (ctx);
    }

    bool isBusesLayoutSupported (const BusesLayout& l) const override
    {
        return l.getMainInputChannelSet()  == juce::AudioChannelSet::stereo()
            && l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }

    SculptProcessor&       getProcessor()       { return sculpt; }
    const SculptProcessor& getProcessor() const { return sculpt; }

private:
    SculptProcessor sculpt;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SculptProcessorNode)
};

// ==============================================================================
//  Reverb (algorithmic — uses AudioBuffer directly)
// ==============================================================================
class ReverbProcessorNode : public EffectProcessorNode
{
public:
    ReverbProcessorNode() : EffectProcessorNode ("Reverb", 2, 2) {}
    juce::String getEffectType() const override { return "Reverb"; }

    void prepareToPlay (double sr, int bs) override
    {
        currentSampleRate = sr;  currentBlockSize = bs;
        juce::dsp::ProcessSpec spec { sr, (juce::uint32) bs, 2 };
        reverb.prepare (spec);
    }
    void releaseResources() override { reverb.reset(); }

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        reverb.process (buffer);
    }

    bool isBusesLayoutSupported (const BusesLayout& l) const override
    {
        return l.getMainInputChannelSet()  == juce::AudioChannelSet::stereo()
            && l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }

    ReverbProcessor&       getProcessor()       { return reverb; }
    const ReverbProcessor& getProcessor() const { return reverb; }

private:
    ReverbProcessor reverb;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReverbProcessorNode)
};

// ==============================================================================
//  Delay  (uses AudioBuffer API)
// ==============================================================================
class DelayProcessorNode : public EffectProcessorNode
{
public:
    DelayProcessorNode() : EffectProcessorNode ("Delay", 2, 2) {}
    juce::String getEffectType() const override { return "Delay"; }

    void prepareToPlay (double sr, int bs) override
    {
        currentSampleRate = sr;  currentBlockSize = bs;
        dly.prepare (sr, bs, 2);
    }
    void releaseResources() override { dly.reset(); }

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        dly.process (buffer);
    }

    bool isBusesLayoutSupported (const BusesLayout& l) const override
    {
        return l.getMainInputChannelSet()  == juce::AudioChannelSet::stereo()
            && l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }

    DelayProcessor&       getProcessor()       { return dly; }
    const DelayProcessor& getProcessor() const { return dly; }

private:
    DelayProcessor dly;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DelayProcessorNode)
};

// ==============================================================================
//  Harmonizer  (processes mono ch0, adds harmony to stereo)
// ==============================================================================
class HarmonizerProcessorNode : public EffectProcessorNode
{
public:
    HarmonizerProcessorNode() : EffectProcessorNode ("Harmonizer", 2, 2) {}
    juce::String getEffectType() const override { return "Harmonizer"; }

    void prepareToPlay (double sr, int bs) override
    {
        currentSampleRate = sr;  currentBlockSize = bs;
        juce::dsp::ProcessSpec spec { sr, (juce::uint32) bs, 2 };
        harm.prepare (spec);
    }
    void releaseResources() override { harm.reset(); }

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        harm.process (ctx);
    }

    bool isBusesLayoutSupported (const BusesLayout& l) const override
    {
        return l.getMainInputChannelSet()  == juce::AudioChannelSet::stereo()
            && l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }

    HarmonizerProcessor&       getProcessor()       { return harm; }
    const HarmonizerProcessor& getProcessor() const { return harm; }

private:
    HarmonizerProcessor harm;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HarmonizerProcessorNode)
};

// ==============================================================================
//  Dynamic EQ  (with sidechain — green pins for vocal ducking)
// ==============================================================================
class DynamicEQProcessorNode : public EffectProcessorNode
{
public:
    DynamicEQProcessorNode() : EffectProcessorNode ("Dynamic EQ", 2, 2, true) 
    {
        getBus(true, 1)->enable(true);
    }
    juce::String getEffectType() const override { return "DynamicEQ"; }

    void prepareToPlay (double sr, int bs) override
    {
        currentSampleRate = sr;  currentBlockSize = bs;
        juce::dsp::ProcessSpec spec { sr, (juce::uint32) bs, 2 };
        dynEQ.prepare (spec);
    }
    void releaseResources() override { dynEQ.reset(); }

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        if (buffer.getNumChannels() > 2)
        {
            juce::AudioBuffer<float> mainBuf (buffer.getArrayOfWritePointers(), 2,
                                               buffer.getNumSamples());
            const float* scPtrs[2] = {
                buffer.getReadPointer (2),
                buffer.getNumChannels() > 3 ? buffer.getReadPointer (3)
                                            : buffer.getReadPointer (2)
            };
            juce::AudioBuffer<float> scBuf (const_cast<float**>(scPtrs), 2,
                                             buffer.getNumSamples());
            dynEQ.process (mainBuf, scBuf);
        }
        else
        {
            juce::AudioBuffer<float> silentSc (2, buffer.getNumSamples());
            silentSc.clear();
            dynEQ.process (buffer, silentSc);
        }
    }

    bool isBusesLayoutSupported (const BusesLayout& l) const override
    {
        if (l.getMainInputChannelSet()  != juce::AudioChannelSet::stereo()) return false;
        if (l.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
        auto sc = l.getChannelSet (true, 1);
        return sc == juce::AudioChannelSet::stereo();
    }

    DynamicEQProcessor&       getProcessor()       { return dynEQ; }
    const DynamicEQProcessor& getProcessor() const { return dynEQ; }

private:
    DynamicEQProcessor dynEQ;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DynamicEQProcessorNode)
};

// ==============================================================================
//  Pitch Processor
// ==============================================================================
class PitchProcessorNode : public EffectProcessorNode
{
public:
    PitchProcessorNode() : EffectProcessorNode ("Pitch", 2, 2) {}
    juce::String getEffectType() const override { return "Pitch"; }

    void prepareToPlay (double sr, int bs) override
    {
        currentSampleRate = sr;
        currentBlockSize = bs;
        juce::dsp::ProcessSpec spec { sr, (juce::uint32) bs, 2 };
        pitch.prepare (spec);
    }
    
    void releaseResources() override { pitch.reset(); }

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        pitch.process (buffer);
    }

    bool isBusesLayoutSupported (const BusesLayout& l) const override
    {
        return l.getMainInputChannelSet()  == juce::AudioChannelSet::stereo()
            && l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }

    PitchProcessor&       getProcessor()       { return pitch; }
    const PitchProcessor& getProcessor() const { return pitch; }

private:
    PitchProcessor pitch;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchProcessorNode)
};

// ==============================================================================
//  PreAmp (tall node with gain/tone controls)
// ==============================================================================
class PreAmpProcessorNode : public EffectProcessorNode
{
public:
    PreAmpProcessorNode() : EffectProcessorNode ("PreAmp", 2, 2) {}
    juce::String getEffectType() const override { return "PreAmp"; }
    
    float getCustomNodeHeight() const override { return 240.0f; }

    void prepareToPlay (double sr, int bs) override
    {
        currentSampleRate = sr;  currentBlockSize = bs;
        juce::dsp::ProcessSpec spec { sr, (juce::uint32) bs, 2 };
        preamp.prepare (spec);
    }
    void releaseResources() override { preamp.reset(); }

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        preamp.process (buffer);
    }

    bool isBusesLayoutSupported (const BusesLayout& l) const override
    {
        return l.getMainInputChannelSet()  == juce::AudioChannelSet::stereo()
            && l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }

    PreAmpProcessor&       getProcessor()       { return preamp; }
    const PreAmpProcessor& getProcessor() const { return preamp; }

private:
    PreAmpProcessor preamp;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PreAmpProcessorNode)
};

// ==============================================================================
//  DeEsser (sibilance reduction)
// ==============================================================================
class DeEsserProcessorNode : public EffectProcessorNode
{
public:
    DeEsserProcessorNode() : EffectProcessorNode ("DeEsser", 2, 2) {}
    juce::String getEffectType() const override { return "DeEsser"; }

    void prepareToPlay (double sr, int bs) override
    {
        currentSampleRate = sr;  currentBlockSize = bs;
        juce::dsp::ProcessSpec spec { sr, (juce::uint32) bs, 2 };
        deesser.prepare (spec);
    }
    void releaseResources() override { deesser.reset(); }

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        deesser.process (buffer);
    }

    bool isBusesLayoutSupported (const BusesLayout& l) const override
    {
        return l.getMainInputChannelSet()  == juce::AudioChannelSet::stereo()
            && l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }

    DeEsserProcessor&       getProcessor()       { return deesser; }
    const DeEsserProcessor& getProcessor() const { return deesser; }

private:
    DeEsserProcessor deesser;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeEsserProcessorNode)
};

// ==============================================================================
//  Saturation (analog warmth / distortion)
// ==============================================================================
class SaturationProcessorNode : public EffectProcessorNode
{
public:
    SaturationProcessorNode() : EffectProcessorNode ("Saturation", 2, 2) {}
    juce::String getEffectType() const override { return "Saturation"; }

    void prepareToPlay (double sr, int bs) override
    {
        currentSampleRate = sr;  currentBlockSize = bs;
        juce::dsp::ProcessSpec spec { sr, (juce::uint32) bs, 2 };
        saturation.prepare (spec);
    }
    void releaseResources() override { saturation.reset(); }

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        saturation.process (buffer);
    }

    bool isBusesLayoutSupported (const BusesLayout& l) const override
    {
        return l.getMainInputChannelSet()  == juce::AudioChannelSet::stereo()
            && l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }

    SaturationProcessor&       getProcessor()       { return saturation; }
    const SaturationProcessor& getProcessor() const { return saturation; }

private:
    SaturationProcessor saturation;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SaturationProcessorNode)
};

// ==============================================================================
//  Doubler (Voice doubling with formant shift)
// ==============================================================================
class DoublerProcessorNode : public EffectProcessorNode
{
public:
    DoublerProcessorNode() : EffectProcessorNode ("Doubler", 2, 2) {}
    juce::String getEffectType() const override { return "Doubler"; }

    void prepareToPlay (double sr, int bs) override
    {
        currentSampleRate = sr;  currentBlockSize = bs;
        juce::dsp::ProcessSpec spec { sr, (juce::uint32) bs, 2 };
        doubler.prepare (spec);
    }
    void releaseResources() override { doubler.reset(); }

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        doubler.process (buffer);
    }

    bool isBusesLayoutSupported (const BusesLayout& l) const override
    {
        return l.getMainInputChannelSet()  == juce::AudioChannelSet::stereo()
            && l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }

    DoublerProcessor&       getProcessor()       { return doubler; }
    const DoublerProcessor& getProcessor() const { return doubler; }

private:
    DoublerProcessor doubler;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DoublerProcessorNode)
};

// ==============================================================================
//  Recorder (termination point - no output, records to disk)
// ==============================================================================
class RecorderProcessorNode : public EffectProcessorNode
{
public:
    RecorderProcessorNode()
        : EffectProcessorNode ("Recorder", 2, 0)
    {
    }

    juce::String getEffectType() const override { return "Recorder"; }

    void prepareToPlay (double sr, int bs) override
    {
        currentSampleRate = sr;  currentBlockSize = bs;
        recorder.prepareToPlay (sr, bs);
    }

    void releaseResources() override
    {
        recorder.releaseResources();
    }

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override
    {
        recorder.processBlock (buffer, midi);
    }

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override
    {
        return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
            && layouts.getMainOutputChannelSet().isDisabled();
    }

    void getStateInformation (juce::MemoryBlock& destData) override
    {
        recorder.getStateInformation (destData);
    }

    void setStateInformation (const void* data, int sizeInBytes) override
    {
        recorder.setStateInformation (data, sizeInBytes);
    }

    RecorderProcessor& getProcessor() { return recorder; }
    const RecorderProcessor& getProcessor() const { return recorder; }

private:
    RecorderProcessor recorder;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RecorderProcessorNode)
};

// ==============================================================================
//  Tuner — DISABLED (detection not production-ready)
// ==============================================================================
// class TunerProcessorNode : public EffectProcessorNode { ... };

// ==============================================================================
//  Studio Reverb (Dattorro Progenitor — separate node from convolution reverb)
// ==============================================================================
class StudioReverbProcessorNode : public EffectProcessorNode
{
public:
    StudioReverbProcessorNode() : EffectProcessorNode ("Studio Reverb", 2, 2) {}
    juce::String getEffectType() const override { return "StudioReverb"; }

    void prepareToPlay (double sr, int bs) override
    {
        currentSampleRate = sr;  currentBlockSize = bs;
        juce::dsp::ProcessSpec spec { sr, (juce::uint32) bs, 2 };
        studioReverb.prepare (spec);
    }
    void releaseResources() override { studioReverb.reset(); }

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        studioReverb.process (buffer);
    }

    bool isBusesLayoutSupported (const BusesLayout& l) const override
    {
        return l.getMainInputChannelSet()  == juce::AudioChannelSet::stereo()
            && l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }

    StudioReverbProcessor&       getProcessor()       { return studioReverb; }
    const StudioReverbProcessor& getProcessor() const { return studioReverb; }

private:
    StudioReverbProcessor studioReverb;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StudioReverbProcessorNode)
};

// ==============================================================================
//  Master (Real-time mastering chain — Airwindows DSP)
// ==============================================================================
class MasterProcessorNode : public EffectProcessorNode
{
public:
    MasterProcessorNode() : EffectProcessorNode ("Master", 2, 2) {}
    juce::String getEffectType() const override { return "Master"; }

    void prepareToPlay (double sr, int bs) override
    {
        currentSampleRate = sr;  currentBlockSize = bs;
        juce::dsp::ProcessSpec spec { sr, (juce::uint32) bs, 2 };
        master.prepare (spec);
    }
    void releaseResources() override { master.reset(); }

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        master.process (buffer);
    }

    bool isBusesLayoutSupported (const BusesLayout& l) const override
    {
        return l.getMainInputChannelSet()  == juce::AudioChannelSet::stereo()
            && l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }

    MasterProcessor&       getProcessor()       { return master; }
    const MasterProcessor& getProcessor() const { return master; }

private:
    MasterProcessor master;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MasterProcessorNode)
};

// ==============================================================================
//  Transient Splitter (envelope-based transient/sustain separator)
// ==============================================================================
class TransientSplitterNode : public EffectProcessorNode
{
public:
    TransientSplitterNode() : EffectProcessorNode ("Transient Splitter", 2, 4) {}
    juce::String getEffectType() const override { return "TransientSplitter"; }

    void prepareToPlay (double sr, int bs) override
    {
        currentSampleRate = sr;  currentBlockSize = bs;
        splitter.prepare (sr, bs);
    }
    void releaseResources() override { splitter.reset(); }

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        splitter.process (buffer);
    }

    bool isBusesLayoutSupported (const BusesLayout& l) const override
    {
        // 2-in stereo, 4-out discrete (transient L/R + sustain L/R)
        auto inSet  = l.getMainInputChannelSet();
        auto outSet = l.getMainOutputChannelSet();

        if (inSet == juce::AudioChannelSet::stereo()
            && outSet == juce::AudioChannelSet::discreteChannels (4))
            return true;

        // Also accept if graph gives unified 4-ch buffer
        if (inSet.size() >= 2 && outSet.size() >= 4)
            return true;

        return false;
    }

    void getStateInformation (juce::MemoryBlock& dest) override { splitter.getState (dest); }
    void setStateInformation (const void* data, int size) override { splitter.setState (data, size); }

    TransientSplitterProcessor&       getProcessor()       { return splitter; }
    const TransientSplitterProcessor& getProcessor() const { return splitter; }

private:
    TransientSplitterProcessor splitter;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransientSplitterNode)
};

// ==============================================================================
//  Factory — create an effect node by type string
// ==============================================================================
inline std::unique_ptr<EffectProcessorNode> createEffectNode (const juce::String& type)
{
    // Studio effects
    if (type == "EQ")           return std::make_unique<EQProcessorNode>();
    if (type == "Compressor")   return std::make_unique<CompressorProcessorNode>();
    if (type == "Gate")         return std::make_unique<GateProcessorNode>();
    if (type == "Exciter")      return std::make_unique<ExciterProcessorNode>();
    if (type == "Sculpt")       return std::make_unique<SculptProcessorNode>();
    if (type == "Reverb")       return std::make_unique<ReverbProcessorNode>();
    if (type == "Delay")        return std::make_unique<DelayProcessorNode>();
    if (type == "Harmonizer")   return std::make_unique<HarmonizerProcessorNode>();
    if (type == "DynamicEQ")    return std::make_unique<DynamicEQProcessorNode>();
    if (type == "Pitch")        return std::make_unique<PitchProcessorNode>();
    if (type == "PreAmp")       return std::make_unique<PreAmpProcessorNode>();
    if (type == "DeEsser")      return std::make_unique<DeEsserProcessorNode>();
    if (type == "Saturation")   return std::make_unique<SaturationProcessorNode>();
    if (type == "Doubler")      return std::make_unique<DoublerProcessorNode>();
    if (type == "Recorder")     return std::make_unique<RecorderProcessorNode>();
    // if (type == "Tuner")        return std::make_unique<TunerProcessorNode>();  // DISABLED
    if (type == "StudioReverb") return std::make_unique<StudioReverbProcessorNode>();
    if (type == "Master")      return std::make_unique<MasterProcessorNode>();
    if (type == "TransientSplitter") return std::make_unique<TransientSplitterNode>();

    // Guitar effects
    if (type == "GuitarOverdrive")  return std::make_unique<OverdriveProcessorNode>();
    if (type == "GuitarDistortion") return std::make_unique<DistortionProcessorNode>();
    if (type == "GuitarFuzz")       return std::make_unique<FuzzProcessorNode>();
    if (type == "GuitarChorus")     return std::make_unique<GuitarChorusProcessorNode>();
    if (type == "GuitarFlanger")    return std::make_unique<GuitarFlangerProcessorNode>();
    if (type == "GuitarPhaser")     return std::make_unique<GuitarPhaserProcessorNode>();
    if (type == "GuitarTremolo")    return std::make_unique<GuitarTremoloProcessorNode>();
    if (type == "GuitarVibrato")    return std::make_unique<GuitarVibratoProcessorNode>();
    if (type == "GuitarTone")       return std::make_unique<GuitarToneProcessorNode>();
    if (type == "GuitarRotary")     return std::make_unique<GuitarRotaryProcessorNode>();
    if (type == "GuitarWah")        return std::make_unique<GuitarWahProcessorNode>();
    if (type == "GuitarReverb")     return std::make_unique<GuitarReverbProcessorNode>();
    if (type == "GuitarNoiseGate")  return std::make_unique<GuitarNoiseGateProcessorNode>();
    if (type == "GuitarToneStack")  return std::make_unique<ToneStackProcessorNode>();
    if (type == "GuitarCabSim")     return std::make_unique<CabSimProcessorNode>();
    if (type == "GuitarCabIR")      return std::make_unique<CabIRProcessorNode>();

    return nullptr;
}

// Full list for menus
inline juce::StringArray getAvailableEffectTypes()
{
    return {
        // Studio
        "PreAmp", "Gate", "EQ", "Compressor", "Exciter", "Sculpt",
        "Reverb", "StudioReverb", "Delay", "Harmonizer", "DynamicEQ", "Pitch",
        "DeEsser", "Saturation", "Doubler", "Recorder", "Master", "TransientSplitter",
        // Guitar
        "GuitarOverdrive", "GuitarDistortion", "GuitarFuzz",
        "GuitarChorus", "GuitarFlanger", "GuitarPhaser", "GuitarTremolo",
        "GuitarVibrato", "GuitarTone", "GuitarRotary", "GuitarWah",
        "GuitarReverb", "GuitarNoiseGate", "GuitarToneStack", "GuitarCabSim",
        "GuitarCabIR"
    };
}
