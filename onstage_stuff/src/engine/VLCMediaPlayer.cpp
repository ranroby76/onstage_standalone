#include "VLCMediaPlayer.h"
#include <cstring>
#include <juce_core/juce_core.h>

#if JUCE_LINUX
#include <stdlib.h>
#endif

VLCMediaPlayer::VLCMediaPlayer()
{
    // CRITICAL FIX: Ensure VLC finds its plugins relative to the executable
    juce::File appDir = juce::File::getSpecialLocation(juce::File::currentApplicationFile).getParentDirectory();
    juce::File pluginDir = appDir.getChildFile("plugins");
    
    #if JUCE_WINDOWS
        juce::String pathEnv = "VLC_PLUGIN_PATH=" + pluginDir.getFullPathName();
        _putenv(pathEnv.toRawUTF8());
    #elif JUCE_LINUX
        // On Linux, if using system VLC, we typically don't need this.
        // But if you bundle libs, use setenv.
        // setenv("VLC_PLUGIN_PATH", "/path/to/plugins", 1);
    #endif

    // FIX: Added --vout=vmem to explicitly enable Video Memory output callbacks
    const char* args[] = { 
        "--aout=amem", 
        "--vout=vmem",
        "--no-video-title-show",
        "--no-osd"
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
            libvlc_video_set_format(m_mediaPlayer, "RV32", videoWidth, videoHeight, videoWidth * 4);
            
            // Initialize images only if player exists
            currentVideoImage = juce::Image(juce::Image::ARGB, videoWidth, videoHeight, true);
            bufferVideoImage = juce::Image(juce::Image::ARGB, videoWidth, videoHeight, true);
            
            // Clear to black initially
            currentVideoImage.clear(currentVideoImage.getBounds(), juce::Colours::black);
            bufferVideoImage.clear(bufferVideoImage.getBounds(), juce::Colours::black);
        }
    }
    else
    {
        // Log critical failure (Debugger only)
        DBG("CRITICAL: libvlc_new failed. Check plugin path: " + pluginDir.getFullPathName());
    }
}

VLCMediaPlayer::~VLCMediaPlayer()
{
    stop();
    if (m_mediaPlayer) libvlc_media_player_release(m_mediaPlayer);
    if (m_instance) libvlc_release(m_instance);
}

bool VLCMediaPlayer::prepareToPlay(int samplesPerBlock, double sampleRate)
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

    isPrepared = true;
    return true;
}

void VLCMediaPlayer::releaseResources()
{
    stop();
    fifo.reset();
    ringBuffer.clear();
    isPrepared = false;
}

bool VLCMediaPlayer::loadFile(const juce::String& path)
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

void VLCMediaPlayer::play()
{
    if (m_mediaPlayer) 
    {
        int rate = (currentSampleRate > 0) ? static_cast<int>(currentSampleRate) : 44100;
        libvlc_audio_set_format(m_mediaPlayer, "S16N", rate, 2);
             
        libvlc_media_player_play(m_mediaPlayer);
    }
}

void VLCMediaPlayer::pause()
{
    if (m_mediaPlayer) libvlc_media_player_pause(m_mediaPlayer);
}

void VLCMediaPlayer::stop()
{
    if (m_mediaPlayer) libvlc_media_player_stop(m_mediaPlayer);
    
    juce::ScopedLock sl(audioLock);
    fifo.reset();
    ringBuffer.clear();
    
    juce::ScopedLock slV(videoLockMutex);
    // Instead of clear, fill with black
    if (currentVideoImage.isValid())
        currentVideoImage.clear(currentVideoImage.getBounds(), juce::Colours::black);
}

void VLCMediaPlayer::setVolume(float newVolume)
{
    volume = juce::jlimit(0.0f, 20.0f, newVolume);
}

float VLCMediaPlayer::getVolume() const { return volume; }

void VLCMediaPlayer::setRate(float newRate)
{
    if (m_mediaPlayer) libvlc_media_player_set_rate(m_mediaPlayer, newRate);
}

float VLCMediaPlayer::getRate() const
{
    return m_mediaPlayer ? libvlc_media_player_get_rate(m_mediaPlayer) : 1.0f;
}

bool VLCMediaPlayer::hasFinished() const
{
    if (!m_mediaPlayer) return false;
    return libvlc_media_player_get_state(m_mediaPlayer) == libvlc_Ended;
}

bool VLCMediaPlayer::isPlaying() const
{
    if (!m_mediaPlayer) return false;
    return libvlc_media_player_is_playing(m_mediaPlayer) != 0;
}

float VLCMediaPlayer::getPosition() const
{
    if (!m_mediaPlayer) return 0.0f;
    return libvlc_media_player_get_position(m_mediaPlayer);
}

void VLCMediaPlayer::setPosition(float pos)
{
    if (m_mediaPlayer) libvlc_media_player_set_position(m_mediaPlayer, pos);
}

int64_t VLCMediaPlayer::getLengthMs() const
{
    if (!m_mediaPlayer) return 0;
    return libvlc_media_player_get_length(m_mediaPlayer);
}

void* VLCMediaPlayer::videoLock(void* data, void** planes)
{
    auto* self = static_cast<VLCMediaPlayer*>(data);
    if (self && self->bufferVideoImage.isValid())
    {
        juce::Image::BitmapData bitmapData(self->bufferVideoImage, juce::Image::BitmapData::readWrite);
        *planes = bitmapData.data;
        return nullptr;
    }
    return nullptr;
}

void VLCMediaPlayer::videoUnlock(void* data, void* picture, void* const* planes)
{
    juce::ignoreUnused(data, picture, planes);
}

void VLCMediaPlayer::videoDisplay(void* data, void* picture)
{
    auto* self = static_cast<VLCMediaPlayer*>(data);
    if (self)
    {
        juce::ScopedLock sl(self->videoLockMutex);
        if (self->bufferVideoImage.isValid())
            self->currentVideoImage = self->bufferVideoImage.createCopy();
    }
}

juce::Image VLCMediaPlayer::getCurrentVideoFrame()
{
    juce::ScopedLock sl(videoLockMutex);
    if (currentVideoImage.isValid())
        return currentVideoImage;
    return juce::Image(); // Return null image if not valid
}

void VLCMediaPlayer::audioPlay(void* data, const void* samples, unsigned count, int64_t pts) {
    auto* self = static_cast<VLCMediaPlayer*>(data);
    if (self) self->addAudioSamples(samples, count, pts);
}
void VLCMediaPlayer::audioPause(void*, int64_t) {}
void VLCMediaPlayer::audioResume(void*, int64_t) {}
void VLCMediaPlayer::audioFlush(void* data, int64_t) {
    auto* self = static_cast<VLCMediaPlayer*>(data);
    if (self) { juce::ScopedLock sl(self->audioLock); self->fifo.reset(); }
}
void VLCMediaPlayer::audioDrain(void*) {}

void VLCMediaPlayer::addAudioSamples(const void* samples, unsigned count, int64_t pts)
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

void VLCMediaPlayer::getNextAudioBlock(const juce::AudioSourceChannelInfo& info)
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
        
        if (size1 > 0) {
            for (int ch = 0; ch < 2; ++ch) 
                info.buffer->addFrom(ch, info.startSample, ringBuffer, ch, start1, size1, volume);
        }
        if (size2 > 0) {
            for (int ch = 0; ch < 2; ++ch)
                info.buffer->addFrom(ch, info.startSample + size1, ringBuffer, ch, start2, size2, volume);
        }
        fifo.finishedRead(size1 + size2);
    }
    
    if (toRead < numSamples) 
        info.buffer->clear(info.startSample + toRead, numSamples - toRead);
}