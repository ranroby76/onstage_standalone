/*
  ==============================================================================

    VLCMediaPlayer.h
    OnStage - Abstract base class interface

  ==============================================================================
*/

#ifndef ONSTAGE_ENGINE_VLC_MEDIA_PLAYER_H
#define ONSTAGE_ENGINE_VLC_MEDIA_PLAYER_H

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_gui_basics/juce_gui_basics.h>

class VLCMediaPlayer
{
public:
    VLCMediaPlayer() = default;
    virtual ~VLCMediaPlayer() = default;

    // Lifecycle
    virtual void prepareToPlay(int samplesPerBlock, double sampleRate) = 0;
    virtual void releaseResources() = 0;

    // Playback
    virtual void play(const juce::String& path) = 0;
    virtual void stop() = 0;
    virtual bool isPlaying() const = 0;

    // Audio
    virtual void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) = 0;

    // Video
    virtual void attachVideoComponent(juce::Component* videoComponent) = 0;
};

#endif // ONSTAGE_ENGINE_VLC_MEDIA_PLAYER_H
