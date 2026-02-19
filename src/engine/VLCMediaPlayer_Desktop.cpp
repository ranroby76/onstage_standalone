/*
  ==============================================================================

    VLCMediaPlayer_Desktop.cpp
    OnStage - MINIMAL BUG FIX VERSION (keeps abstract base class architecture)

    FIX: VLC args for proper A/V sync without sacrificing audio quality.
    FIX: Volume smoothing to prevent clicks on volume changes.

  ==============================================================================
*/

#include "VLCMediaPlayer_Desktop.h"
#include <cstring>
#include <juce_core/juce_core.h>

#if JUCE_LINUX
#include <stdlib.h>
#endif

VLCMediaPlayer_Desktop::VLCMediaPlayer_Desktop()
{
    juce::File appDir = juce::File::getSpecialLocation(juce::File::currentApplicationFile).getParentDirectory();
    juce::File pluginDir = appDir.getChildFile("plugins");
    
    #if JUCE_WINDOWS
        juce::String pathEnv = "VLC_PLUGIN_PATH=" + pluginDir.getFullPathName();
        _putenv(pathEnv.toRawUTF8());
    #endif

    const char* args[] = { 
        "--aout=amem", 
        "--vout=vmem",
        "--no-video-title-show",
        "--no-osd",
        "--no-xlib",
        "--quiet",
        
        // A/V sync: keep video frames in lockstep with audio
        "--no-drop-late-frames",
        "--no-skip-frames",
        "--clock-jitter=0",
        
        // Caching: 500ms gives decoder headroom without adding latency
        "--file-caching=500",
        "--network-caching=500"
    };
    m_instance = libvlc_new(sizeof(args) / sizeof(args[0]), args);
    
    if (m_instance)
    {
        m_mediaPlayer = libvlc_media_player_new(m_instance);
        
        if (m_mediaPlayer)
        {
            libvlc_audio_set_callbacks(m_mediaPlayer, audioPlay, audioPause, audioResume, audioFlush, audioDrain, this);
            libvlc_audio_set_format(m_mediaPlayer, "S16N", 44100, 2);

            libvlc_video_set_callbacks(m_mediaPlayer, videoLock, videoUnlock, videoDisplay, this);
            libvlc_video_set_format(m_mediaPlayer, "RV32", 1280, 720, 1280 * 4);
            
            currentVideoImage = juce::Image(juce::Image::ARGB, 1280, 720, true);
            bufferVideoImage = juce::Image(juce::Image::ARGB, 1280, 720, true);
            
            currentVideoImage.clear(currentVideoImage.getBounds(), juce::Colours::black);
            bufferVideoImage.clear(bufferVideoImage.getBounds(), juce::Colours::black);
        }
    }
}

VLCMediaPlayer_Desktop::~VLCMediaPlayer_Desktop()
{
    stop();
    if (m_mediaPlayer) libvlc_media_player_release(m_mediaPlayer);
    if (m_instance) libvlc_release(m_instance);
}

void VLCMediaPlayer_Desktop::prepareToPlay(int samplesPerBlock, double sampleRate)
{
    if (sampleRate > 1000.0)
        currentSampleRate = sampleRate;
    else
        currentSampleRate = 44100.0;

    maxBlockSize = samplesPerBlock;
    
    ringBuffer.setSize(2, 65536); 
    fifo.setTotalSize(ringBuffer.getNumSamples());
    fifo.reset();
    
    if (m_mediaPlayer)
    {
        libvlc_audio_set_format(m_mediaPlayer, "S16N", static_cast<int>(currentSampleRate), 2);
    }

    // Reset volume smoother
    smoothedVolume = volume;

    isPrepared = true;
}

void VLCMediaPlayer_Desktop::releaseResources()
{
    stop();
    fifo.reset();
    ringBuffer.clear();
    isPrepared = false;
}

bool VLCMediaPlayer_Desktop::loadFile(const juce::String& path)
{
    stop();
    if (!m_instance || !m_mediaPlayer) return false;
    
    int rate = (currentSampleRate > 0) ? static_cast<int>(currentSampleRate) : 44100;
    libvlc_audio_set_format(m_mediaPlayer, "S16N", rate, 2);

    libvlc_media_t* media = libvlc_media_new_path(m_instance, path.toUTF8());
    if (media == nullptr) return false;

    libvlc_media_player_set_media(m_mediaPlayer, media);
    libvlc_media_release(media);
    return true;
}

void VLCMediaPlayer_Desktop::play(const juce::String& path)
{
    if (loadFile(path))
    {
        if (m_mediaPlayer) 
        {
            int rate = (currentSampleRate > 0) ? static_cast<int>(currentSampleRate) : 44100;
            libvlc_audio_set_format(m_mediaPlayer, "S16N", rate, 2);
            libvlc_media_player_play(m_mediaPlayer);
        }
    }
}

void VLCMediaPlayer_Desktop::play()
{
    if (m_mediaPlayer) 
    {
        int rate = (currentSampleRate > 0) ? static_cast<int>(currentSampleRate) : 44100;
        libvlc_audio_set_format(m_mediaPlayer, "S16N", rate, 2);
        libvlc_media_player_play(m_mediaPlayer);
    }
}

void VLCMediaPlayer_Desktop::pause()
{
    if (m_mediaPlayer) libvlc_media_player_pause(m_mediaPlayer);
}

void VLCMediaPlayer_Desktop::stop()
{
    if (m_mediaPlayer) libvlc_media_player_stop(m_mediaPlayer);
    
    juce::ScopedLock sl(audioLock);
    fifo.reset();
    ringBuffer.clear();
    
    juce::ScopedLock slV(videoLockMutex);
    if (currentVideoImage.isValid())
        currentVideoImage.clear(currentVideoImage.getBounds(), juce::Colours::black);
}

void VLCMediaPlayer_Desktop::setVolume(float newVolume)
{
    volume = juce::jlimit(0.0f, 2.0f, newVolume);
}

float VLCMediaPlayer_Desktop::getVolume() const { return volume; }

void VLCMediaPlayer_Desktop::setRate(float newRate)
{
    if (m_mediaPlayer) libvlc_media_player_set_rate(m_mediaPlayer, newRate);
}

float VLCMediaPlayer_Desktop::getRate() const
{
    return m_mediaPlayer ? libvlc_media_player_get_rate(m_mediaPlayer) : 1.0f;
}

bool VLCMediaPlayer_Desktop::hasFinished() const
{
    if (!m_mediaPlayer) return false;
    return libvlc_media_player_get_state(m_mediaPlayer) == libvlc_Ended;
}

bool VLCMediaPlayer_Desktop::isPlaying() const
{
    if (!m_mediaPlayer) return false;
    return libvlc_media_player_is_playing(m_mediaPlayer) != 0;
}

bool VLCMediaPlayer_Desktop::isPaused() const
{
    if (!m_mediaPlayer) return false;
    auto state = libvlc_media_player_get_state(m_mediaPlayer);
    return state == libvlc_Paused;
}

float VLCMediaPlayer_Desktop::getPosition() const
{
    if (!m_mediaPlayer) return 0.0f;
    return libvlc_media_player_get_position(m_mediaPlayer);
}

void VLCMediaPlayer_Desktop::setPosition(float pos)
{
    if (m_mediaPlayer) libvlc_media_player_set_position(m_mediaPlayer, pos);
}

int64_t VLCMediaPlayer_Desktop::getLengthMs() const
{
    if (!m_mediaPlayer) return 0;
    return libvlc_media_player_get_length(m_mediaPlayer);
}

// FIX #1: BitmapData MUST be created on stack and destroyed in same scope
void* VLCMediaPlayer_Desktop::videoLock(void* data, void** planes)
{
    auto* self = static_cast<VLCMediaPlayer_Desktop*>(data);
    if (self && self->bufferVideoImage.isValid())
    {
        juce::Image::BitmapData bitmapData(self->bufferVideoImage, juce::Image::BitmapData::readWrite);
        *planes = bitmapData.data;
        return nullptr;
    }
    return nullptr;
}

void VLCMediaPlayer_Desktop::videoUnlock(void* data, void* picture, void* const* planes)
{
    juce::ignoreUnused(data, picture, planes);
}

void VLCMediaPlayer_Desktop::videoDisplay(void* data, void* picture)
{
    auto* self = static_cast<VLCMediaPlayer_Desktop*>(data);
    if (self)
    {
        juce::ScopedLock sl(self->videoLockMutex);
        if (self->bufferVideoImage.isValid())
            self->currentVideoImage = self->bufferVideoImage.createCopy();
        
        if (self->attachedVideoComponent)
            self->attachedVideoComponent->repaint();
    }
}

juce::Image VLCMediaPlayer_Desktop::getCurrentVideoFrame()
{
    juce::ScopedLock sl(videoLockMutex);
    if (currentVideoImage.isValid())
        return currentVideoImage;
    return juce::Image();
}

void VLCMediaPlayer_Desktop::attachVideoComponent(juce::Component* videoComponent)
{
    attachedVideoComponent = videoComponent;
}

void VLCMediaPlayer_Desktop::audioPlay(void* data, const void* samples, unsigned count, int64_t pts) {
    auto* self = static_cast<VLCMediaPlayer_Desktop*>(data);
    if (self) self->addAudioSamples(samples, count, pts);
}
void VLCMediaPlayer_Desktop::audioPause(void*, int64_t) {}
void VLCMediaPlayer_Desktop::audioResume(void*, int64_t) {}
void VLCMediaPlayer_Desktop::audioFlush(void* data, int64_t) {
    auto* self = static_cast<VLCMediaPlayer_Desktop*>(data);
    if (self) { juce::ScopedLock sl(self->audioLock); self->fifo.reset(); }
}
void VLCMediaPlayer_Desktop::audioDrain(void*) {}

void VLCMediaPlayer_Desktop::addAudioSamples(const void* samples, unsigned count, int64_t pts)
{
    juce::ScopedLock sl(audioLock);
    const int space = fifo.getFreeSpace();
    int toWrite = juce::jmin(static_cast<int>(count), space);
    if (toWrite > 0)
    {
        int start1, size1, start2, size2;
        fifo.prepareToWrite(toWrite, start1, size1, start2, size2);
        
        const int16_t* src = static_cast<const int16_t*>(samples);
        const float scale = 1.0f / 32768.0f;
        if (size1 > 0) {
            for (int i = 0; i < size1; ++i) {
                float left = src[i * 2] * scale;
                float right = src[i * 2 + 1] * scale;
                ringBuffer.setSample(0, start1 + i, left);
                ringBuffer.setSample(1, start1 + i, right);
            }
        }
        if (size2 > 0) {
            for (int i = 0; i < size2; ++i) {
                float left = src[(size1 + i) * 2] * scale;
                float right = src[(size1 + i) * 2 + 1] * scale;
                ringBuffer.setSample(0, start2 + i, left);
                ringBuffer.setSample(1, start2 + i, right);
            }
        }
        fifo.finishedWrite(size1 + size2);
    }
}

void VLCMediaPlayer_Desktop::getNextAudioBlock(const juce::AudioSourceChannelInfo& info)
{
    if (!isPrepared) { info.clearActiveBufferRegion(); return; }
    
    if (libvlc_media_player_get_state(m_mediaPlayer) == libvlc_Paused)
    {
        info.clearActiveBufferRegion();
        return;
    }

    juce::ScopedLock sl(audioLock);
    
    int numSamples = info.numSamples;
    int available = fifo.getNumReady();
    int toRead = juce::jmin(numSamples, available);
    
    if (toRead > 0) {
        int start1, size1, start2, size2;
        fifo.prepareToRead(toRead, start1, size1, start2, size2);
        
        // FIX: Volume smoothing — ramp to prevent clicks on volume changes
        const float targetVol = volume;
        const float startVol = smoothedVolume;
        const float volStep = (targetVol - startVol) / (float)toRead;
        
        // First segment
        if (size1 > 0) {
            for (int ch = 0; ch < 2; ++ch) {
                const float* src = ringBuffer.getReadPointer(ch, start1);
                float* dst = info.buffer->getWritePointer(ch, info.startSample);
                float vol = startVol;
                for (int i = 0; i < size1; ++i) {
                    dst[i] = src[i] * vol;
                    vol += volStep;
                }
            }
        }
        
        // Second segment (ring buffer wrap)
        if (size2 > 0) {
            for (int ch = 0; ch < 2; ++ch) {
                const float* src = ringBuffer.getReadPointer(ch, start2);
                float* dst = info.buffer->getWritePointer(ch, info.startSample + size1);
                float vol = startVol + volStep * size1;
                for (int i = 0; i < size2; ++i) {
                    dst[i] = src[i] * vol;
                    vol += volStep;
                }
            }
        }
        
        smoothedVolume = targetVol;
        fifo.finishedRead(size1 + size2);
    }
    
    // Clear any unfilled samples
    if (toRead < numSamples) 
        info.buffer->clear(info.startSample + toRead, numSamples - toRead);
}