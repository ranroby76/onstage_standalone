// ### **3. The Desktop Implementation (Source)** Your **original** `VLCMediaPlayer.cpp`, adapted to implement `VLCMediaPlayer_Desktop`.

/*
  ==============================================================================

    VLCMediaPlayer_Desktop.h
    OnStage

    Original LibVLC implementation for Windows/Mac/Linux.
    Renamed from VLCMediaPlayer to VLCMediaPlayer_Desktop.

  ==============================================================================
*/

#ifndef ONSTAGE_ENGINE_VLC_MEDIA_PLAYER_DESKTOP_H
#define ONSTAGE_ENGINE_VLC_MEDIA_PLAYER_DESKTOP_H

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_graphics/juce_graphics.h> 

extern "C" {
    #include <vlc/libvlc.h>
    #include <vlc/libvlc_media.h>
    #include <vlc/libvlc_renderer_discoverer.h> 
    #include <vlc/libvlc_media_player.h>
}

class VLCMediaPlayer_Desktop
{
public:
    VLCMediaPlayer_Desktop();
    ~VLCMediaPlayer_Desktop();

    bool prepareToPlay(int samplesPerBlock, double sampleRate);
    void releaseResources();
    bool loadFile(const juce::String& path);
    void play();
    void pause();
    void stop();
    void setVolume(float newVolume);
    float getVolume() const;
    void setRate(float newRate);
    float getRate() const;
    bool hasFinished() const;

    void getNextAudioBlock(const juce::AudioSourceChannelInfo& info);
    juce::Image getCurrentVideoFrame();

    bool isPlaying() const;
    float getPosition() const;
    void setPosition(float pos);
    int64_t getLengthMs() const;

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
    int videoWidth = 1280;
    int videoHeight = 720;

    double currentSampleRate = 44100.0;
    int maxBlockSize = 512;
    float volume = 1.0f;
    bool isPrepared = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VLCMediaPlayer_Desktop)
};
#endif // ONSTAGE_ENGINE_VLC_MEDIA_PLAYER_DESKTOP_H