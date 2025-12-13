// **Changes:** Implemented time formatting and label updates.

#include "MediaPage.h"

MediaPage::MediaPage(AudioEngine& engine, IOSettingsManager& settings) : audioEngine(engine),
    progressSlider(juce::Slider::LinearBar, juce::Slider::NoTextBox) 
{
    videoSurface = std::make_unique<VideoSurfaceComponent>(engine);
    addAndMakeVisible(videoSurface.get());
    
    playlistComponent = std::make_unique<PlaylistComponent>(engine, settings);
    addAndMakeVisible(playlistComponent.get());

    // Transport Controls
    addAndMakeVisible(playPauseBtn);
    playPauseBtn.setButtonText("PLAY");
    playPauseBtn.setMidiInfo("MIDI: Note 15");
    playPauseBtn.onClick = [this] { 
        auto& player = audioEngine.getMediaPlayer();
        if (player.isPlaying()) player.pause();
        else player.play();
    };

    addAndMakeVisible(stopBtn);
    stopBtn.setButtonText("STOP");
    stopBtn.setMidiInfo("MIDI: Note 16");
    stopBtn.onClick = [this] { 
        audioEngine.stopAllPlayback();
        playPauseBtn.setButtonText("PLAY");
        progressSlider.setValue(0.0, juce::dontSendNotification);
    };

    // Sliders & Labels
    addAndMakeVisible(progressSlider);
    progressSlider.setRange(0.0, 1.0, 0.001);
    progressSlider.setColour(juce::Slider::trackColourId, juce::Colour(0xFFD4AF37));
    progressSlider.setColour(juce::Slider::backgroundColourId, juce::Colour(0xFF404040));
    progressSlider.onDragStart = [this] { isUserDraggingSlider = true; };
    progressSlider.onDragEnd = [this] { 
        isUserDraggingSlider = false;
        audioEngine.getMediaPlayer().setPosition((float)progressSlider.getValue());
    };
    progressSlider.onValueChange = [this] {
        if (isUserDraggingSlider) audioEngine.getMediaPlayer().setPosition((float)progressSlider.getValue());
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

MediaPage::~MediaPage() { stopTimer(); }

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
    auto& player = audioEngine.getMediaPlayer();
    bool isPlaying = player.isPlaying();
    playPauseBtn.setButtonText(isPlaying ? "PAUSE" : "PLAY");
    
    if (!isUserDraggingSlider && isPlaying)
    {
        progressSlider.setValue(player.getPosition(), juce::dontSendNotification);
    }

    // Update Clocks
    // VLC Length is in MS
    double lenMs = (double)player.getLengthMs(); 
    double posRatio = player.getPosition();
    double currentMs = lenMs * posRatio;

    totalTimeLabel.setText(formatTime(lenMs / 1000.0), juce::dontSendNotification);
    currentTimeLabel.setText(formatTime(currentMs / 1000.0), juce::dontSendNotification);
}

void MediaPage::paint(juce::Graphics& g) { g.fillAll(juce::Colour(0xFF000000)); }

void MediaPage::resized()
{
    auto area = getLocalBounds();
    int totalWidth = area.getWidth();
    int playerAreaWidth = (int)(totalWidth * 0.65f);
    auto playerArea = area.removeFromLeft(playerAreaWidth);
    playlistComponent->setBounds(area);
    
    int transportHeight = 30;
    auto transportArea = playerArea.removeFromBottom(transportHeight);
    
    // Bottom Row: Buttons
    playPauseBtn.setBounds(transportArea.removeFromLeft(60).reduced(2));
    stopBtn.setBounds(transportArea.removeFromLeft(60).reduced(2));
    
    // Time & Slider Area (Above Buttons)
    // Actually, design request: Clock Left (Total) and Clock Right (Progress)? 
    // Request: "Clock that shows tracks total length in the left... and another... in the right"
    // Usually Total is on the right, but I will follow instructions: Total Left, Progress Right?
    // "total length in the left... progress in the right" -> Okay.
    
    // Let's create a small strip ABOVE the slider for text? Or beside the slider?
    // "above the time slider"
    
    int sliderHeight = 20;
    int labelHeight = 15;
    
    auto sliderStrip = playerArea.removeFromBottom(sliderHeight + labelHeight + 5); 
    // Strip contains Labels (Top) + Slider (Bottom)
    
    auto labelRow = sliderStrip.removeFromTop(labelHeight);
    totalTimeLabel.setBounds(labelRow.removeFromLeft(60));
    currentTimeLabel.setBounds(labelRow.removeFromRight(60));
    
    progressSlider.setBounds(sliderStrip.reduced(2));

    // Video Area
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