// #D:\Workspace\onstage_colosseum_upgrade\src\MediaPage.cpp
#include "MediaPage.h"
#include "PluginProcessor.h"

MediaPage::MediaPage(SubterraneumAudioProcessor& proc) : processor(proc),
    progressSlider(juce::Slider::LinearBar, juce::Slider::NoTextBox) 
{
    videoSurface = std::make_unique<VideoSurfaceComponent>(processor);
    addAndMakeVisible(videoSurface.get());
    
    playlistComponent = std::make_unique<PlaylistComponent>(processor);
    addAndMakeVisible(playlistComponent.get());

    // Transport Controls
    addAndMakeVisible(playPauseBtn);
    playPauseBtn.setButtonText("PLAY");
    playPauseBtn.setMidiInfo("MIDI: Note 15");
    playPauseBtn.setLookAndFeel(&greenButtonLF);
    playPauseBtn.onClick = [this] { 
        auto& player = processor.getMediaPlayer();
        
        if (playlistComponent->getCurrentTrackIndex() >= 0) {
            if (player.isPlaying()) {
                player.pause();
            } else if (player.isPaused()) {
                player.play();  // Resume
            } else {
                playlistComponent->playSelectedTrack();  // Start playing
            }
        }
    };

    addAndMakeVisible(stopBtn);
    stopBtn.setButtonText("STOP");
    stopBtn.setMidiInfo("MIDI: Note 16");
    stopBtn.setLookAndFeel(&redButtonLF);
    stopBtn.onClick = [this] { 
        processor.getMediaPlayer().stop();
        playPauseBtn.setButtonText("PLAY");
        progressSlider.setValue(0.0, juce::dontSendNotification);
    };

    // Sliders & Labels
    addAndMakeVisible(progressSlider);
    progressSlider.setRange(0.0, 1.0, 0.001);
    progressSlider.setColour(juce::Slider::trackColourId, juce::Colour(0xFFD4AF37));
    progressSlider.setColour(juce::Slider::backgroundColourId, juce::Colour(0xFF404040));
    
    progressSlider.onDragStart = [this] { isUserDraggingSlider = true; };
    progressSlider.onDragEnd = [this] { isUserDraggingSlider = false; };
    
    progressSlider.onValueChange = [this] {
        if (progressSlider.isMouseButtonDown() || isUserDraggingSlider) {
            processor.getMediaPlayer().setPosition((float)progressSlider.getValue());
        }
    };

    // Clocks
    addAndMakeVisible(currentTimeLabel);
    currentTimeLabel.setText("00:00", juce::dontSendNotification);
    currentTimeLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    currentTimeLabel.setJustificationType(juce::Justification::centredLeft);
    currentTimeLabel.setFont(juce::Font(14.0f, juce::Font::bold));

    addAndMakeVisible(totalTimeLabel);
    totalTimeLabel.setText("00:00", juce::dontSendNotification);
    totalTimeLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    totalTimeLabel.setJustificationType(juce::Justification::centredRight);
    totalTimeLabel.setFont(juce::Font(14.0f, juce::Font::bold));

    startTimerHz(30);
}

MediaPage::~MediaPage() 
{ 
    stopTimer(); 
    playPauseBtn.setLookAndFeel(nullptr);
    stopBtn.setLookAndFeel(nullptr);
}

juce::String MediaPage::formatTime(double seconds) const
{
    if (seconds < 0) seconds = 0;
    int totalSeconds = (int)seconds;
    int m = totalSeconds / 60;
    int s = totalSeconds % 60;
    return juce::String::formatted("%02d:%02d", m, s);
}

void MediaPage::timerCallback()
{
    auto& player = processor.getMediaPlayer();
    bool playing = player.isPlaying();
    playPauseBtn.setButtonText(playing ? "PAUSE" : "PLAY");
    
    if (!isUserDraggingSlider && !progressSlider.isMouseButtonDown() && playing)
    {
        progressSlider.setValue(player.getPosition(), juce::dontSendNotification);
    }

    double lenMs = (double)player.getLengthMs(); 
    double posRatio = player.getPosition();
    double currentMs = lenMs * posRatio;

    totalTimeLabel.setText(formatTime(lenMs / 1000.0), juce::dontSendNotification);
    currentTimeLabel.setText(formatTime(currentMs / 1000.0), juce::dontSendNotification);
}

void MediaPage::paint(juce::Graphics& g) 
{ 
    g.fillAll(juce::Colour(0xFF000000)); 
}

void MediaPage::resized()
{
    auto area = getLocalBounds();
    int totalWidth = area.getWidth();
    
    // Playlist takes 26% (reduced from 35% - which is 25% smaller)
    // Video area takes 74% (increased from 65%)
    int playlistWidth = (int)(totalWidth * 0.26f);
    int playerAreaWidth = totalWidth - playlistWidth;
    
    auto playerArea = area.removeFromLeft(playerAreaWidth);
    playlistComponent->setBounds(area);
    
    int transportHeight = 30;
    auto transportArea = playerArea.removeFromBottom(transportHeight);
    
    int buttonWidth = transportArea.getWidth() / 2;
    playPauseBtn.setBounds(transportArea.removeFromLeft(buttonWidth).reduced(2));
    stopBtn.setBounds(transportArea.reduced(2));
    
    int sliderHeight = 20;
    int labelHeight = 15;
    
    auto sliderStrip = playerArea.removeFromBottom(sliderHeight + labelHeight + 5); 
    
    auto labelRow = sliderStrip.removeFromTop(labelHeight);
    totalTimeLabel.setBounds(labelRow.removeFromLeft(60));
    currentTimeLabel.setBounds(labelRow.removeFromRight(60));
    
    progressSlider.setBounds(sliderStrip.reduced(2));

    // Video Area — maintain 16:9 aspect ratio
    int availableWidth = playerArea.getWidth();
    int availableHeight = playerArea.getHeight();
    int targetVideoWidth = availableWidth;
    int targetVideoHeight = (int)(targetVideoWidth * (9.0f / 16.0f));
    if (targetVideoHeight > availableHeight) {
        targetVideoHeight = availableHeight;
        targetVideoWidth = (int)(targetVideoHeight * (16.0f / 9.0f));
    }
    int xOffset = (availableWidth - targetVideoWidth) / 2;
    int yOffset = (availableHeight - targetVideoHeight) / 2;

    videoSurface->setBounds(playerArea.getX() + xOffset, playerArea.getY() + yOffset, targetVideoWidth, targetVideoHeight);
}
