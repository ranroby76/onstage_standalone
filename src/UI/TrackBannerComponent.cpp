#include "TrackBannerComponent.h"

TrackBannerComponent::TrackBannerComponent(int index, PlaylistItem& item, 
                                           std::function<void()> onRemove,
                                           std::function<void()> onExpandToggle,
                                           std::function<void()> onBannerClick,
                                           std::function<void()> onPlayButton,
                                           std::function<void(float)> onVolChange,
                                           std::function<void(float)> onSpeedChange)
    : trackIndex(index), itemData(item), 
      onRemoveCallback(onRemove), 
      onExpandToggleCallback(onExpandToggle), 
      onBannerClickCallback(onBannerClick),
      onPlayButtonCallback(onPlayButton),
      onVolChangeCallback(onVolChange), 
      onSpeedChangeCallback(onSpeedChange)
{
    // Play button - completely transparent, only for click detection
    addAndMakeVisible(playButton);
    playButton.setButtonText("");
    playButton.setMidiInfo("Select Track");
    playButton.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    playButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
    playButton.setColour(juce::TextButton::textColourOffId, juce::Colours::transparentBlack);
    playButton.setColour(juce::TextButton::textColourOnId, juce::Colours::transparentBlack);
    playButton.setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    playButton.onClick = onPlayButtonCallback;
    
    addAndMakeVisible(removeButton);
    removeButton.setButtonText("X");
    removeButton.setMidiInfo("Remove Track from Playlist");
    removeButton.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    removeButton.setColour(juce::TextButton::textColourOffId, juce::Colours::red);
    removeButton.onClick = onRemoveCallback;

    addAndMakeVisible(expandButton);
    expandButton.setButtonText(itemData.isExpanded ? "-" : "+");
    expandButton.setMidiInfo("Show/Hide Controls (Volume, Speed, Wait)");
    expandButton.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    expandButton.onClick = onExpandToggleCallback;

    if (itemData.isExpanded)
    {
        // 1. Volume (0.0 to 2.0 = volume multiplier)
        volSlider = std::make_unique<StyledSlider>(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
        volSlider->setMidiInfo("Track Volume (0.0x to 2.0x) - Double-click to reset to 1.0x"); 
        volSlider->setRange(0.0, 2.0, 0.01);
        volSlider->setValue(itemData.volume, juce::dontSendNotification);
        volSlider->setTextValueSuffix("x");
        volSlider->onValueChange = [this] { 
            itemData.volume = (float)volSlider->getValue();
            if (onVolChangeCallback) onVolChangeCallback(itemData.volume);
        };
        // FIXED: Double-click resets to default (1.0x)
        volSlider->onDoubleClick = [this] {
            volSlider->setValue(1.0, juce::sendNotification);
        };
        addAndMakeVisible(volSlider.get());

        // 2. Speed (0.5 to 1.5 = symmetric around 1.0)
        speedSlider = std::make_unique<StyledSlider>(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
        speedSlider->setMidiInfo("Playback Speed (0.5x - 1.5x) - Double-click to reset to 1.0x");
        speedSlider->setRange(0.5, 1.5, 0.01);  // FIXED: Symmetric range for middle alignment
        speedSlider->setValue(itemData.playbackSpeed, juce::dontSendNotification);
        speedSlider->onValueChange = [this] { 
            itemData.playbackSpeed = (float)speedSlider->getValue();
            if (onSpeedChangeCallback) onSpeedChangeCallback(itemData.playbackSpeed);
        };
        // FIXED: Double-click resets to default (1.0x)
        speedSlider->onDoubleClick = [this] {
            speedSlider->setValue(1.0, juce::sendNotification);
        };
        addAndMakeVisible(speedSlider.get());

        // 3. Wait Delay
        delaySlider = std::make_unique<StyledSlider>(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
        delaySlider->setMidiInfo("Wait Time Between Tracks (0-30 seconds)");
        delaySlider->setRange(0.0, 30.0, 1.0);
        delaySlider->setValue(itemData.transitionDelaySec, juce::dontSendNotification);
        delaySlider->setTextValueSuffix(" s");
        delaySlider->onValueChange = [this] {
            itemData.transitionDelaySec = (int)delaySlider->getValue();
        };
        addAndMakeVisible(delaySlider.get());

        addAndMakeVisible(volLabel); volLabel.setText("Vol", juce::dontSendNotification);
        addAndMakeVisible(speedLabel); speedLabel.setText("Speed", juce::dontSendNotification);
        addAndMakeVisible(delayLabel); delayLabel.setText("Wait", juce::dontSendNotification);
    }
}

void TrackBannerComponent::onLongPress()
{
    showMidiTooltip(this, "Track: " + itemData.title);
}

void TrackBannerComponent::mouseDown(const juce::MouseEvent& e)
{
    if (e.mods.isRightButtonDown())
    {
        onLongPress();
        return; 
    }
    handleMouseDown(e); 
}

void TrackBannerComponent::mouseUp(const juce::MouseEvent& e)
{
    handleMouseUp(e);
    if (e.mods.isRightButtonDown() || isLongPressTriggered) return;
    
    // FIX: Banner click calls onBannerClickCallback (which does nothing now)
    if (onBannerClickCallback) onBannerClickCallback();
}

void TrackBannerComponent::mouseDrag(const juce::MouseEvent& e)
{
    handleMouseDrag(e);
}

void TrackBannerComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    
    // Background
    if (isCurrentTrack) 
        g.setColour(juce::Colour(0xFF152215)); 
    else 
        g.setColour(juce::Colour(0xFF1A1A1A)); 
    g.fillRoundedRectangle(bounds, 5.0f);
    
    // Border
    if (isCurrentTrack) {
        g.setColour(juce::Colour(0xFF00FF00));
        g.drawRoundedRectangle(bounds, 5.0f, 2.0f);
    } else {
        g.setColour(juce::Colour(0xFF404040)); 
        g.drawRoundedRectangle(bounds, 5.0f, 1.0f);
    }

    // FIX: Draw circular button with play triangle
    float circleX = 10.0f;
    float circleY = 10.0f;
    float circleDiameter = 24.0f;
    
    // Circle background
    g.setColour(isCurrentTrack ? juce::Colour(0xFF00FF00) : juce::Colour(0xFFD4AF37));
    g.fillEllipse(circleX, circleY, circleDiameter, circleDiameter);
    
    // FIX: Draw play triangle inside circle
    g.setColour(juce::Colours::black);
    juce::Path triangle;
    float centerX = circleX + circleDiameter / 2.0f;
    float centerY = circleY + circleDiameter / 2.0f;
    float triangleSize = 8.0f;
    
    // Play triangle pointing right
    triangle.addTriangle(
        centerX - triangleSize / 2.0f, centerY - triangleSize / 2.0f,  // Top left
        centerX - triangleSize / 2.0f, centerY + triangleSize / 2.0f,  // Bottom left
        centerX + triangleSize / 2.0f, centerY                          // Right point
    );
    g.fillPath(triangle);
    
    // Title text
    g.setColour(juce::Colour(0xFFD4AF37)); 
    g.setFont(juce::Font(15.0f, juce::Font::bold));
    auto textArea = getLocalBounds().reduced(5).withTrimmedLeft(40).withTrimmedRight(110).withHeight(34);
    g.drawFittedText(itemData.title, textArea, juce::Justification::centredLeft, 1);
}

void TrackBannerComponent::resized()
{
    auto bounds = getLocalBounds();
    
    // Play button covers the circular area
    playButton.setBounds(10, 10, 24, 24);
    
    expandButton.setBounds(bounds.getWidth() - 30, 10, 20, 20);
    removeButton.setBounds(bounds.getWidth() - 60, 10, 20, 20);
    
    if (itemData.isExpanded)
    {
        int startY = 44;
        int rowH = 30;
        int labelW = 40;
        
        // 1. Volume
        volLabel.setBounds(10, startY, labelW, rowH);
        volSlider->setBounds(10 + labelW, startY, bounds.getWidth() - 20 - labelW, rowH);
        
        // 2. Speed
        speedLabel.setBounds(10, startY + rowH, labelW, rowH);
        speedSlider->setBounds(10 + labelW, startY + rowH, bounds.getWidth() - 20 - labelW, rowH);
        
        // 3. Wait
        delayLabel.setBounds(10, startY + rowH * 2, labelW, rowH);
        delaySlider->setBounds(10 + labelW, startY + rowH * 2, bounds.getWidth() - 20 - labelW, rowH);
    }
}

void TrackBannerComponent::setPlaybackState(bool isCurrent, bool isAudioActive)
{
    isCurrentTrack = isCurrent;
    isAudioPlaying = isAudioActive;
    repaint();
}