/*
  ==============================================================================

    VLCMediaPlayer.h
    OnStage

    Acts as a facade to select between the Desktop (VLC) and Apple (Native) 
    media player implementations.

  ==============================================================================
*/

#ifndef ONSTAGE_ENGINE_VLC_MEDIA_PLAYER_FACADE_H
#define ONSTAGE_ENGINE_VLC_MEDIA_PLAYER_FACADE_H

#include <juce_core/juce_core.h>

#if JUCE_MAC || JUCE_IOS
    // Use Native AVFoundation for both macOS and iOS
    #include "NativeMediaPlayer_Apple.h"
    using VLCMediaPlayer = NativeMediaPlayer_Apple;
#else
    // Use VLC for Windows and Linux
    #include "VLCMediaPlayer_Desktop.h"
    using VLCMediaPlayer = VLCMediaPlayer_Desktop;
#endif

#endif // ONSTAGE_ENGINE_VLC_MEDIA_PLAYER_FACADE_H