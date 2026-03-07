// #D:\Workspace\onstage_colosseum_upgrade\src\PlaylistComponent.cpp
#include "PlaylistComponent.h"
#include "PluginProcessor.h"

PlaylistComponent::PlaylistComponent(SubterraneumAudioProcessor& proc) 
    : processor(proc)
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
            juce::File(getMediaFolder()), 
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
    
    startTimer(100);
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
    
    auto row1 = bounds.removeFromTop(35);
    headerLabel.setBounds(row1.removeFromLeft(120).reduced(5)); 
    autoPlayToggle.setBounds(row1.removeFromLeft(120)); 
    defaultFolderButton.setBounds(row1.reduced(2));
    
    auto row2 = bounds.removeFromTop(35); 
    int buttonWidth = row2.getWidth() / 4;
    addTrackButton.setBounds(row2.removeFromLeft(buttonWidth).reduced(2)); 
    clearButton.setBounds(row2.removeFromLeft(buttonWidth).reduced(2)); 
    saveButton.setBounds(row2.removeFromLeft(buttonWidth).reduced(2)); 
    loadButton.setBounds(row2.reduced(2));
    
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
    processor.getMediaPlayer().stop(); 
    currentTrackIndex = -1; 
    playlist.clear(); 
    rebuildList(); 
}

void PlaylistComponent::removeTrack(int index) 
{ 
    if (index < 0 || index >= (int)playlist.size()) return; 
    
    if (index == currentTrackIndex) 
        processor.getMediaPlayer().stop(); 
    
    playlist.erase(playlist.begin() + index); 
    
    if (index < currentTrackIndex) 
        currentTrackIndex--; 
    else if (index == currentTrackIndex) 
        currentTrackIndex = -1; 
    
    rebuildList(); 
}

void PlaylistComponent::selectTrack(int index) 
{ 
    if (index < 0 || index >= (int)playlist.size()) 
        return;
    
    currentTrackIndex = index;
    updateBannerVisuals();
}

void PlaylistComponent::playSelectedTrack()
{
    if (currentTrackIndex < 0 || currentTrackIndex >= (int)playlist.size()) 
        return;
    
    auto& item = playlist[currentTrackIndex]; 
    auto& player = processor.getMediaPlayer();
    
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
    if (index < 0 || index >= (int)playlist.size()) 
        return;
    
    currentTrackIndex = index; 
    auto& item = playlist[index]; 
    auto& player = processor.getMediaPlayer();
    
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
    // TODO: Save playlist to file
}

void PlaylistComponent::loadPlaylist() 
{ 
    // TODO: Load playlist from file
}

juce::String PlaylistComponent::getMediaFolder() const
{
    if (auto* settings = processor.appProperties.getUserSettings())
        return settings->getValue("mediaFolder", 
            juce::File::getSpecialLocation(juce::File::userMusicDirectory).getFullPathName());
    return juce::File::getSpecialLocation(juce::File::userMusicDirectory).getFullPathName();
}

void PlaylistComponent::saveMediaFolder(const juce::String& path)
{
    if (auto* settings = processor.appProperties.getUserSettings())
    {
        settings->setValue("mediaFolder", path);
        settings->saveIfNeeded();
    }
}

void PlaylistComponent::setDefaultFolder() 
{ 
    auto chooser = std::make_shared<juce::FileChooser>(
        "Select Default Media Folder", 
        juce::File(getMediaFolder())
    );
    
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
        [this, chooser](const juce::FileChooser& fc) 
        {
            auto result = fc.getResult();
            if (result.exists() && result.isDirectory())
            {
                saveMediaFolder(result.getFullPathName());
                
                juce::AlertWindow::showMessageBoxAsync(
                    juce::MessageBoxIconType::InfoIcon,
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
            [this, i]() { removeTrack((int)i); },
            [this, i]() {
                playlist[i].isExpanded = !playlist[i].isExpanded; 
                rebuildList(); 
            },
            []() { /* onBannerClick - DO NOTHING */ },
            [this, i]() {
                processor.getMediaPlayer().stop();
                currentTrackIndex = (int)i;
                playTrack((int)i);
            },
            [this, i](float v) {
                playlist[i].volume = v; 
                if ((int)i == currentTrackIndex) 
                    processor.getMediaPlayer().setVolume(v); 
            },
            [this, i](float s) {
                playlist[i].playbackSpeed = s; 
                if ((int)i == currentTrackIndex) 
                    processor.getMediaPlayer().setRate(s); 
            }
        );
        
        banners.add(b); 
        listContainer.addAndMakeVisible(b); 
        
        int h = playlist[i].isExpanded ? 140 : 44;
        b->setBounds(0, totalHeight, w, h); 
        totalHeight += h;
    }
    
    listContainer.setSize(w, totalHeight);
    
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
            (i == currentTrackIndex && processor.getMediaPlayer().isPlaying())
        ); 
    }
}

void PlaylistComponent::timerCallback() 
{ 
    if (waitingForTransition)
    {
        transitionCountdown -= 100;
        
        if (transitionCountdown <= 0)
        {
            waitingForTransition = false;
            transitionCountdown = 0;
            
            if (currentTrackIndex < (int)playlist.size())
                playTrack(currentTrackIndex);
        }
    }
    else if (autoPlayEnabled && currentTrackIndex >= 0 && processor.getMediaPlayer().hasFinished()) 
    { 
        int nextIndex = currentTrackIndex + 1;
        
        if (nextIndex < (int)playlist.size())
        {
            int waitSeconds = playlist[currentTrackIndex].transitionDelaySec;
            
            if (waitSeconds > 0)
            {
                currentTrackIndex = nextIndex;
                waitingForTransition = true;
                transitionCountdown = waitSeconds * 1000;
                updateBannerVisuals();
            }
            else
            {
                currentTrackIndex = nextIndex;
                playTrack(currentTrackIndex);
            }
        }
    } 
    
    updateBannerVisuals(); 
}
