#include "MidiSelectors.h"

// ==============================================================================
// MidiInputChannelSelector Implementation
// ==============================================================================
MidiInputChannelSelector::MidiInputChannelSelector(const juce::String& deviceName, int currentMask, std::function<void(int)> onMaskChanged) 
    : midiDeviceName(deviceName), maskChangedCallback(onMaskChanged), channelMask(currentMask)
{
    titleLabel.setText("MIDI Routing: " + deviceName, juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centred); 
    titleLabel.setFont(juce::Font(14.0f, juce::Font::bold)); 
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);
    
    closeButton.setColour(juce::TextButton::buttonColourId, juce::Colours::red.darker()); 
    closeButton.addListener(this);
    addAndMakeVisible(closeButton);
    
    allButton.setColour(juce::TextButton::buttonColourId, juce::Colours::green.darker());
    allButton.addListener(this);
    addAndMakeVisible(allButton);
    
    noneButton.setColour(juce::TextButton::buttonColourId, juce::Colours::grey.darker());
    noneButton.addListener(this);
    addAndMakeVisible(noneButton);
    
    for (int i = 1; i <= 16; ++i) { 
        auto* btn = channelButtons.add(new juce::TextButton(juce::String(i)));
        btn->setClickingTogglesState(true); 
        btn->setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff8B0000)); // Dark red when ON
        btn->setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        btn->setToggleState((channelMask >> (i - 1)) & 1, juce::dontSendNotification); 
        btn->addListener(this); 
        addAndMakeVisible(btn); 
    }
    setSize(280, 200);
}

MidiInputChannelSelector::~MidiInputChannelSelector() {}

void MidiInputChannelSelector::paint(juce::Graphics& g) { 
    g.fillAll(juce::Colours::black.withAlpha(0.95f)); 
    g.setColour(juce::Colour(0xff8B0000)); // Dark red border
    g.drawRect(getLocalBounds(), 2);
}

void MidiInputChannelSelector::resized() { 
    auto area = getLocalBounds().reduced(8); 
    
    auto header = area.removeFromTop(25);
    closeButton.setBounds(header.removeFromRight(25)); 
    titleLabel.setBounds(header); 
    
    area.removeFromTop(8);
    
    auto controlRow = area.removeFromTop(25);
    allButton.setBounds(controlRow.removeFromLeft(60));
    controlRow.removeFromLeft(10);
    noneButton.setBounds(controlRow.removeFromLeft(60));
    
    area.removeFromTop(10);
    
    int cols = 4; 
    int rows = 4; 
    int w = area.getWidth() / cols;
    int h = area.getHeight() / rows;
    for (int i = 0; i < 16; ++i) { 
        if (channelButtons[i]) { 
            int r = i / cols;
            int c = i % cols; 
            channelButtons[i]->setBounds(area.getX() + c * w + 2, area.getY() + r * h + 2, w - 4, h - 4);
        } 
    } 
}

void MidiInputChannelSelector::buttonClicked(juce::Button* b) { 
    if (b == &closeButton) { 
        if (auto* callout = findParentComponentOfClass<juce::CallOutBox>()) 
            callout->setVisible(false);
        return;
    }
    
    if (b == &allButton) {
        channelMask = 0xFFFF;
        for (int i = 0; i < 16; ++i)
            channelButtons[i]->setToggleState(true, juce::dontSendNotification);
        if (maskChangedCallback) maskChangedCallback(channelMask);
        return;
    }
    
    if (b == &noneButton) {
        channelMask = 0;
        for (int i = 0; i < 16; ++i)
            channelButtons[i]->setToggleState(false, juce::dontSendNotification);
        if (maskChangedCallback) maskChangedCallback(channelMask);
        return;
    }
    
    channelMask = 0;
    for (int i = 0; i < 16; ++i) { 
        if (channelButtons[i]->getToggleState()) 
            channelMask |= (1 << i);
    } 
    if (maskChangedCallback) maskChangedCallback(channelMask);
}

// ==============================================================================
// MidiChannelSelector Implementation (FOR VSTi NODES - SINGLE CHANNEL SELECT)
// ==============================================================================
MidiChannelSelector::MidiChannelSelector(MeteringProcessor* processor, std::function<void()> onUpdate) 
    : meteringProc(processor), updateCallback(onUpdate) 
{
    titleLabel.setText("Select MIDI Input Channel", juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centred); 
    titleLabel.setFont(juce::Font(14.0f, juce::Font::bold)); 
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);
    
    closeButton.setColour(juce::TextButton::buttonColourId, juce::Colours::red.darker()); 
    closeButton.addListener(this);
    addAndMakeVisible(closeButton);
    
    // Get current single channel (convert mask to channel number)
    int mask = meteringProc ? meteringProc->getMidiChannelMask() : 0;
    int currentChannel = 1; // Default to channel 1
    if (mask != 0) {
        // Find first enabled channel in mask
        for (int i = 0; i < 16; ++i) {
            if ((mask >> i) & 1) {
                currentChannel = i + 1;
                break;
            }
        }
    }
    
    for (int i = 1; i <= 16; ++i) { 
        auto* btn = channelButtons.add(new juce::TextButton(juce::String(i)));
        btn->setClickingTogglesState(false); // Radio button behavior - no toggle
        btn->setColour(juce::TextButton::buttonOnColourId, juce::Colours::orange);
        btn->setColour(juce::TextButton::textColourOnId, juce::Colours::black);
        btn->setColour(juce::TextButton::buttonColourId, juce::Colours::grey.darker());
        
        // Highlight current channel
        if (i == currentChannel) {
            btn->setColour(juce::TextButton::buttonColourId, juce::Colours::orange);
            btn->setColour(juce::TextButton::textColourOffId, juce::Colours::black);
        }
        
        btn->addListener(this); 
        addAndMakeVisible(btn); 
    }
    setSize(280, 180);
}

MidiChannelSelector::~MidiChannelSelector() {}

void MidiChannelSelector::paint(juce::Graphics& g) { 
    g.fillAll(juce::Colours::black.withAlpha(0.95f)); 
    g.setColour(juce::Colours::orange); 
    g.drawRect(getLocalBounds(), 2);
}

void MidiChannelSelector::resized() { 
    auto area = getLocalBounds().reduced(8); 
    
    auto header = area.removeFromTop(25);
    closeButton.setBounds(header.removeFromRight(25)); 
    titleLabel.setBounds(header); 
    
    area.removeFromTop(8);
    
    int cols = 4; 
    int rows = 4; 
    int w = area.getWidth() / cols;
    int h = area.getHeight() / rows;
    for (int i = 0; i < 16; ++i) { 
        if (channelButtons[i]) { 
            int r = i / cols;
            int c = i % cols; 
            channelButtons[i]->setBounds(area.getX() + c * w + 2, area.getY() + r * h + 2, w - 4, h - 4);
        } 
    } 
}

void MidiChannelSelector::buttonClicked(juce::Button* b) { 
    if (b == &closeButton) { 
        if (auto* callout = findParentComponentOfClass<juce::CallOutBox>()) 
            callout->setVisible(false);
        return;
    } 
    
    if (meteringProc) { 
        // Find which channel button was clicked
        for (int i = 0; i < 16; ++i) {
            if (channelButtons[i] == b) {
                // Set single channel (convert channel number to mask with only that bit set)
                int selectedChannel = i + 1;
                int mask = (1 << i); // Single bit set for this channel
                meteringProc->setMidiChannelMask(mask);
                
                // Update button colors - only selected channel is highlighted
                for (int j = 0; j < 16; ++j) {
                    if (j == i) {
                        // Selected channel - orange
                        channelButtons[j]->setColour(juce::TextButton::buttonColourId, juce::Colours::orange);
                        channelButtons[j]->setColour(juce::TextButton::textColourOffId, juce::Colours::black);
                    } else {
                        // Unselected channels - dark grey
                        channelButtons[j]->setColour(juce::TextButton::buttonColourId, juce::Colours::grey.darker());
                        channelButtons[j]->setColour(juce::TextButton::textColourOffId, juce::Colours::lightgrey);
                    }
                }
                
                if (updateCallback) updateCallback();
                break;
            }
        }
    } 
}
