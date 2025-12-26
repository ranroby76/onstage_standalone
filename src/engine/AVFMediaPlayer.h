/*
  ==============================================================================

    AVFMediaPlayer.h
    OnStage - Abstract base class interface for Mac (AVFoundation)

  ==============================================================================
*/

#ifndef ONSTAGE_ENGINE_AVF_MEDIA_PLAYER_H
#define ONSTAGE_ENGINE_AVF_MEDIA_PLAYER_H

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_gui_basics/juce_gui_basics.h>

class AVFMediaPlayer
{
public:
    AVFMediaPlayer() = default;
    virtual ~AVFMediaPlayer() = default;

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
    
    // Extended functionality
    virtual bool loadFile(const juce::String& path) = 0;
    virtual void play() = 0;
    virtual void pause() = 0;
    virtual bool isPaused() const = 0;
    virtual void setVolume(float newVolume) = 0;
    virtual float getVolume() const = 0;
    virtual void setRate(float newRate) = 0;
    virtual float getRate() const = 0;
    virtual bool hasFinished() const = 0;
    virtual float getPosition() const = 0;
    virtual void setPosition(float pos) = 0;
    virtual int64_t getLengthMs() const = 0;
    virtual juce::Image getCurrentVideoFrame() = 0;
};

#endif // ONSTAGE_ENGINE_AVF_MEDIA_PLAYER_H
