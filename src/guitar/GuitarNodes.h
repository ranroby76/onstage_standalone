// ==============================================================================
//  GuitarNodes.h
//  OnStage — Node wrappers for all Guitar DSP processors
//
//  Each wrapper makes a guitar DSP processor usable as a node inside
//  juce::AudioProcessorGraph.  All guitar nodes return "Guitar" category
//  via getNodeCategory() for deep purple rendering on canvas.
//
//  Include this from EffectNodes.h after the existing effect nodes.
// ==============================================================================

#pragma once

#include "../guitar/OverdriveProcessor.h"
#include "../guitar/DistortionProcessor.h"
#include "../guitar/FuzzProcessor.h"
#include "../guitar/GuitarChorusProcessor.h"
#include "../guitar/GuitarFlangerProcessor.h"
#include "../guitar/GuitarPhaserProcessor.h"
#include "../guitar/GuitarTremoloProcessor.h"
#include "../guitar/GuitarVibratoProcessor.h"
#include "../guitar/GuitarToneProcessor.h"
#include "../guitar/GuitarRotaryProcessor.h"
#include "../guitar/GuitarWahProcessor.h"
#include "../guitar/GuitarReverbProcessor.h"
#include "../guitar/GuitarNoiseGateProcessor.h"
#include "../guitar/ToneStackProcessor.h"
#include "../guitar/CabSimProcessor.h"
#include "../guitar/CabIRProcessor.h"

// ==============================================================================
//  Overdrive
// ==============================================================================
class OverdriveProcessorNode : public EffectProcessorNode
{
public:
    OverdriveProcessorNode() : EffectProcessorNode ("Overdrive", 2, 2) {}
    juce::String getEffectType() const override { return "GuitarOverdrive"; }
    juce::String getNodeCategory() const { return "Guitar"; }

    void prepareToPlay (double sr, int bs) override
    {
        currentSampleRate = sr; currentBlockSize = bs;
        juce::dsp::ProcessSpec spec { sr, (juce::uint32) bs, 2 };
        proc.prepare (spec);
    }
    void releaseResources() override { proc.reset(); }
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override { proc.process (buffer); }

    bool isBusesLayoutSupported (const BusesLayout& l) const override
    {
        return l.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
            && l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }

    OverdriveProcessor&       getProcessor()       { return proc; }
    const OverdriveProcessor& getProcessor() const { return proc; }

private:
    OverdriveProcessor proc;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OverdriveProcessorNode)
};

// ==============================================================================
//  Distortion
// ==============================================================================
class DistortionProcessorNode : public EffectProcessorNode
{
public:
    DistortionProcessorNode() : EffectProcessorNode ("Distortion", 2, 2) {}
    juce::String getEffectType() const override { return "GuitarDistortion"; }
    juce::String getNodeCategory() const { return "Guitar"; }

    void prepareToPlay (double sr, int bs) override
    {
        currentSampleRate = sr; currentBlockSize = bs;
        juce::dsp::ProcessSpec spec { sr, (juce::uint32) bs, 2 };
        proc.prepare (spec);
    }
    void releaseResources() override { proc.reset(); }
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override { proc.process (buffer); }

    bool isBusesLayoutSupported (const BusesLayout& l) const override
    {
        return l.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
            && l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }

    DistortionProcessor&       getProcessor()       { return proc; }
    const DistortionProcessor& getProcessor() const { return proc; }

private:
    DistortionProcessor proc;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DistortionProcessorNode)
};

// ==============================================================================
//  Fuzz
// ==============================================================================
class FuzzProcessorNode : public EffectProcessorNode
{
public:
    FuzzProcessorNode() : EffectProcessorNode ("Fuzz", 2, 2) {}
    juce::String getEffectType() const override { return "GuitarFuzz"; }
    juce::String getNodeCategory() const { return "Guitar"; }

    void prepareToPlay (double sr, int bs) override
    {
        currentSampleRate = sr; currentBlockSize = bs;
        juce::dsp::ProcessSpec spec { sr, (juce::uint32) bs, 2 };
        proc.prepare (spec);
    }
    void releaseResources() override { proc.reset(); }
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override { proc.process (buffer); }

    bool isBusesLayoutSupported (const BusesLayout& l) const override
    {
        return l.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
            && l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }

    FuzzProcessor&       getProcessor()       { return proc; }
    const FuzzProcessor& getProcessor() const { return proc; }

private:
    FuzzProcessor proc;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FuzzProcessorNode)
};

// ==============================================================================
//  Guitar Chorus
// ==============================================================================
class GuitarChorusProcessorNode : public EffectProcessorNode
{
public:
    GuitarChorusProcessorNode() : EffectProcessorNode ("Guitar Chorus", 2, 2) {}
    juce::String getEffectType() const override { return "GuitarChorus"; }
    juce::String getNodeCategory() const { return "Guitar"; }

    void prepareToPlay (double sr, int bs) override
    {
        currentSampleRate = sr; currentBlockSize = bs;
        juce::dsp::ProcessSpec spec { sr, (juce::uint32) bs, 2 };
        proc.prepare (spec);
    }
    void releaseResources() override { proc.reset(); }
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override { proc.process (buffer); }

    bool isBusesLayoutSupported (const BusesLayout& l) const override
    {
        return l.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
            && l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }

    GuitarChorusProcessor&       getProcessor()       { return proc; }
    const GuitarChorusProcessor& getProcessor() const { return proc; }

private:
    GuitarChorusProcessor proc;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GuitarChorusProcessorNode)
};

// ==============================================================================
//  Guitar Flanger
// ==============================================================================
class GuitarFlangerProcessorNode : public EffectProcessorNode
{
public:
    GuitarFlangerProcessorNode() : EffectProcessorNode ("Guitar Flanger", 2, 2) {}
    juce::String getEffectType() const override { return "GuitarFlanger"; }
    juce::String getNodeCategory() const { return "Guitar"; }

    void prepareToPlay (double sr, int bs) override
    {
        currentSampleRate = sr; currentBlockSize = bs;
        juce::dsp::ProcessSpec spec { sr, (juce::uint32) bs, 2 };
        proc.prepare (spec);
    }
    void releaseResources() override { proc.reset(); }
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override { proc.process (buffer); }

    bool isBusesLayoutSupported (const BusesLayout& l) const override
    {
        return l.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
            && l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }

    GuitarFlangerProcessor&       getProcessor()       { return proc; }
    const GuitarFlangerProcessor& getProcessor() const { return proc; }

private:
    GuitarFlangerProcessor proc;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GuitarFlangerProcessorNode)
};

// ==============================================================================
//  Guitar Phaser
// ==============================================================================
class GuitarPhaserProcessorNode : public EffectProcessorNode
{
public:
    GuitarPhaserProcessorNode() : EffectProcessorNode ("Guitar Phaser", 2, 2) {}
    juce::String getEffectType() const override { return "GuitarPhaser"; }
    juce::String getNodeCategory() const { return "Guitar"; }

    void prepareToPlay (double sr, int bs) override
    {
        currentSampleRate = sr; currentBlockSize = bs;
        juce::dsp::ProcessSpec spec { sr, (juce::uint32) bs, 2 };
        proc.prepare (spec);
    }
    void releaseResources() override { proc.reset(); }
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override { proc.process (buffer); }

    bool isBusesLayoutSupported (const BusesLayout& l) const override
    {
        return l.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
            && l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }

    GuitarPhaserProcessor&       getProcessor()       { return proc; }
    const GuitarPhaserProcessor& getProcessor() const { return proc; }

private:
    GuitarPhaserProcessor proc;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GuitarPhaserProcessorNode)
};

// ==============================================================================
//  Guitar Tremolo
// ==============================================================================
class GuitarTremoloProcessorNode : public EffectProcessorNode
{
public:
    GuitarTremoloProcessorNode() : EffectProcessorNode ("Guitar Tremolo", 2, 2) {}
    juce::String getEffectType() const override { return "GuitarTremolo"; }
    juce::String getNodeCategory() const { return "Guitar"; }

    void prepareToPlay (double sr, int bs) override
    {
        currentSampleRate = sr; currentBlockSize = bs;
        juce::dsp::ProcessSpec spec { sr, (juce::uint32) bs, 2 };
        proc.prepare (spec);
    }
    void releaseResources() override { proc.reset(); }
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override { proc.process (buffer); }

    bool isBusesLayoutSupported (const BusesLayout& l) const override
    {
        return l.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
            && l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }

    GuitarTremoloProcessor&       getProcessor()       { return proc; }
    const GuitarTremoloProcessor& getProcessor() const { return proc; }

private:
    GuitarTremoloProcessor proc;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GuitarTremoloProcessorNode)
};

// ==============================================================================
//  Guitar Reverb (Freeverb)
// ==============================================================================
class GuitarReverbProcessorNode : public EffectProcessorNode
{
public:
    GuitarReverbProcessorNode() : EffectProcessorNode ("Guitar Reverb", 2, 2) {}
    juce::String getEffectType() const override { return "GuitarReverb"; }
    juce::String getNodeCategory() const { return "Guitar"; }

    void prepareToPlay (double sr, int bs) override
    {
        currentSampleRate = sr; currentBlockSize = bs;
        juce::dsp::ProcessSpec spec { sr, (juce::uint32) bs, 2 };
        proc.prepare (spec);
    }
    void releaseResources() override { proc.reset(); }
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override { proc.process (buffer); }

    bool isBusesLayoutSupported (const BusesLayout& l) const override
    {
        return l.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
            && l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }

    GuitarReverbProcessor&       getProcessor()       { return proc; }
    const GuitarReverbProcessor& getProcessor() const { return proc; }

private:
    GuitarReverbProcessor proc;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GuitarReverbProcessorNode)
};

// ==============================================================================
//  Guitar Noise Gate
// ==============================================================================
class GuitarNoiseGateProcessorNode : public EffectProcessorNode
{
public:
    GuitarNoiseGateProcessorNode() : EffectProcessorNode ("Guitar Gate", 2, 2) {}
    juce::String getEffectType() const override { return "GuitarNoiseGate"; }
    juce::String getNodeCategory() const { return "Guitar"; }

    void prepareToPlay (double sr, int bs) override
    {
        currentSampleRate = sr; currentBlockSize = bs;
        juce::dsp::ProcessSpec spec { sr, (juce::uint32) bs, 2 };
        proc.prepare (spec);
    }
    void releaseResources() override { proc.reset(); }
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override { proc.process (buffer); }

    bool isBusesLayoutSupported (const BusesLayout& l) const override
    {
        return l.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
            && l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }

    GuitarNoiseGateProcessor&       getProcessor()       { return proc; }
    const GuitarNoiseGateProcessor& getProcessor() const { return proc; }

private:
    GuitarNoiseGateProcessor proc;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GuitarNoiseGateProcessorNode)
};

// ==============================================================================
//  Tone Stack
// ==============================================================================
class ToneStackProcessorNode : public EffectProcessorNode
{
public:
    ToneStackProcessorNode() : EffectProcessorNode ("Tone Stack", 2, 2) {}
    juce::String getEffectType() const override { return "GuitarToneStack"; }
    juce::String getNodeCategory() const { return "Guitar"; }

    void prepareToPlay (double sr, int bs) override
    {
        currentSampleRate = sr; currentBlockSize = bs;
        juce::dsp::ProcessSpec spec { sr, (juce::uint32) bs, 2 };
        proc.prepare (spec);
    }
    void releaseResources() override { proc.reset(); }
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override { proc.process (buffer); }

    bool isBusesLayoutSupported (const BusesLayout& l) const override
    {
        return l.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
            && l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }

    ToneStackProcessor&       getProcessor()       { return proc; }
    const ToneStackProcessor& getProcessor() const { return proc; }

private:
    ToneStackProcessor proc;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ToneStackProcessorNode)
};

// ==============================================================================
//  Cabinet Simulator
// ==============================================================================
class CabSimProcessorNode : public EffectProcessorNode
{
public:
    CabSimProcessorNode() : EffectProcessorNode ("Cab Sim", 2, 2) {}
    juce::String getEffectType() const override { return "GuitarCabSim"; }
    juce::String getNodeCategory() const { return "Guitar"; }

    void prepareToPlay (double sr, int bs) override
    {
        currentSampleRate = sr; currentBlockSize = bs;
        juce::dsp::ProcessSpec spec { sr, (juce::uint32) bs, 2 };
        proc.prepare (spec);
    }
    void releaseResources() override { proc.reset(); }
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override { proc.process (buffer); }

    bool isBusesLayoutSupported (const BusesLayout& l) const override
    {
        return l.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
            && l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }

    CabSimProcessor&       getProcessor()       { return proc; }
    const CabSimProcessor& getProcessor() const { return proc; }

private:
    CabSimProcessor proc;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CabSimProcessorNode)
};

// ==============================================================================
//  Guitar Vibrato
// ==============================================================================
class GuitarVibratoProcessorNode : public EffectProcessorNode
{
public:
    GuitarVibratoProcessorNode() : EffectProcessorNode ("Vibrato", 2, 2) {}
    juce::String getEffectType() const override { return "GuitarVibrato"; }
    juce::String getNodeCategory() const override { return "Guitar"; }

    void prepareToPlay (double sr, int bs) override
    {
        currentSampleRate = sr; currentBlockSize = bs;
        juce::dsp::ProcessSpec spec { sr, (juce::uint32) bs, 2 };
        proc.prepare (spec);
    }
    void releaseResources() override { proc.reset(); }
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override { proc.process (buffer); }

    bool isBusesLayoutSupported (const BusesLayout& l) const override
    {
        return l.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
            && l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }

    GuitarVibratoProcessor&       getProcessor()       { return proc; }
    const GuitarVibratoProcessor& getProcessor() const { return proc; }

private:
    GuitarVibratoProcessor proc;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GuitarVibratoProcessorNode)
};

// ==============================================================================
//  Guitar Tone (Baxandall EQ)
// ==============================================================================
class GuitarToneProcessorNode : public EffectProcessorNode
{
public:
    GuitarToneProcessorNode() : EffectProcessorNode ("Tone", 2, 2) {}
    juce::String getEffectType() const override { return "GuitarTone"; }
    juce::String getNodeCategory() const override { return "Guitar"; }

    void prepareToPlay (double sr, int bs) override
    {
        currentSampleRate = sr; currentBlockSize = bs;
        juce::dsp::ProcessSpec spec { sr, (juce::uint32) bs, 2 };
        proc.prepare (spec);
    }
    void releaseResources() override { proc.reset(); }
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override { proc.process (buffer); }

    bool isBusesLayoutSupported (const BusesLayout& l) const override
    {
        return l.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
            && l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }

    GuitarToneProcessor&       getProcessor()       { return proc; }
    const GuitarToneProcessor& getProcessor() const { return proc; }

private:
    GuitarToneProcessor proc;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GuitarToneProcessorNode)
};

// ==============================================================================
//  Guitar Rotary Speaker
// ==============================================================================
class GuitarRotaryProcessorNode : public EffectProcessorNode
{
public:
    GuitarRotaryProcessorNode() : EffectProcessorNode ("Rotary Speaker", 2, 2) {}
    juce::String getEffectType() const override { return "GuitarRotary"; }
    juce::String getNodeCategory() const override { return "Guitar"; }

    void prepareToPlay (double sr, int bs) override
    {
        currentSampleRate = sr; currentBlockSize = bs;
        juce::dsp::ProcessSpec spec { sr, (juce::uint32) bs, 2 };
        proc.prepare (spec);
    }
    void releaseResources() override { proc.reset(); }
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override { proc.process (buffer); }

    bool isBusesLayoutSupported (const BusesLayout& l) const override
    {
        return l.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
            && l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }

    GuitarRotaryProcessor&       getProcessor()       { return proc; }
    const GuitarRotaryProcessor& getProcessor() const { return proc; }

private:
    GuitarRotaryProcessor proc;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GuitarRotaryProcessorNode)
};

// ==============================================================================
//  Guitar Wah
// ==============================================================================
class GuitarWahProcessorNode : public EffectProcessorNode
{
public:
    GuitarWahProcessorNode() : EffectProcessorNode ("Wah", 2, 2) {}
    juce::String getEffectType() const override { return "GuitarWah"; }
    juce::String getNodeCategory() const override { return "Guitar"; }

    void prepareToPlay (double sr, int bs) override
    {
        currentSampleRate = sr; currentBlockSize = bs;
        juce::dsp::ProcessSpec spec { sr, (juce::uint32) bs, 2 };
        proc.prepare (spec);
    }
    void releaseResources() override { proc.reset(); }
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override { proc.process (buffer); }

    bool isBusesLayoutSupported (const BusesLayout& l) const override
    {
        return l.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
            && l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }

    GuitarWahProcessor&       getProcessor()       { return proc; }
    const GuitarWahProcessor& getProcessor() const { return proc; }

private:
    GuitarWahProcessor proc;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GuitarWahProcessorNode)
};

// ==============================================================================
//  Cabinet IR (Convolution)
// ==============================================================================
class CabIRProcessorNode : public EffectProcessorNode
{
public:
    CabIRProcessorNode() : EffectProcessorNode ("Cab IR", 2, 2) {}
    juce::String getEffectType() const override { return "GuitarCabIR"; }
    juce::String getNodeCategory() const { return "Guitar"; }

    void prepareToPlay (double sr, int bs) override
    {
        currentSampleRate = sr; currentBlockSize = bs;
        juce::dsp::ProcessSpec spec { sr, (juce::uint32) bs, 2 };
        proc.prepare (spec);
    }
    void releaseResources() override { proc.reset(); }
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override { proc.process (buffer); }

    bool isBusesLayoutSupported (const BusesLayout& l) const override
    {
        return l.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
            && l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }

    void getStateInformation (juce::MemoryBlock& destData) override
    {
        juce::ValueTree state ("CabIR");
        state.setProperty ("irFile", proc.getIRFile().getFullPathName(), nullptr);

        auto p = proc.getParams();
        state.setProperty ("mix",       (double) p.mix,       nullptr);
        state.setProperty ("level",     (double) p.level,     nullptr);
        state.setProperty ("highCutHz", (double) p.highCutHz, nullptr);
        state.setProperty ("lowCutHz",  (double) p.lowCutHz,  nullptr);
        state.setProperty ("bypassed",  proc.isBypassed(),    nullptr);

        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
    }

    void setStateInformation (const void* data, int sizeInBytes) override
    {
        auto state = juce::ValueTree::readFromData (data, (size_t) sizeInBytes);
        if (! state.isValid()) return;

        CabIRProcessor::Params p;
        p.mix       = (float)(double) state.getProperty ("mix",       1.0);
        p.level     = (float)(double) state.getProperty ("level",     1.0);
        p.highCutHz = (float)(double) state.getProperty ("highCutHz", 12000.0);
        p.lowCutHz  = (float)(double) state.getProperty ("lowCutHz",  80.0);
        proc.setParams (p);
        proc.setBypassed ((bool) state.getProperty ("bypassed", false));

        juce::String irPath = state.getProperty ("irFile", "").toString();
        if (irPath.isNotEmpty())
        {
            juce::File irFile (irPath);
            if (irFile.existsAsFile())
                proc.loadIRFromFile (irFile);
        }
    }

    CabIRProcessor&       getProcessor()       { return proc; }
    const CabIRProcessor& getProcessor() const { return proc; }

private:
    CabIRProcessor proc;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CabIRProcessorNode)
};