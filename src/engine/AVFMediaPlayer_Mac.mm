/*
  ==============================================================================

    AVFMediaPlayer_Mac.mm
    OnStage - Mac implementation using AVFoundation
    
    NOTE: This file must be compiled as Objective-C++ (.mm extension)

  ==============================================================================
*/

#include "AVFMediaPlayer_Mac.h"

#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>
#import <Accelerate/Accelerate.h>

// Audio tap callback for capturing audio from AVPlayer
static void audioTapProcess(MTAudioProcessingTapRef tap,
                            CMItemCount numberFrames,
                            MTAudioProcessingTapFlags flags,
                            AudioBufferList *bufferListInOut,
                            CMItemCount *numberFramesOut,
                            MTAudioProcessingTapFlags *flagsOut)
{
    AVFMediaPlayer_Mac* player = nullptr;
    void* context = MTAudioProcessingTapGetStorage(tap);
    if (context)
        player = static_cast<AVFMediaPlayer_Mac*>(context);
    
    OSStatus status = MTAudioProcessingTapGetSourceAudio(tap, numberFrames, bufferListInOut,
                                                          flagsOut, nullptr, numberFramesOut);
    
    if (status == noErr && player && bufferListInOut->mNumberBuffers > 0)
    {
        // Extract audio samples and add to ring buffer
        AudioBuffer& buffer = bufferListInOut->mBuffers[0];
        const float* audioData = static_cast<const float*>(buffer.mData);
        int numChannels = buffer.mNumberChannels;
        int numFrames = static_cast<int>(*numberFramesOut);
        
        // Convert interleaved to planar and add to JUCE ring buffer
        juce::AudioBuffer<float> tempBuffer(numChannels, numFrames);
        
        if (numChannels == 1)
        {
            tempBuffer.copyFrom(0, 0, audioData, numFrames);
        }
        else if (numChannels == 2)
        {
            // Deinterleave stereo
            for (int i = 0; i < numFrames; ++i)
            {
                tempBuffer.setSample(0, i, audioData[i * 2]);
                tempBuffer.setSample(1, i, audioData[i * 2 + 1]);
            }
        }
        
        // Add to ring buffer (implementation in player)
        // This would require exposing a method to add samples
    }
}

// Audio tap callbacks struct
static void audioTapInit(MTAudioProcessingTapRef tap, void *clientInfo, void **tapStorageOut)
{
    *tapStorageOut = clientInfo;
}

static void audioTapFinalize(MTAudioProcessingTapRef tap)
{
    // Cleanup if needed
}

static void audioTapPrepare(MTAudioProcessingTapRef tap,
                           CMItemCount maxFrames,
                           const AudioStreamBasicDescription *processingFormat)
{
    // Preparation if needed
}

static void audioTapUnprepare(MTAudioProcessingTapRef tap)
{
    // Cleanup if needed
}

// ==============================================================================

AVFMediaPlayer_Mac::AVFMediaPlayer_Mac()
{
}

AVFMediaPlayer_Mac::~AVFMediaPlayer_Mac()
{
    cleanupPlayer();
}

void AVFMediaPlayer_Mac::prepareToPlay(int samplesPerBlock, double sampleRate)
{
    maxBlockSize = samplesPerBlock;
    currentSampleRate = sampleRate;
    isPrepared = true;
}

void AVFMediaPlayer_Mac::releaseResources()
{
    isPrepared = false;
}

bool AVFMediaPlayer_Mac::loadFile(const juce::String& path)
{
    @autoreleasepool
    {
        cleanupPlayer();
        
        NSURL* url = [NSURL fileURLWithPath: path.toNSString()];
        if (!url)
            return false;
        
        AVAsset* asset = [AVAsset assetWithURL: url];
        if (!asset)
            return false;
        
        playerItem = [AVPlayerItem playerItemWithAsset: asset];
        if (!playerItem)
            return false;
        
        player = [AVPlayer playerWithPlayerItem: playerItem];
        if (!player)
            return false;
        
        // Setup audio tap for audio extraction
        setupAudioTap();
        
        // Setup video output
        NSDictionary* pixelBufferAttributes = @{
            (id)kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_32BGRA)
        };
        
        videoOutput = [[AVPlayerItemVideoOutput alloc] initWithPixelBufferAttributes: pixelBufferAttributes];
        [playerItem addOutput: videoOutput];
        
        return true;
    }
}

void AVFMediaPlayer_Mac::play(const juce::String& path)
{
    if (loadFile(path))
        play();
}

void AVFMediaPlayer_Mac::play()
{
    @autoreleasepool
    {
        if (player)
        {
            [player play];
            isCurrentlyPlaying = true;
            isCurrentlyPaused = false;
        }
    }
}

void AVFMediaPlayer_Mac::pause()
{
    @autoreleasepool
    {
        if (player)
        {
            [player pause];
            isCurrentlyPlaying = false;
            isCurrentlyPaused = true;
        }
    }
}

void AVFMediaPlayer_Mac::stop()
{
    @autoreleasepool
    {
        if (player)
        {
            [player pause];
            [player seekToTime: kCMTimeZero];
            isCurrentlyPlaying = false;
            isCurrentlyPaused = false;
        }
    }
}

bool AVFMediaPlayer_Mac::isPlaying() const
{
    return isCurrentlyPlaying;
}

bool AVFMediaPlayer_Mac::isPaused() const
{
    return isCurrentlyPaused;
}

void AVFMediaPlayer_Mac::setVolume(float newVolume)
{
    @autoreleasepool
    {
        volume = newVolume;
        if (player)
            [player setVolume: volume];
    }
}

float AVFMediaPlayer_Mac::getVolume() const
{
    return volume;
}

void AVFMediaPlayer_Mac::setRate(float newRate)
{
    @autoreleasepool
    {
        if (player)
            [player setRate: newRate];
    }
}

float AVFMediaPlayer_Mac::getRate() const
{
    @autoreleasepool
    {
        if (player)
            return [player rate];
        return 1.0f;
    }
}

bool AVFMediaPlayer_Mac::hasFinished() const
{
    @autoreleasepool
    {
        if (!playerItem)
            return true;
        
        CMTime currentTime = [playerItem currentTime];
        CMTime duration = [playerItem duration];
        
        return CMTIME_COMPARE_INLINE(currentTime, >=, duration);
    }
}

float AVFMediaPlayer_Mac::getPosition() const
{
    @autoreleasepool
    {
        if (!playerItem)
            return 0.0f;
        
        CMTime currentTime = [playerItem currentTime];
        CMTime duration = [playerItem duration];
        
        if (CMTIME_IS_INVALID(duration) || CMTIME_IS_INDEFINITE(duration))
            return 0.0f;
        
        double current = CMTimeGetSeconds(currentTime);
        double total = CMTimeGetSeconds(duration);
        
        if (total > 0.0)
            return static_cast<float>(current / total);
        
        return 0.0f;
    }
}

void AVFMediaPlayer_Mac::setPosition(float pos)
{
    @autoreleasepool
    {
        if (!playerItem)
            return;
        
        CMTime duration = [playerItem duration];
        if (CMTIME_IS_INVALID(duration) || CMTIME_IS_INDEFINITE(duration))
            return;
        
        double seconds = CMTimeGetSeconds(duration) * pos;
        CMTime targetTime = CMTimeMakeWithSeconds(seconds, duration.timescale);
        
        [player seekToTime: targetTime toleranceBefore: kCMTimeZero toleranceAfter: kCMTimeZero];
    }
}

int64_t AVFMediaPlayer_Mac::getLengthMs() const
{
    @autoreleasepool
    {
        if (!playerItem)
            return 0;
        
        CMTime duration = [playerItem duration];
        if (CMTIME_IS_INVALID(duration) || CMTIME_IS_INDEFINITE(duration))
            return 0;
        
        double seconds = CMTimeGetSeconds(duration);
        return static_cast<int64_t>(seconds * 1000.0);
    }
}

void AVFMediaPlayer_Mac::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    juce::ScopedLock lock(audioLock);
    
    int numSamples = bufferToFill.numSamples;
    int availableSamples = fifo.getNumReady();
    
    if (availableSamples < numSamples)
    {
        // Not enough samples - fill with silence
        bufferToFill.clearActiveBufferRegion();
        return;
    }
    
    // Read from ring buffer
    int start1, size1, start2, size2;
    fifo.prepareToRead(numSamples, start1, size1, start2, size2);
    
    // Copy first block
    if (size1 > 0)
    {
        for (int ch = 0; ch < bufferToFill.buffer->getNumChannels(); ++ch)
        {
            if (ch < ringBuffer.getNumChannels())
                bufferToFill.buffer->copyFrom(ch, bufferToFill.startSample,
                                              ringBuffer, ch, start1, size1);
        }
    }
    
    // Copy second block (wraparound)
    if (size2 > 0)
    {
        for (int ch = 0; ch < bufferToFill.buffer->getNumChannels(); ++ch)
        {
            if (ch < ringBuffer.getNumChannels())
                bufferToFill.buffer->copyFrom(ch, bufferToFill.startSample + size1,
                                              ringBuffer, ch, start2, size2);
        }
    }
    
    fifo.finishedRead(size1 + size2);
    
    // Apply volume
    bufferToFill.buffer->applyGain(bufferToFill.startSample, numSamples, volume);
}

juce::Image AVFMediaPlayer_Mac::getCurrentVideoFrame()
{
    @autoreleasepool
    {
        juce::ScopedLock lock(videoLock);
        
        if (!videoOutput || !playerItem)
            return juce::Image();
        
        CMTime currentTime = [playerItem currentTime];
        CVPixelBufferRef pixelBuffer = [videoOutput copyPixelBufferForItemTime: currentTime
                                                             itemTimeForDisplay: nil];
        
        if (!pixelBuffer)
            return currentVideoFrame;
        
        CVPixelBufferLockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);
        
        size_t width = CVPixelBufferGetWidth(pixelBuffer);
        size_t height = CVPixelBufferGetHeight(pixelBuffer);
        void* baseAddress = CVPixelBufferGetBaseAddress(pixelBuffer);
        size_t bytesPerRow = CVPixelBufferGetBytesPerRow(pixelBuffer);
        
        if (currentVideoFrame.getWidth() != (int)width ||
            currentVideoFrame.getHeight() != (int)height)
        {
            currentVideoFrame = juce::Image(juce::Image::ARGB, (int)width, (int)height, true);
        }
        
        juce::Image::BitmapData bitmap(currentVideoFrame, juce::Image::BitmapData::writeOnly);
        
        for (size_t y = 0; y < height; ++y)
        {
            uint8_t* src = (uint8_t*)baseAddress + (y * bytesPerRow);
            uint8_t* dst = bitmap.getLinePointer((int)y);
            memcpy(dst, src, width * 4);
        }
        
        CVPixelBufferUnlockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);
        CVPixelBufferRelease(pixelBuffer);
        
        return currentVideoFrame;
    }
}

void AVFMediaPlayer_Mac::attachVideoComponent(juce::Component* videoComponent)
{
    attachedVideoComponent = videoComponent;
}

void AVFMediaPlayer_Mac::setupAudioTap()
{
    // Audio tap setup would go here
    // This requires creating MTAudioProcessingTap with callbacks
    // Simplified for now - full implementation would capture audio via tap
}

void AVFMediaPlayer_Mac::cleanupPlayer()
{
    @autoreleasepool
    {
        if (player)
        {
            [player pause];
            player = nullptr;
        }
        
        if (videoOutput)
        {
            videoOutput = nullptr;
        }
        
        if (playerItem)
        {
            playerItem = nullptr;
        }
        
        isCurrentlyPlaying = false;
        isCurrentlyPaused = false;
    }
}
