#include "TrackBannerComponent.h"

TrackBannerComponent::TrackBannerComponent(int index, PlaylistItem& item, 
                                           std::function<void()> onRemove,
                                           std::function<void()> onExpandToggle,
                                           std::function<void()> onSelect,
                                           std::function<void(float)> onVolChange,
                                           std::function<void(float)> onSpeedChange,
                                           std::function<void(float)> onPitchChange)
    : trackIndex(index), itemData(item), 
      onRemoveCallback(onRemove), 
      onExpandToggleCallback(onExpandToggle), onSelectCallback(onSelect),
      onVolChangeCallback(onVolChange), onSpeedChangeCallback(onSpeedChange),
      onPitchChangeCallback(onPitchChange)
{
    addAndMakeVisible(indexLabel);
    indexLabel.setText(juce::String(index + 1), juce::dontSendNotification);
    indexLabel.setJustificationType(juce::Justification::centred);
    indexLabel.setColour(juce::Label::textColourId, juce::Colours::black);
    indexLabel.setInterceptsMouseClicks(false, false); 
    
    addAndMakeVisible(removeButton);
    removeButton.setButtonText("X");
    removeButton.setMidiInfo("Remove Track from Playlist");
    removeButton.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    removeButton.setColour(juce::TextButton::textColourOffId, juce::Colours::red);
    removeButton.onClick = onRemoveCallback;

    addAndMakeVisible(crossfadeButton);
    crossfadeButton.setButtonText("F");
    crossfadeButton.setMidiInfo("Crossfade Mode: When ON, 'Wait' becomes the fade overlap duration.");
    crossfadeButton.setClickingTogglesState(true);
    crossfadeButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF2A2A2A));
    crossfadeButton.setColour(juce::TextButton::textColourOffId, juce::Colours::grey);
    crossfadeButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xFFD4AF37));
    crossfadeButton.setColour(juce::TextButton::textColourOnId, juce::Colours::black);
    crossfadeButton.setToggleState(itemData.isCrossfade, juce::dontSendNotification);
    
    crossfadeButton.onClick = [this] { 
        itemData.isCrossfade = crossfadeButton.getToggleState();
        if (delaySlider) {
            delaySlider->setTextValueSuffix(itemData.isCrossfade ? " s (Fade)" : " s (Wait)");
            delaySlider->setValue(delaySlider->getValue(), juce::dontSendNotification);
        }
    };

    addAndMakeVisible(expandButton);
    expandButton.setButtonText(itemData.isExpanded ? "-" : "+");
    expandButton.setMidiInfo("Show/Hide Controls (Volume, Speed, Wait)");
    expandButton.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    expandButton.onClick = onExpandToggleCallback;

    if (itemData.isExpanded)
    {
        // 1. Volume
        volSlider = std::make_unique<StyledSlider>(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
        volSlider->setMidiInfo("Track Gain (0dB to +22dB)"); 
        volSlider->setRange(0.0, 2.0, 0.01);
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

        // 2. Pitch (NEW)
        pitchSlider = std::make_unique<StyledSlider>(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
        pitchSlider->setMidiInfo("Pitch Shift (-12 to +12 semitones)");
        pitchSlider->setRange(-12.0, 12.0, 1.0); // 25 steps: -12..0..+12
        pitchSlider->setValue(itemData.pitch, juce::dontSendNotification);
        pitchSlider->setTextValueSuffix(" st");
        pitchSlider->onValueChange = [this] {
            itemData.pitch = (float)pitchSlider->getValue();
            if (onPitchChangeCallback) onPitchChangeCallback(itemData.pitch);
        };
        addAndMakeVisible(pitchSlider.get());

        // 3. Speed
        speedSlider = std::make_unique<StyledSlider>(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
        speedSlider->setMidiInfo("Playback Speed (0.1x - 2.1x)");
        speedSlider->setRange(0.1, 2.1, 0.01);
        speedSlider->setValue(itemData.playbackSpeed, juce::dontSendNotification);
        speedSlider->onValueChange = [this] { 
            itemData.playbackSpeed = (float)speedSlider->getValue();
            if (onSpeedChangeCallback) onSpeedChangeCallback(itemData.playbackSpeed);
        };
        addAndMakeVisible(speedSlider.get());

        // 4. Wait/Crossfade
        delaySlider = std::make_unique<StyledSlider>(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
        delaySlider->setMidiInfo("Transition Wait Time (or Crossfade Length if F is on)");
        delaySlider->setRange(0.0, 30.0, 1.0);
        delaySlider->setValue(itemData.transitionDelaySec, juce::dontSendNotification);
        delaySlider->setTextValueSuffix(itemData.isCrossfade ? " s (Fade)" : " s (Wait)");
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
        addAndMakeVisible(pitchLabel); pitchLabel.setText("Pitch", juce::dontSendNotification);
        addAndMakeVisible(speedLabel); speedLabel.setText("Speed", juce::dontSendNotification);
        addAndMakeVisible(delayLabel); delayLabel.setText("Wait", juce::dontSendNotification);
    }
}

void TrackBannerComponent::onLongPress()
{
    showMidiTooltip(this, "Track: " + itemData.title + "\nLeft-Click to Select/Load");
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
    if (onSelectCallback) onSelectCallback();
}

void TrackBannerComponent::mouseDrag(const juce::MouseEvent& e)
{
    handleMouseDrag(e);
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
    crossfadeButton.setBounds(bounds.getWidth() - 90, 10, 25, 20);
    
    if (itemData.isExpanded)
    {
        int startY = 44;
        int rowH = 30;
        int labelW = 40;
        
        // 1. Volume
        volLabel.setBounds(10, startY, labelW, rowH);
        volSlider->setBounds(10 + labelW, startY, bounds.getWidth() - 20 - labelW, rowH);
        
        // 2. Pitch
        pitchLabel.setBounds(10, startY + rowH, labelW, rowH);
        pitchSlider->setBounds(10 + labelW, startY + rowH, bounds.getWidth() - 20 - labelW, rowH);

        // 3. Speed
        speedLabel.setBounds(10, startY + rowH * 2, labelW, rowH);
        speedSlider->setBounds(10 + labelW, startY + rowH * 2, bounds.getWidth() - 20 - labelW, rowH);
        
        // 4. Wait
        delayLabel.setBounds(10, startY + rowH * 3, labelW, rowH);
        delaySlider->setBounds(10 + labelW, startY + rowH * 3, bounds.getWidth() - 20 - labelW, rowH);
    }
}

void TrackBannerComponent::setPlaybackState(bool isCurrent, bool isAudioActive)
{
    isCurrentTrack = isCurrent;
    isAudioPlaying = isAudioActive;
    repaint();
}