/*
  ==============================================================================

    NativeMediaPlayer_Apple.h
    OnStage

    Shared Native implementation for iOS AND macOS using AVFoundation.
    Uses juce::AudioTransportSource for audio and AVPlayerItemVideoOutput for video.

  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_graphics/juce_graphics.h>

// Forward declaration for the Obj-C++ video wrapper
class VideoFrameExtractor; 

class NativeMediaPlayer_Apple
{
public:
    NativeMediaPlayer_Apple();
    ~NativeMediaPlayer_Apple();

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
    bool isPlaying() const;
    
    // Position/Length
    float getPosition() const;
    void setPosition(float pos);
    int64_t getLengthMs() const;

    // Callbacks
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& info);
    juce::Image getCurrentVideoFrame();

private:
    juce::AudioFormatManager formatManager;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    juce::AudioTransportSource transportSource;
    
    // Resampler to handle rate changes and sample rate mismatches
    juce::ResamplingAudioSource resampleSource {&transportSource, false, 2};

    double currentSampleRate = 44100.0;
    double originalSampleRate = 44100.0;
    float currentRate = 1.0f;

    std::unique_ptr<VideoFrameExtractor> videoExtractor;
    juce::Image currentVideoImage;
    bool isVideoLoaded = false;
};