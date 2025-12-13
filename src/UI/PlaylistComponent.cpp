#include "PlaylistComponent.h"
#include "../RegistrationManager.h"

using namespace juce;

PlaylistComponent::PlaylistComponent(AudioEngine& engine, IOSettingsManager& settings)
    : audioEngine(engine), ioSettings(settings)
{
    addAndMakeVisible(headerLabel);
    headerLabel.setText("PLAYLIST", dontSendNotification);
    headerLabel.setFont(Font(18.0f, Font::bold));
    headerLabel.setColour(Label::textColourId, Colour(0xFFD4AF37)); 
    headerLabel.setJustificationType(Justification::centredLeft);

    addAndMakeVisible(autoPlayToggle);
    autoPlayToggle.setButtonText("Auto-Play");
    autoPlayToggle.setToggleState(true, dontSendNotification);
    autoPlayToggle.setColour(ToggleButton::textColourId, Colours::white);
    autoPlayToggle.setColour(ToggleButton::tickColourId, Colour(0xFFD4AF37));
    autoPlayToggle.onClick = [this] { autoPlayEnabled = autoPlayToggle.getToggleState(); };

    addAndMakeVisible(defaultFolderButton);
    defaultFolderButton.setButtonText("Set Default Folder");
    defaultFolderButton.setColour(TextButton::buttonColourId, Colour(0xFF404040));
    defaultFolderButton.onClick = [this] { setDefaultFolder(); };

    addAndMakeVisible(addTrackButton);
    addTrackButton.setButtonText("Add Files");
    addTrackButton.setColour(TextButton::buttonColourId, Colour(0xFF404040));
    addTrackButton.onClick = [this] {
        File startDir = File::getSpecialLocation(File::userMusicDirectory);
        String savedPath = ioSettings.getMediaFolder();
        if (savedPath.isNotEmpty()) {
            File f(savedPath);
            if (f.isDirectory()) startDir = f;
        }

        auto fc = std::make_shared<FileChooser>("Select Media Files",
            startDir,
            "*.mp3;*.wav;*.aiff;*.flac;*.ogg;*.m4a;*.mp4;*.avi;*.mov;*.mkv;*.webm;*.mpg;*.mpeg", true);
        fc->launchAsync(FileBrowserComponent::openMode | FileBrowserComponent::canSelectMultipleItems,
            [this, fc](const FileChooser& chooser) {
                auto results = chooser.getResults();
                for (auto& file : results)
                    addTrack(file);
            });
    };

    addAndMakeVisible(clearButton);
    clearButton.setButtonText("Clear");
    clearButton.setColour(TextButton::buttonColourId, Colour(0xFF8B0000)); 
    clearButton.onClick = [this] { clearPlaylist(); };

    addAndMakeVisible(saveButton);
    saveButton.setButtonText("Save List");
    saveButton.setColour(TextButton::buttonColourId, Colour(0xFF2A2A2A));
    saveButton.onClick = [this] { savePlaylist(); };

    addAndMakeVisible(loadButton);
    loadButton.setButtonText("Load List");
    loadButton.setColour(TextButton::buttonColourId, Colour(0xFF2A2A2A));
    loadButton.onClick = [this] { loadPlaylist(); };

    addAndMakeVisible(viewport);
    viewport.setScrollBarsShown(true, false);
    
    viewport.setViewedComponent(&listContainer, false);

    startTimerHz(30);
}

PlaylistComponent::~PlaylistComponent()
{
    stopTimer();
    banners.clear();
}

void PlaylistComponent::paint(Graphics& g)
{
    g.fillAll(Colour(0xFF222222));
}

void PlaylistComponent::resized()
{
    auto area = getLocalBounds().reduced(12);
    auto row1 = area.removeFromTop(35);
    headerLabel.setBounds(row1.removeFromLeft(120).reduced(5, 0));
    autoPlayToggle.setBounds(row1.removeFromRight(100).reduced(5, 0));
    auto rowFolder = area.removeFromTop(35);
    defaultFolderButton.setBounds(rowFolder.reduced(2));
    auto row3 = area.removeFromTop(40);
    int numButtons = 4;
    int btnWidth = row3.getWidth() / numButtons;
    
    addTrackButton.setBounds(row3.removeFromLeft(btnWidth).reduced(2));
    clearButton.setBounds(row3.removeFromLeft(btnWidth).reduced(2));
    saveButton.setBounds(row3.removeFromLeft(btnWidth).reduced(2));
    loadButton.setBounds(row3.removeFromLeft(btnWidth).reduced(2));
    
    viewport.setBounds(area);
    rebuildList();
}

void PlaylistComponent::setDefaultFolder()
{
    auto fc = std::make_shared<FileChooser>("Choose Default Media Folder",
        File::getSpecialLocation(File::userMusicDirectory));
    fc->launchAsync(FileBrowserComponent::openMode | FileBrowserComponent::canSelectDirectories,
        [this, fc](const FileChooser& chooser) {
            auto result = chooser.getResult();
            if (result.isDirectory()) {
                ioSettings.saveMediaFolder(result.getFullPathName());
                NativeMessageBox::showMessageBoxAsync(AlertWindow::InfoIcon, "Success", 
                    "Default media folder set to:\n" + result.getFileName());
            }
        });
}

void PlaylistComponent::addTrack(const File& file)
{
    if (!RegistrationManager::getInstance().isProMode() && playlist.size() >= 3)
    {
        NativeMessageBox::showMessageBoxAsync(AlertWindow::InfoIcon, 
            "Demo Mode", "Playlist is limited to 3 tracks in Demo Mode.\nPlease register to unlock.");
        return;
    }

    PlaylistItem item;
    item.filePath = file.getFullPathName();
    item.title = file.getFileNameWithoutExtension();
    item.volume = 1.0f;
    item.playbackSpeed = 1.0f;
    item.transitionDelaySec = 0;
    item.isCrossfade = false; 
    item.pitch = 0.0f; // Default pitch
    
    playlist.push_back(item);
    if (currentTrackIndex == -1 && playlist.size() == 1)
    {
        selectTrack(0);
    }
    
    rebuildList();
}

void PlaylistComponent::clearPlaylist()
{
    playlist.clear();
    banners.clear();
    currentTrackIndex = -1;
    audioEngine.stopAllPlayback();
    rebuildList();
}

void PlaylistComponent::removeTrack(int index)
{
    if (index >= 0 && index < (int)playlist.size())
    {
        playlist.erase(playlist.begin() + index);
        if (currentTrackIndex == index) 
        {
            currentTrackIndex = -1;
            audioEngine.stopAllPlayback();
        }
        else if (currentTrackIndex > index)
        {
            currentTrackIndex--;
        }
        
        rebuildList();
    }
}

void PlaylistComponent::selectTrack(int index)
{
    if (index < 0 || index >= (int)playlist.size()) return;

    currentTrackIndex = index;
    hasTriggeredCrossfade = false; 
    
    auto& item = playlist[index];
    audioEngine.stopAllPlayback();
    
    auto& player = audioEngine.getMediaPlayer();
    if (player.loadFile(item.filePath))
    {
        player.setVolume(item.volume);
        player.setRate(item.playbackSpeed);
        audioEngine.setBackingTrackPitch(item.pitch);
    }
    
    updateBannerVisuals();
}

void PlaylistComponent::playTrack(int index)
{
    selectTrack(index);
    audioEngine.getMediaPlayer().play();
}

void PlaylistComponent::rebuildList()
{
    banners.clear();
    listContainer.removeAllChildren();
    
    int y = 0;
    for (size_t i = 0; i < playlist.size(); ++i)
    {
        auto& item = playlist[i];
        // Height increased for extra row
        int currentH = item.isExpanded ? 170 : 44;
        
        auto* banner = new TrackBannerComponent((int)i, item, 
            [this, i] { removeTrack((int)i); }, 
            [this, i] { 
                playlist[i].isExpanded = !playlist[i].isExpanded; 
                rebuildList(); 
            }, 
            [this, i] { playTrack((int)i); }, 
            [this, i](float vol) { 
                if (currentTrackIndex == (int)i) audioEngine.getMediaPlayer().setVolume(vol);
            },
            [this, i](float speed) {
                if (currentTrackIndex == (int)i) audioEngine.getMediaPlayer().setRate(speed);
            },
            [this, i](float pitch) {
                if (currentTrackIndex == (int)i) audioEngine.setBackingTrackPitch(pitch);
            }
        );
        
        banner->setBounds(0, y, viewport.getWidth(), currentH);
        listContainer.addAndMakeVisible(banner);
        banners.add(banner);
        
        y += currentH + 2;
    }
    
    listContainer.setSize(viewport.getWidth(), y + 50);
    updateBannerVisuals();
}

void PlaylistComponent::updateBannerVisuals()
{
    for (int i = 0; i < banners.size(); ++i)
    {
        bool isCurrent = (i == currentTrackIndex);
        bool isPlaying = isCurrent && audioEngine.getMediaPlayer().isPlaying();
        banners[i]->setPlaybackState(isCurrent, isPlaying);
    }
}

void PlaylistComponent::timerCallback()
{
    audioEngine.updateCrossfadeState();
    if (autoPlayEnabled && currentTrackIndex >= 0 && currentTrackIndex < (int)playlist.size())
    {
        auto& player = audioEngine.getMediaPlayer();
        if (player.isPlaying() && !hasTriggeredCrossfade)
        {
            auto& currentItem = playlist[currentTrackIndex];
            int nextIndex = currentTrackIndex + 1;
            
            if (nextIndex < (int)playlist.size())
            {
                auto& nextItem = playlist[nextIndex];
                if (currentItem.isCrossfade)
                {
                    int64_t lenMs = player.getLengthMs();
                    float pos = player.getPosition(); 
                    
                    if (lenMs > 0)
                    {
                        int64_t currentMs = (int64_t)(pos * lenMs);
                        int64_t remainingMs = lenMs - currentMs;
                        int64_t overlapMs = currentItem.transitionDelaySec * 1000;
                        if (overlapMs < 100) overlapMs = 100;
                        if (remainingMs <= overlapMs)
                        {
                            hasTriggeredCrossfade = true;
                            double fadeDuration = (double)currentItem.transitionDelaySec;
                            // Trigger crossfade logic (TODO: update AudioEngine triggerCrossfade to accept pitch)
                            // We set it for next track implicitly via load, but crossfade might need explicit support
                            // For now, next track loads with its settings.
                            audioEngine.triggerCrossfade(nextItem.filePath, fadeDuration, nextItem.volume, nextItem.playbackSpeed);
                            // Set pitch for next track immediately
                            audioEngine.setBackingTrackPitch(nextItem.pitch);
                            
                            currentTrackIndex = nextIndex;
                            hasTriggeredCrossfade = false;
                        }
                    }
                }
                else
                {
                    if (player.hasFinished())
                    {
                        if (!waitingForTransition)
                        {
                            waitingForTransition = true;
                            transitionCountdown = currentItem.transitionDelaySec * 30; 
                        }
                    }
                }
            }
        }
        
        if (waitingForTransition)
        {
            if (transitionCountdown-- <= 0)
            {
                waitingForTransition = false;
                int next = currentTrackIndex + 1;
                if (next < (int)playlist.size())
                {
                    playTrack(next);
                }
            }
        }
    }
    updateBannerVisuals();
}

void PlaylistComponent::savePlaylist()
{
    auto fc = std::make_shared<FileChooser>("Save Playlist",
        File::getSpecialLocation(File::userDocumentsDirectory), "*.json");
    fc->launchAsync(FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles,
        [this, fc](const FileChooser& chooser) {
            auto file = chooser.getResult();
            if (file == File{}) return; 

            if (!file.hasFileExtension("json"))
                file = file.withFileExtension("json");

            DynamicObject::Ptr root = new DynamicObject();
            Array<var> tracks;
            
            for (const auto& item : playlist)
            {
                DynamicObject::Ptr obj = new DynamicObject();
                obj->setProperty("path", item.filePath);
                obj->setProperty("title", item.title);
                obj->setProperty("vol", item.volume);
                obj->setProperty("speed", item.playbackSpeed);
                obj->setProperty("pitch", item.pitch); // Save Pitch
                obj->setProperty("delay", item.transitionDelaySec);
                obj->setProperty("xfade", item.isCrossfade);
                tracks.add(obj.get());
            }
            
            root->setProperty("tracks", tracks);
            if (file.replaceWithText(JSON::toString(var(root.get()))))
            {
                NativeMessageBox::showMessageBoxAsync(AlertWindow::InfoIcon, "Success", 
                    "Playlist saved successfully!");
            }
            else
            {
                NativeMessageBox::showMessageBoxAsync(AlertWindow::WarningIcon, "Error", 
                    "Could not write to file.");
            }
        });
}

void PlaylistComponent::loadPlaylist()
{
    auto fc = std::make_shared<FileChooser>("Load Playlist",
        File::getSpecialLocation(File::userDocumentsDirectory), "*.json");
    fc->launchAsync(FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
        [this, fc](const FileChooser& chooser) {
            auto file = chooser.getResult();
            if (!file.existsAsFile()) return;
            
            var json = JSON::parse(file);
            if (auto* root = json.getDynamicObject())
            {
                if (auto* tracks = root->getProperty("tracks").getArray())
                {
                    clearPlaylist();
                    
                    bool isPro = RegistrationManager::getInstance().isProMode();
                    int max = isPro ? 9999 : 3;
                    int count = 0;
                    
                    for (auto& t : *tracks)
                    {
                        if (count >= max) break;
                        
                        if (auto* obj = t.getDynamicObject())
                        {
                            PlaylistItem item;
                            item.filePath = obj->getProperty("path").toString();
                            if (obj->hasProperty("title"))
                                item.title = obj->getProperty("title").toString();
                            else
                                item.ensureTitle();
                            if (File(item.filePath).existsAsFile())
                            {
                                item.volume = (float)obj->getProperty("vol");
                                item.playbackSpeed = (float)obj->getProperty("speed");
                                if (obj->hasProperty("pitch")) item.pitch = (float)obj->getProperty("pitch");
                                item.transitionDelaySec = (int)obj->getProperty("delay");
                                item.isCrossfade = (bool)obj->getProperty("xfade");
                                playlist.push_back(item);
                                count++;
                            }
                        }
                    }
                    
                    rebuildList();
                    if (!playlist.empty()) selectTrack(0);
                }
            }
            else
            {
                NativeMessageBox::showMessageBoxAsync(AlertWindow::WarningIcon, "Error", 
                    "Failed to parse playlist file.");
            }
        });
}