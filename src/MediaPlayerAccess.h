// #D:\Workspace\onstage_colosseum_upgrade\src\MediaPlayerAccess.h
// ==============================================================================
//  MediaPlayerAccess.h
//  OnStage — Platform-specific media player typedef
//
//  Windows & macOS: VLC-powered playback
//  Linux: NullMediaPlayer (no backend yet)
// ==============================================================================

#pragma once

#if JUCE_WINDOWS || JUCE_MAC
  #include "VLCMediaPlayer_Desktop.h"
  using MediaPlayerType = VLCMediaPlayer_Desktop;
#else
  #include "NullMediaPlayer.h"
  using MediaPlayerType = NullMediaPlayer;
#endif
