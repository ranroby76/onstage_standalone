// Implemented the help strings and fixed the mouse interaction logic.

#include "TrackBannerComponent.h"

TrackBannerComponent::TrackBannerComponent(int index, PlaylistItem& item, 
                                           std::function<void()> onRemove,
                                           std::function<void()> onExpandToggle,
                                           std::function<void()> onSelect,
                                           std::function<void(float)> onVolChange,
                                           std::function<void(float)> onSpeedChange)
    : trackIndex(index), itemData(item), 
      onRemoveCallback(onRemove), 
      onExpandToggleCallback(onExpandToggle), onSelectCallback(onSelect),
      onVolChangeCallback(onVolChange), onSpeedChangeCallback(onSpeedChange)
{
    addAndMakeVisible(indexLabel);
    indexLabel.setText(juce::String(index + 1), juce::dontSendNotification);
    indexLabel.setJustificationType(juce::Justification::centred);
    indexLabel.setColour(juce::Label::textColourId, juce::Colours::black);
    indexLabel.setInterceptsMouseClicks(false, false); 
    
    addAndMakeVisible(removeButton);
    removeButton.setButtonText("X");
    removeButton.setMidiInfo("Remove Track from Playlist"); // Right-click help
    removeButton.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    removeButton.setColour(juce::TextButton::textColourOffId, juce::Colours::red);
    removeButton.onClick = onRemoveCallback;

    // Crossfade Button configuration
    addAndMakeVisible(crossfadeButton);
    crossfadeButton.setButtonText("F");
    // Set Right-Click Help
    crossfadeButton.setMidiInfo("Crossfade Mode: When ON, 'Wait' becomes the fade overlap duration.");
    
    // Make it behave like a toggle button
    crossfadeButton.setClickingTogglesState(true);
    
    // Style: OFF (Dark Grey)
    crossfadeButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF2A2A2A));
    crossfadeButton.setColour(juce::TextButton::textColourOffId, juce::Colours::grey);
    
    // Style: ON (Gold Background, Black Text)
    crossfadeButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xFFD4AF37));
    crossfadeButton.setColour(juce::TextButton::textColourOnId, juce::Colours::black);
    
    // Set Initial State
    crossfadeButton.setToggleState(itemData.isCrossfade, juce::dontSendNotification);
    
    crossfadeButton.onClick = [this] { 
        itemData.isCrossfade = crossfadeButton.getToggleState();
        // Update slider text to reflect negative/positive immediately
        if (delaySlider) {
            delaySlider->setTextValueSuffix(itemData.isCrossfade ? " s (Fade)" : " s (Wait)");
            // Force refresh of text display
            delaySlider->setValue(delaySlider->getValue(), juce::dontSendNotification);
        }
    };

    addAndMakeVisible(expandButton);
    expandButton.setButtonText(itemData.isExpanded ? "-" : "+");
    expandButton.setMidiInfo("Show/Hide Controls (Volume, Speed, Wait)"); // Right-click help
    expandButton.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    expandButton.onClick = onExpandToggleCallback;

    if (itemData.isExpanded)
    {
        volSlider = std::make_unique<StyledSlider>(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
        volSlider->setMidiInfo("Track Gain (0dB to +22dB)"); // Help Text
        volSlider->setRange(0.0, 2.0, 0.01);
        
        // INIT: Convert stored Linear Gain to Slider Position
        float initSliderVal = 0.0f;
        if (itemData.volume > 0.0001f) {
            float db = juce::Decibels::gainToDecibels(itemData.volume);
            float norm = (db / 44.0f) + 0.5f;
            initSliderVal = juce::jlimit(0.0f, 2.0f, norm * 2.0f);
        }
        volSlider->setValue(initSliderVal, juce::dontSendNotification);
        
        volSlider->onValueChange = [this] { 
            float sliderVal = (float)volSlider->getValue();
            float linear = 0.0f;
            
            if (sliderVal > 0.0f) {
                float norm = sliderVal / 2.0f;
                float db = (norm - 0.5f) * 44.0f;
                linear = juce::Decibels::decibelsToGain(db);
            }
            
            itemData.volume = linear;
            if (onVolChangeCallback) onVolChangeCallback(itemData.volume);
        };
        addAndMakeVisible(volSlider.get());

        speedSlider = std::make_unique<StyledSlider>(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
        speedSlider->setMidiInfo("Playback Speed (0.1x - 2.1x)"); // Help Text
        speedSlider->setRange(0.1, 2.1, 0.01);
        speedSlider->setValue(itemData.playbackSpeed, juce::dontSendNotification);
        speedSlider->onValueChange = [this] { 
            itemData.playbackSpeed = (float)speedSlider->getValue();
            if (onSpeedChangeCallback) onSpeedChangeCallback(itemData.playbackSpeed);
        };
        addAndMakeVisible(speedSlider.get());

        delaySlider = std::make_unique<StyledSlider>(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
        delaySlider->setMidiInfo("Transition Wait Time (or Crossfade Length if F is on)"); // Help Text
        delaySlider->setRange(0.0, 30.0, 1.0);
        delaySlider->setValue(itemData.transitionDelaySec, juce::dontSendNotification);
        
        // Initialize suffix based on state
        delaySlider->setTextValueSuffix(itemData.isCrossfade ? " s (Fade)" : " s (Wait)");
        
        // Override getTextFromValue to show negative numbers if Crossfade is on
        delaySlider->textFromValueFunction = [this](double value) {
            if (itemData.isCrossfade && value > 0) 
                return "-" + juce::String(value, 0) + " s";
            return juce::String(value, 0) + " s";
        };

        delaySlider->onValueChange = [this] { 
            itemData.transitionDelaySec = (int)delaySlider->getValue(); 
        };
        addAndMakeVisible(delaySlider.get());

        addAndMakeVisible(volLabel); volLabel.setText("Vol", juce::dontSendNotification);
        addAndMakeVisible(speedLabel); speedLabel.setText("Speed", juce::dontSendNotification);
        addAndMakeVisible(delayLabel); delayLabel.setText("Wait", juce::dontSendNotification);
    }
}

void TrackBannerComponent::mouseDown(const juce::MouseEvent& e)
{
    // FIX: Block right-click selection logic. 
    // Instead, show a general banner tooltip.
    if (e.mods.isRightButtonDown())
    {
        showMidiTooltip(this, "Track: " + itemData.title + "\nLeft-Click to Select/Load");
        return; 
    }
    
    if (onSelectCallback) onSelectCallback();
}

void TrackBannerComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    if (isCurrentTrack) g.setColour(juce::Colour(0xFF152215)); 
    else g.setColour(juce::Colour(0xFF1A1A1A)); 
    g.fillRoundedRectangle(bounds, 5.0f);

    if (isCurrentTrack) {
        g.setColour(juce::Colour(0xFF00FF00));
        g.drawRoundedRectangle(bounds, 5.0f, 2.0f);
    } else {
        g.setColour(juce::Colour(0xFF404040)); 
        g.drawRoundedRectangle(bounds, 5.0f, 1.0f);
    }

    g.setColour(isCurrentTrack ? juce::Colour(0xFF00FF00) : juce::Colour(0xFFD4AF37));
    g.fillEllipse(10, 10, 24, 24);
    
    g.setColour(juce::Colour(0xFFD4AF37)); 
    g.setFont(juce::Font(15.0f, juce::Font::bold));
    auto textArea = getLocalBounds().reduced(5).withTrimmedLeft(40).withTrimmedRight(110).withHeight(34);
    g.drawFittedText(itemData.title, textArea, juce::Justification::centredLeft, 1);
}

void TrackBannerComponent::resized()
{
    auto bounds = getLocalBounds();
    
    indexLabel.setBounds(10, 10, 24, 24);
    expandButton.setBounds(bounds.getWidth() - 30, 10, 20, 20);
    removeButton.setBounds(bounds.getWidth() - 60, 10, 20, 20);
    
    // Position "F" button to the left of Remove button
    crossfadeButton.setBounds(bounds.getWidth() - 90, 10, 25, 20);
    
    if (itemData.isExpanded)
    {
        int startY = 44;
        int rowH = 30;
        int labelW = 40;
        
        volLabel.setBounds(10, startY, labelW, rowH);
        volSlider->setBounds(10 + labelW, startY, bounds.getWidth() - 20 - labelW, rowH);
        
        speedLabel.setBounds(10, startY + rowH, labelW, rowH);
        speedSlider->setBounds(10 + labelW, startY + rowH, bounds.getWidth() - 20 - labelW, rowH);
        
        delayLabel.setBounds(10, startY + rowH*2, labelW, rowH);
        delaySlider->setBounds(10 + labelW, startY + rowH*2, bounds.getWidth() - 20 - labelW, rowH);
    }
}

void TrackBannerComponent::setPlaybackState(bool isCurrent, bool isAudioActive)
{
    isCurrentTrack = isCurrent;
    isAudioPlaying = isAudioActive;
    repaint();
}