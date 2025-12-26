/*
  ==============================================================================

    AVFMediaPlayer_Mac.h
    OnStage - Mac implementation using AVFoundation

  ==============================================================================
*/

#ifndef ONSTAGE_ENGINE_AVF_MEDIA_PLAYER_MAC_H
#define ONSTAGE_ENGINE_AVF_MEDIA_PLAYER_MAC_H

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "AVFMediaPlayer.h"

#ifdef __OBJC__
@class AVPlayer;
@class AVPlayerItem;
@class AVAsset;
@class AVAudioMix;
@class AVPlayerItemVideoOutput;
#else
typedef struct objc_object AVPlayer;
typedef struct objc_object AVPlayerItem;
typedef struct objc_object AVAsset;
typedef struct objc_object AVAudioMix;
typedef struct objc_object AVPlayerItemVideoOutput;
#endif

class AVFMediaPlayer_Mac : public AVFMediaPlayer
{
public:
    AVFMediaPlayer_Mac();
    ~AVFMediaPlayer_Mac() override;

    // AVFMediaPlayer overrides
    void prepareToPlay(int samplesPerBlock, double sampleRate) override;
    void releaseResources() override;

    void play(const juce::String& path) override;
    void stop() override;
    bool isPlaying() const override;

    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;
    void attachVideoComponent(juce::Component* videoComponent) override;

    // Extended functionality
    bool loadFile(const juce::String& path) override;
    void play() override;
    void pause() override;
    bool isPaused() const override;
    void setVolume(float newVolume) override;
    float getVolume() const override;
    void setRate(float newRate) override;
    float getRate() const override;
    bool hasFinished() const override;
    float getPosition() const override;
    void setPosition(float pos) override;
    int64_t getLengthMs() const override;
    juce::Image getCurrentVideoFrame() override;

private:
    void setupAudioTap();
    void cleanupPlayer();
    
    AVPlayer* player = nullptr;
    AVPlayerItem* playerItem = nullptr;
    AVPlayerItemVideoOutput* videoOutput = nullptr;
    
    juce::CriticalSection audioLock;
    juce::AudioBuffer<float> ringBuffer {2, 65536};
    juce::AbstractFifo fifo {65536};
    
    juce::CriticalSection videoLock;
    juce::Image currentVideoFrame;
    
    juce::Component* attachedVideoComponent = nullptr;
    
    double currentSampleRate = 44100.0;
    int maxBlockSize = 512;
    float volume = 1.0f;
    bool isPrepared = false;
    bool isCurrentlyPlaying = false;
    bool isCurrentlyPaused = false;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AVFMediaPlayer_Mac)
};

#endif // ONSTAGE_ENGINE_AVF_MEDIA_PLAYER_MAC_H
