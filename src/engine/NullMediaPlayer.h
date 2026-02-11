#pragma once
// ============================================================================
//  NullMediaPlayer — stub for Linux (no media playback backend yet)
// ============================================================================

#include <juce_core/juce_core.h>

class NullMediaPlayer
{
public:
    NullMediaPlayer() = default;
    ~NullMediaPlayer() = default;

    void initialise()          {}
    void shutdown()            {}

    bool loadFile (const juce::File&) { return false; }
    void play()                {}
    void pause()               {}
    void stop()                {}

    bool isPlaying()   const   { return false; }
    bool isPaused()    const   { return false; }
    bool isLoaded()    const   { return false; }

    double getLengthInSeconds() const   { return 0.0; }
    double getPositionInSeconds() const { return 0.0; }
    void   setPosition (double)        {}

    float  getVolume() const           { return 1.0f; }
    void   setVolume (float)           {}

    int    getPlaybackRate() const     { return 0; }
    void   setPlaybackRate (int)       {}

    int    getPitchSemitones() const   { return 0; }
    void   setPitchSemitones (int)     {}
};
