#include "PlaylistComponent.h"
#include "../AudioEngine.h"
#include "../IOSettingsManager.h"

PlaylistComponent::PlaylistComponent(AudioEngine& engine, IOSettingsManager& settings) 
    : audioEngine(engine), ioSettings(settings) 
{
    addAndMakeVisible(headerLabel); 
    headerLabel.setText("PLAYLIST", juce::dontSendNotification);
    headerLabel.setFont(juce::Font(20.0f, juce::Font::bold)); 
    headerLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFD4AF37));
    
    addAndMakeVisible(autoPlayToggle); 
    autoPlayToggle.setButtonText("Auto-Play"); 
    autoPlayToggle.setToggleState(autoPlayEnabled, juce::dontSendNotification);
    autoPlayToggle.onClick = [this]() { autoPlayEnabled = autoPlayToggle.getToggleState(); };
    
    addAndMakeVisible(defaultFolderButton); 
    defaultFolderButton.setButtonText("Set Default Folder");
    defaultFolderButton.onClick = [this]() { setDefaultFolder(); };
    
    addAndMakeVisible(addTrackButton); 
    addTrackButton.setButtonText("Add Tracks");
    addTrackButton.onClick = [this]() {
        auto chooser = std::make_shared<juce::FileChooser>("Select Files", 
            juce::File(ioSettings.getMediaFolder()), 
            "*.mp3;*.wav;*.mp4;*.avi;*.mkv");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectMultipleItems, 
            [this, chooser](const juce::FileChooser& fc) { 
                for (auto& f : fc.getResults()) 
                    addTrack(f); 
            });
    };
    
    addAndMakeVisible(clearButton); 
    clearButton.setButtonText("Clear All"); 
    clearButton.onClick = [this]() { clearPlaylist(); };
    
    addAndMakeVisible(saveButton); 
    saveButton.setButtonText("Save"); 
    saveButton.onClick = [this]() { savePlaylist(); };
    
    addAndMakeVisible(loadButton); 
    loadButton.setButtonText("Load"); 
    loadButton.onClick = [this]() { loadPlaylist(); };
    
    addAndMakeVisible(viewport); 
    viewport.setViewedComponent(&listContainer, false); 
    
    startTimer(100);  // FIX: Changed to 100ms for better countdown accuracy
}

PlaylistComponent::~PlaylistComponent() 
{ 
    stopTimer(); 
}

void PlaylistComponent::paint(juce::Graphics& g) 
{ 
    g.fillAll(juce::Colour(0xFF222222)); 
}

void PlaylistComponent::resized() 
{
    auto bounds = getLocalBounds(); 
    
    // Row 1: PLAYLIST header | Auto-Play | Set Default Folder (stretched)
    auto row1 = bounds.removeFromTop(35);
    headerLabel.setBounds(row1.removeFromLeft(120).reduced(5)); 
    autoPlayToggle.setBounds(row1.removeFromLeft(120)); 
    defaultFolderButton.setBounds(row1.reduced(2));  // Takes remaining space
    
    // Row 2: Add Tracks | Clear All | Save | Load (all equal width)
    auto row2 = bounds.removeFromTop(35); 
    int buttonWidth = row2.getWidth() / 4;
    addTrackButton.setBounds(row2.removeFromLeft(buttonWidth).reduced(2)); 
    clearButton.setBounds(row2.removeFromLeft(buttonWidth).reduced(2)); 
    saveButton.setBounds(row2.removeFromLeft(buttonWidth).reduced(2)); 
    loadButton.setBounds(row2.reduced(2));  // Takes remaining space
    
    viewport.setBounds(bounds); 
    rebuildList();
}

void PlaylistComponent::addTrack(const juce::File& file) 
{ 
    PlaylistItem item; 
    item.filePath = file.getFullPathName(); 
    item.title = file.getFileNameWithoutExtension(); 
    item.volume = 1.0f; 
    item.playbackSpeed = 1.0f; 
    playlist.push_back(item); 
    rebuildList(); 
}

void PlaylistComponent::clearPlaylist() 
{ 
    audioEngine.getMediaPlayer().stop(); 
    currentTrackIndex = -1; 
    playlist.clear(); 
    rebuildList(); 
}

void PlaylistComponent::removeTrack(int index) 
{ 
    if (index < 0 || index >= (int)playlist.size()) return; 
    
    if (index == currentTrackIndex) 
        audioEngine.getMediaPlayer().stop(); 
    
    playlist.erase(playlist.begin() + index); 
    
    if (index < currentTrackIndex) 
        currentTrackIndex--; 
    else if (index == currentTrackIndex) 
        currentTrackIndex = -1; 
    
    rebuildList(); 
}

void PlaylistComponent::selectTrack(int index) 
{ 
    // Just select, don't play!
    if (index < 0 || index >= (int)playlist.size()) 
        return;
    
    currentTrackIndex = index;
    updateBannerVisuals();
}

void PlaylistComponent::playSelectedTrack()
{
    // Called by MediaPage main PLAY button
    if (currentTrackIndex < 0 || currentTrackIndex >= (int)playlist.size()) 
        return;
    
    auto& item = playlist[currentTrackIndex]; 
    auto& player = audioEngine.getMediaPlayer();
    
    player.stop(); 
    
    if (player.loadFile(item.filePath)) 
    { 
        player.setVolume(item.volume); 
        player.setRate(item.playbackSpeed); 
        player.play(); 
    }
}

int PlaylistComponent::getCurrentTrackIndex() const
{
    return currentTrackIndex;
}

void PlaylistComponent::playTrack(int index) 
{
    // This is for auto-play only
    if (index < 0 || index >= (int)playlist.size()) 
        return;
    
    currentTrackIndex = index; 
    auto& item = playlist[index]; 
    auto& player = audioEngine.getMediaPlayer();
    
    player.stop(); 
    
    if (player.loadFile(item.filePath)) 
    { 
        player.setVolume(item.volume); 
        player.setRate(item.playbackSpeed); 
        player.play(); 
    } 
    
    updateBannerVisuals();
}

void PlaylistComponent::savePlaylist() 
{ 
    // Save logic here
}

void PlaylistComponent::loadPlaylist() 
{ 
    // Load logic here
}

void PlaylistComponent::setDefaultFolder() 
{ 
    // FIX: Implement Set Default Folder functionality
    auto chooser = std::make_shared<juce::FileChooser>(
        "Select Default Media Folder", 
        juce::File(ioSettings.getMediaFolder())
    );
    
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
        [this, chooser](const juce::FileChooser& fc) 
        {
            auto result = fc.getResult();
            if (result.exists() && result.isDirectory())
            {
                // FIX: Use correct method name - saveMediaFolder already saves to disk
                ioSettings.saveMediaFolder(result.getFullPathName());
                
                juce::AlertWindow::showMessageBoxAsync(
                    juce::AlertWindow::InfoIcon,
                    "Default Folder Set",
                    "Default media folder set to:\n" + result.getFullPathName(),
                    "OK"
                );
            }
        }
    );
}

void PlaylistComponent::rebuildList() 
{
    banners.clear(); 
    
    int totalHeight = 0;
    int w = viewport.getWidth() - viewport.getScrollBarThickness();
    
    for (size_t i = 0; i < playlist.size(); ++i) 
    {
        auto* b = new TrackBannerComponent(
            (int)i, 
            playlist[i], 
            [this, i]() { removeTrack((int)i); },           // onRemove
            [this, i]() {                                    // onExpandToggle
                playlist[i].isExpanded = !playlist[i].isExpanded; 
                rebuildList(); 
            },
            []() { /* onBannerClick - DO NOTHING! */ },     // Banner click does nothing
            [this, i]() {                                    // Green button - STOP and JUMP
                audioEngine.getMediaPlayer().stop();         // Stop current playback
                currentTrackIndex = (int)i;                  // Select this track
                playTrack((int)i);                           // Play immediately
            },
            [this, i](float v) {                            // onVolChange
                playlist[i].volume = v; 
                if ((int)i == currentTrackIndex) 
                    audioEngine.getMediaPlayer().setVolume(v); 
            },
            [this, i](float s) {                            // onSpeedChange
                playlist[i].playbackSpeed = s; 
                if ((int)i == currentTrackIndex) 
                    audioEngine.getMediaPlayer().setRate(s); 
            }
        );
        
        banners.add(b); 
        listContainer.addAndMakeVisible(b); 
        
        int h = playlist[i].isExpanded ? 140 : 44;  // FIX: Reduced height (removed pitch slider)
        b->setBounds(0, totalHeight, w, h); 
        totalHeight += h;
    }
    
    listContainer.setSize(w, totalHeight);
    
    // FIXED #3: Always keep at least one track selected when playlist is not empty
    if (!playlist.empty() && currentTrackIndex < 0) {
        currentTrackIndex = 0;
        updateBannerVisuals();
    }
}

void PlaylistComponent::updateBannerVisuals() 
{ 
    for (int i = 0; i < banners.size(); ++i) 
    {
        banners[i]->setPlaybackState(
            i == currentTrackIndex, 
            (i == currentTrackIndex && audioEngine.getMediaPlayer().isPlaying())
        ); 
    }
}

void PlaylistComponent::timerCallback() 
{ 
    // FIX: Implement Wait delay countdown
    if (waitingForTransition)
    {
        transitionCountdown -= 100;  // Subtract 100ms per timer tick
        
        if (transitionCountdown <= 0)
        {
            // Time's up! Play next track
            waitingForTransition = false;
            transitionCountdown = 0;
            
            if (currentTrackIndex < (int)playlist.size())
                playTrack(currentTrackIndex);
        }
    }
    else if (autoPlayEnabled && currentTrackIndex >= 0 && audioEngine.getMediaPlayer().hasFinished()) 
    { 
        // Current track finished
        int nextIndex = currentTrackIndex + 1;
        
        if (nextIndex < (int)playlist.size())
        {
            // FIX: Check if previous track has a wait delay
            int waitSeconds = playlist[currentTrackIndex].transitionDelaySec;
            
            if (waitSeconds > 0)
            {
                // Start countdown
                currentTrackIndex = nextIndex;  // Select next track
                waitingForTransition = true;
                transitionCountdown = waitSeconds * 1000;  // Convert to milliseconds
                updateBannerVisuals();  // Show next track as selected while waiting
            }
            else
            {
                // No wait - play immediately
                currentTrackIndex = nextIndex;
                playTrack(currentTrackIndex);
            }
        }
    } 
    
    updateBannerVisuals(); 
}
