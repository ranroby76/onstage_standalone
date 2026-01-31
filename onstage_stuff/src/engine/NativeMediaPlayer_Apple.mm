/*
  ==============================================================================

    NativeMediaPlayer_Apple.mm
    OnStage

    Implementation using AVFoundation (Shared for macOS & iOS).

  ==============================================================================
*/

#include "NativeMediaPlayer_Apple.h"
#import <AVFoundation/AVFoundation.h>
#import <CoreVideo/CoreVideo.h>

// ==============================================================================
// VideoFrameExtractor
// Helper class to extract video frames synchronized to a timestamp
// ==============================================================================
class VideoFrameExtractor
{
public:
    VideoFrameExtractor() {}
    ~VideoFrameExtractor() { cleanUp(); }

    void loadVideo(const juce::String& path)
    {
        cleanUp();
        
        NSURL* url = [NSURL fileURLWithPath: [NSString stringWithUTF8String: path.toRawUTF8()]];
        
        // Load the asset
        asset = [AVURLAsset assetWithURL:url];
        if (!asset) return;

        // Setup Video Output settings (BGRA is standard for CoreVideo on Apple Silicon & Intel)
        NSDictionary* settings = @{ (id)kCVPixelBufferPixelFormatTypeKey : @(kCVPixelFormatType_32BGRA) };
        output = [[AVPlayerItemVideoOutput alloc] initWithPixelBufferAttributes:settings];
        
        playerItem = [AVPlayerItem playerItemWithAsset:asset];
        [playerItem addOutput:output];
        
        // We create a player just to hold the item status (loading/ready), 
        // but we drive the time manually via getFrameAtTime
        player = [AVPlayer playerWithPlayerItem:playerItem];
        player.muted = YES; 
        player.actionAtItemEnd = AVPlayerActionAtItemEndPause;
    }

    juce::Image getFrameAtTime(double seconds)
    {
        if (!output) return juce::Image();

        // Timebase 600 is standard for video
        CMTime time = CMTimeMakeWithSeconds(seconds, 600);
        
        // Check if a pixel buffer is available for this timestamp
        if ([output hasNewPixelBufferForItemTime:time])
        {
            CMTime actualTime;
            CVPixelBufferRef buffer = [output copyPixelBufferForItemTime:time itemTimeForDisplay:&actualTime];
            
            if (buffer)
            {
                juce::Image img = convertPixelBufferToImage(buffer);
                CVPixelBufferRelease(buffer); // Important: Release the buffer
                return img;
            }
        }
        return juce::Image(); // Return invalid image if no new frame
    }

    void cleanUp() {
        player = nil;
        playerItem = nil;
        output = nil;
        asset = nil;
    }

private:
    AVPlayer* player = nil;
    AVPlayerItem* playerItem = nil;
    AVPlayerItemVideoOutput* output = nil;
    AVAsset* asset = nil;

    juce::Image convertPixelBufferToImage(CVPixelBufferRef buffer)
    {
        CVPixelBufferLockBaseAddress(buffer, kCVPixelBufferLock_ReadOnly);
        
        int width = (int)CVPixelBufferGetWidth(buffer);
        int height = (int)CVPixelBufferGetHeight(buffer);
        uint8* srcData = (uint8*)CVPixelBufferGetBaseAddress(buffer);
        size_t bytesPerRow = CVPixelBufferGetBytesPerRow(buffer);

        // JUCE ARGB vs CoreVideo BGRA
        // JUCE's ARGB format in memory is typically B-G-R-A on little-endian architectures
        // (which includes both Intel Macs and Apple Silicon).
        // A direct memcpy usually results in correct colors.
        
        juce::Image image(juce::Image::ARGB, width, height, true);
        juce::Image::BitmapData destData(image, juce::Image::BitmapData::writeOnly);

        for (int y = 0; y < height; ++y)
        {
            const uint8* srcRow = srcData + (y * bytesPerRow);
            uint8* destRow = destData.getLinePointer(y);
            memcpy(destRow, srcRow, width * 4);
        }

        CVPixelBufferUnlockBaseAddress(buffer, kCVPixelBufferLock_ReadOnly);
        return image;
    }
};

// ==============================================================================
// NativeMediaPlayer_Apple Implementation
// ==============================================================================

NativeMediaPlayer_Apple::NativeMediaPlayer_Apple()
{
    formatManager.registerBasicFormats(); 
    videoExtractor = std::make_unique<VideoFrameExtractor>();
}

NativeMediaPlayer_Apple::~NativeMediaPlayer_Apple()
{
    transportSource.setSource(nullptr);
}

bool NativeMediaPlayer_Apple::prepareToPlay(int samplesPerBlock, double sampleRate)
{
    currentSampleRate = sampleRate;
    resampleSource.prepareToPlay(samplesPerBlock, sampleRate);
    return true;
}

void NativeMediaPlayer_Apple::releaseResources()
{
    transportSource.releaseResources();
    resampleSource.releaseResources();
}

bool NativeMediaPlayer_Apple::loadFile(const juce::String& path)
{
    transportSource.stop();
    transportSource.setSource(nullptr);
    videoExtractor->cleanUp();
    isVideoLoaded = false;
    currentVideoImage = juce::Image();

    juce::File file(path);
    auto* reader = formatManager.createReaderFor(file);
    
    if (reader != nullptr)
    {
        originalSampleRate = reader->sampleRate;
        readerSource.reset(new juce::AudioFormatReaderSource(reader, true));
        
        // Link transport to source. Small buffer helps prevent glitches.
        transportSource.setSource(readerSource.get(), 32768, nullptr, reader->sampleRate);
        
        // Reset Resampler
        double ratio = (originalSampleRate / currentSampleRate) / (double)currentRate;
        resampleSource.setResamplingRatio(ratio);

        // Load Video Side if applicable
        if (path.endsWithIgnoreCase(".mp4") || path.endsWithIgnoreCase(".mov") || path.endsWithIgnoreCase(".m4v") || path.endsWithIgnoreCase(".avi"))
        {
            videoExtractor->loadVideo(path);
            isVideoLoaded = true;
        }
        
        return true;
    }
    return false;
}

void NativeMediaPlayer_Apple::play() 
{ 
    transportSource.start(); 
}

void NativeMediaPlayer_Apple::pause() 
{ 
    transportSource.stop(); 
}

void NativeMediaPlayer_Apple::stop()  
{ 
    transportSource.stop(); 
    transportSource.setPosition(0); 
    if (currentVideoImage.isValid())
        currentVideoImage.clear(currentVideoImage.getBounds(), juce::Colours::black);
}

void NativeMediaPlayer_Apple::setVolume(float newVolume) 
{ 
    transportSource.setGain(newVolume); 
}

float NativeMediaPlayer_Apple::getVolume() const 
{ 
    return transportSource.getGain(); 
}

void NativeMediaPlayer_Apple::setRate(float newRate)
{
    currentRate = newRate;
    if (currentSampleRate > 0 && originalSampleRate > 0)
    {
        // Resampling ratio = (SourceRate / TargetRate) / Speed
        double ratio = (originalSampleRate / currentSampleRate) / (double)newRate;
        resampleSource.setResamplingRatio(ratio);
    }
}

float NativeMediaPlayer_Apple::getRate() const 
{ 
    return currentRate; 
}

bool NativeMediaPlayer_Apple::hasFinished() const 
{ 
    return transportSource.hasStreamFinished(); 
}

bool NativeMediaPlayer_Apple::isPlaying() const 
{ 
    return transportSource.isPlaying(); 
}

float NativeMediaPlayer_Apple::getPosition() const
{
    if (transportSource.getLengthInSeconds() > 0)
        return (float)(transportSource.getCurrentPosition() / transportSource.getLengthInSeconds());
    return 0.0f;
}

void NativeMediaPlayer_Apple::setPosition(float pos)
{
    if (transportSource.getLengthInSeconds() > 0)
        transportSource.setPosition(pos * transportSource.getLengthInSeconds());
}

int64_t NativeMediaPlayer_Apple::getLengthMs() const
{
    return (int64_t)(transportSource.getLengthInSeconds() * 1000.0);
}

void NativeMediaPlayer_Apple::getNextAudioBlock(const juce::AudioSourceChannelInfo& info)
{
    // Pull audio from the Resampler (which pulls from Transport -> Reader)
    resampleSource.getNextAudioBlock(info);
}

juce::Image NativeMediaPlayer_Apple::getCurrentVideoFrame()
{
    if (!isVideoLoaded) return juce::Image();

    // Sync: Ask for the frame corresponding to the current AUDIO time
    double currentSeconds = transportSource.getCurrentPosition();
    juce::Image frame = videoExtractor->getFrameAtTime(currentSeconds);
    
    if (frame.isValid())
        currentVideoImage = frame;
        
    return currentVideoImage;
}