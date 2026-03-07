// #D:\Workspace\onstage_colosseum_upgrade\src\NullMediaPlayer.h
#pragma once
// ============================================================================
//  NullMediaPlayer — stub for Linux (no media playback backend yet)
// ============================================================================

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>

class NullMediaPlayer
{
public:
    NullMediaPlayer() = default;
    ~NullMediaPlayer() = default;

    void prepareToPlay(int, double) {}
    void releaseResources() {}

    bool loadFile(const juce::String&) { return false; }
    void play(const juce::String&) {}
    void play() {}
    void pause() {}
    void stop() {}

    bool isPlaying() const   { return false; }
    bool isPaused() const    { return false; }
    bool hasFinished() const { return false; }

    float getPosition() const       { return 0.0f; }
    void  setPosition(float)        {}
    int64_t getLengthMs() const     { return 0; }

    float getVolume() const         { return 1.0f; }
    void  setVolume(float)          {}

    float getRate() const           { return 1.0f; }
    void  setRate(float)            {}

    juce::Image getCurrentVideoFrame() { return juce::Image(); }
    void attachVideoComponent(juce::Component*) {}

    void getNextAudioBlock(const juce::AudioSourceChannelInfo& info) {
        info.clearActiveBufferRegion();
    }
};
