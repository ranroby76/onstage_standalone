// ==============================================================================
//  PlaybackNode.cpp
//  OnStage — Media player source node implementation
// ==============================================================================

#include "PlaybackNode.h"

PlaybackNode::PlaybackNode (MediaPlayerType& player)
    : AudioProcessor (BusesProperties()
          .withOutput ("Main", juce::AudioChannelSet::stereo(), true)),
      mediaPlayer (player)
{
}

void PlaybackNode::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    mediaPlayer.prepareToPlay (samplesPerBlock, sampleRate);
}

void PlaybackNode::releaseResources()
{
    mediaPlayer.releaseResources();
}

void PlaybackNode::processBlock (juce::AudioBuffer<float>& buffer,
                                  juce::MidiBuffer&)
{
    // Fill the buffer with media player audio
    juce::AudioSourceChannelInfo info (&buffer, 0, buffer.getNumSamples());
    mediaPlayer.getNextAudioBlock (info);
}
