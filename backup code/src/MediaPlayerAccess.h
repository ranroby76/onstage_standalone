// #D:\Workspace\onstage_colosseum_upgrade\src\MediaPlayerAccess.h
// ==============================================================================
//  MediaPlayerAccess.h
//  OnStage — Platform-specific media player typedef
//
//  Include this header wherever you need access to the media player type.
//  The actual media player instance lives in SubterraneumAudioProcessor.
// ==============================================================================

#pragma once

#if JUCE_WINDOWS
  #include "VLCMediaPlayer_Desktop.h"
  using MediaPlayerType = VLCMediaPlayer_Desktop;
#elif JUCE_MAC
  // TODO: AVFMediaPlayer_Mac integration (future)
  #include "NullMediaPlayer.h"
  using MediaPlayerType = NullMediaPlayer;
#else
  #include "NullMediaPlayer.h"
  using MediaPlayerType = NullMediaPlayer;
#endif
