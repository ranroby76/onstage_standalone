// ==============================================================================
//  CabIRNode.h
//  OnStage — Node wrapper for Convolution IR Cabinet
//
//  Append this to GuitarNodes.h, or include separately.
//  Also add the #include for CabIRProcessor.h at the top of GuitarNodes.h.
// ==============================================================================

#pragma once

#include "CabIRProcessor.h"

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

    // State: save/restore IR file path
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
