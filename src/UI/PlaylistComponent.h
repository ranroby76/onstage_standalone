// * **Fix:** Updated constructor to take `IOSettingsManager`. * **Fix:** Added `defaultFolderButton`. <!-- end list -->

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "PlaylistDataStructures.h"
#include "TrackBannerComponent.h"
#include "../AudioEngine.h"
#include "../IOSettingsManager.h"

// ==============================================================================
// Helper: Solid Background Container
// Fixes graphical artifacts (smearing) behind scrolling banners
// ==============================================================================
class PlaylistListContainer : public juce::Component
{
public:
    PlaylistListContainer() { setOpaque(true); }
    
    void paint(juce::Graphics& g) override 
    { 
        // Match the background color of the PlaylistComponent (Task 5 color)
        g.fillAll(juce::Colour(0xFF222222)); 
    }
};

// ==============================================================================
// Playlist Component
// ==============================================================================
class PlaylistComponent : public juce::Component, private juce::Timer
{
public:
    // FIX: Require IOSettingsManager
    PlaylistComponent(AudioEngine& engine, IOSettingsManager& settings);
    ~PlaylistComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Logic
    void addTrack(const juce::File& file);
    void playTrack(int index);
    void removeTrack(int index);
    void selectTrack(int index);
    void clearPlaylist();
    
    // NEW: Called by main PLAY button
    void playSelectedTrack();
    int getCurrentTrackIndex() const;

private:
    void timerCallback() override;
    void rebuildList();
    void updateBannerVisuals();

    // Save/Load Playlist to specific files
    void savePlaylist();
    void loadPlaylist();
    
    // NEW: Set Default Folder
    void setDefaultFolder();

    AudioEngine& audioEngine;
    IOSettingsManager& ioSettings;
    
    std::vector<PlaylistItem> playlist;
    int currentTrackIndex = -1;
    bool autoPlayEnabled = true;
    
    bool waitingForTransition = false;
    int transitionCountdown = 0;
    
    // Prevents double-triggering crossfade
    bool hasTriggeredCrossfade = false;

    // UI
    juce::Label headerLabel;
    juce::ToggleButton autoPlayToggle;
    
    // NEW BUTTON
    juce::TextButton defaultFolderButton; 
    
    juce::TextButton addTrackButton;
    juce::TextButton clearButton;
    juce::TextButton saveButton;
    juce::TextButton loadButton;

    juce::Viewport viewport;
    
    PlaylistListContainer listContainer; 
    
    juce::OwnedArray<TrackBannerComponent> banners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PlaylistComponent)
};
