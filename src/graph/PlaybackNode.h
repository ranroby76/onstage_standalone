// ==============================================================================
//  PlaybackNode.h
//  OnStage — Injects media player audio into the graph as a source node
//
//  No inputs, stereo output.  Pulls audio from the platform-specific media
//  player each block.  The pitch-shift setting is controlled externally.
// ==============================================================================

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../AudioEngine.h"     // for MediaPlayerType typedef

class PlaybackNode : public juce::AudioProcessor
{
public:
    explicit PlaybackNode (MediaPlayerType& player);

    // --- AudioProcessor -----------------------------------------------------
    const juce::String getName() const override { return "Playback"; }

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    bool acceptsMidi()  const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int  getNumPrograms() override                        { return 1; }
    int  getCurrentProgram() override                     { return 0; }
    void setCurrentProgram (int) override                 {}
    const juce::String getProgramName (int) override      { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    juce::AudioProcessorEditor* createEditor() override   { return nullptr; }
    bool hasEditor() const override                       { return false; }

    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override   {}

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override
    {
        // Source node: no inputs, stereo output
        return layouts.getMainInputChannelSet().isDisabled()
            && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }

private:
    MediaPlayerType& mediaPlayer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlaybackNode)
};
