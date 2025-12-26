/*
  ==============================================================================

    VLCMediaPlayer_Desktop.h
    OnStage - MINIMAL BUG FIX VERSION (keeps abstract base class architecture)

  ==============================================================================
*/

#ifndef ONSTAGE_ENGINE_VLC_MEDIA_PLAYER_DESKTOP_H
#define ONSTAGE_ENGINE_VLC_MEDIA_PLAYER_DESKTOP_H

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "VLCMediaPlayer.h"

extern "C" {
    #include <vlc/libvlc.h>
    #include <vlc/libvlc_media.h>
    #include <vlc/libvlc_renderer_discoverer.h> 
    #include <vlc/libvlc_media_player.h>
}

class VLCMediaPlayer_Desktop : public VLCMediaPlayer
{
public:
    VLCMediaPlayer_Desktop();
    ~VLCMediaPlayer_Desktop() override;

    // VLCMediaPlayer overrides
    void prepareToPlay(int samplesPerBlock, double sampleRate) override;
    void releaseResources() override;

    void play(const juce::String& path) override;
    void stop() override;
    bool isPlaying() const override;

    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;
    void attachVideoComponent(juce::Component* videoComponent) override;

    // Additional methods for extended functionality
    bool loadFile(const juce::String& path);
    void play();  // Play already loaded file
    void pause();
    bool isPaused() const;
    void setVolume(float newVolume);
    float getVolume() const;
    void setRate(float newRate);
    float getRate() const;
    bool hasFinished() const;
    float getPosition() const;
    void setPosition(float pos);
    int64_t getLengthMs() const;
    juce::Image getCurrentVideoFrame();

private:
    static void audioPlay(void* data, const void* samples, unsigned count, int64_t pts);
    static void audioPause(void* data, int64_t pts);
    static void audioResume(void* data, int64_t pts);
    static void audioFlush(void* data, int64_t pts);
    static void audioDrain(void* data);

    static void* videoLock(void* data, void** planes);
    static void videoUnlock(void* data, void* picture, void* const* planes);
    static void videoDisplay(void* data, void* picture);

    void addAudioSamples(const void* samples, unsigned count, int64_t pts);

    libvlc_instance_t* m_instance = nullptr;
    libvlc_media_player_t* m_mediaPlayer = nullptr;
    
    juce::CriticalSection audioLock;
    juce::AudioBuffer<float> ringBuffer {2, 65536}; 
    juce::AbstractFifo fifo {65536};

    juce::CriticalSection videoLockMutex;
    juce::Image currentVideoImage; 
    juce::Image bufferVideoImage;
    
    // FIX #1: REMOVED videoBitmapData member - this was causing the crash!
    
    juce::Component* attachedVideoComponent = nullptr;

    double currentSampleRate = 44100.0;
    int maxBlockSize = 512;
    float volume = 1.0f;
    bool isPrepared = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VLCMediaPlayer_Desktop)
};
#endif // ONSTAGE_ENGINE_VLC_MEDIA_PLAYER_DESKTOP_H
