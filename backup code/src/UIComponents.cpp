
#include "UIComponents.h"
#include "PluginProcessor.h"

// ==============================================================================
// AsioStatusLed Implementation
// ==============================================================================
AsioStatusLed::AsioStatusLed() {
    startTimer(200);
}

AsioStatusLed::~AsioStatusLed() {
    stopTimer();
}

void AsioStatusLed::timerCallback() {
    bool newState = false;
    if (SubterraneumAudioProcessor::standaloneDeviceManager) {
        newState = (SubterraneumAudioProcessor::standaloneDeviceManager->getCurrentAudioDevice() != nullptr);
    }
    if (newState != asioActive) {
        asioActive = newState;
        repaint();
    }
}

void AsioStatusLed::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat().reduced(2);
    float size = juce::jmin(bounds.getWidth(), bounds.getHeight());
    auto ledRect = juce::Rectangle<float>(bounds.getCentreX() - size/2, bounds.getCentreY() - size/2, size, size);
    
    juce::Colour ledColor = asioActive ? juce::Colours::limegreen : juce::Colours::red;
    
    g.setColour(ledColor.withAlpha(0.3f));
    g.fillEllipse(ledRect.expanded(2));
    
    g.setColour(ledColor);
    g.fillEllipse(ledRect);
    
    g.setColour(juce::Colours::white.withAlpha(0.4f));
    g.fillEllipse(ledRect.reduced(size * 0.3f).translated(-size * 0.1f, -size * 0.1f));
}

// ==============================================================================
// StatusToolTip Implementation
// ==============================================================================
StatusToolTip::StatusToolTip(const juce::String& title, bool isStreaming, std::function<void()> onDelete) 
    : streaming(isStreaming), deleteCallback(onDelete) 
{ 
    addAndMakeVisible(titleLabel);
    titleLabel.setText(title, juce::dontSendNotification); 
    titleLabel.setJustificationType(juce::Justification::centred); 
    titleLabel.setFont(14.0f); 
    
    addAndMakeVisible(statusLabel); 
    statusLabel.setFont(12.0f); 
    statusLabel.setJustificationType(juce::Justification::centred); 
    
    if (streaming) { 
        statusLabel.setText("Active Streaming", juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgreen); 
        startTimer(50);
    } else { 
        statusLabel.setText("Idle (Bypassed)", juce::dontSendNotification); 
        statusLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    } 
    
    if (deleteCallback) { 
        addAndMakeVisible(deleteButton);
        deleteButton.onClick = [this] { 
            if (deleteCallback) deleteCallback();
            if (auto* callout = findParentComponentOfClass<juce::CallOutBox>()) 
                callout->setVisible(false);
        }; 
    } 
    setSize(200, deleteCallback ? 100 : 70); 
}

StatusToolTip::~StatusToolTip() { stopTimer(); }

void StatusToolTip::timerCallback() { 
    alpha += delta;
    if (alpha >= 1.0f || alpha <= 0.2f) delta = -delta; 
    repaint();
}

void StatusToolTip::paint(juce::Graphics& g) { 
    g.fillAll(juce::Colours::black.withAlpha(0.8f)); 
    g.setColour(juce::Colours::white); 
    g.drawRect(getLocalBounds(), 1);
    if (streaming) { 
        g.setColour(juce::Colours::green.withAlpha(alpha)); 
        float r = 8.0f;
        g.fillEllipse(getWidth() / 2.0f - r, 45.0f, r * 2.0f, r * 2.0f);
    } else { 
        g.setColour(juce::Colours::red); 
        float r = 4.0f;
        g.drawEllipse(getWidth() / 2.0f - r, 48.0f, r * 2.0f, r * 2.0f, 1.0f);
    } 
}

void StatusToolTip::resized() { 
    titleLabel.setBounds(0, 5, getWidth(), 20); 
    statusLabel.setBounds(0, 25, getWidth(), 20);
    if (deleteCallback) { 
        deleteButton.setBounds(10, 65, getWidth() - 20, 25);
    } 
}

// ==============================================================================
// TreeLookAndFeel Implementation
// ==============================================================================
void TreeLookAndFeel::drawTreeviewPlusMinusBox(juce::Graphics& g, const juce::Rectangle<float>& area, 
                                               juce::Colour /*backgroundColour*/, bool isOpen, bool /*isMouseOver*/) {
    using namespace juce;
    Path p;
    p.addTriangle(0.0f, 0.0f, 0.8f, 0.5f, 0.0f, 1.0f); 

    float size = jmin(area.getWidth(), area.getHeight()) * 0.5f;
    auto center = area.getCentre();
    
    auto transform = AffineTransform::scale(size, size).translated(center.x - size*0.5f, center.y - size*0.5f);

    if (isOpen) {
        transform = transform.rotated(MathConstants<float>::halfPi, center.x, center.y);
    }
    
    g.setColour(Colours::orange);
    g.fillPath(p, transform);
}

// ==============================================================================
// ScanDialogWindow Implementation
// ==============================================================================
ScanDialogWindow::ScanDialogWindow(const juce::String& title, std::function<void()> onCloseCallback)
    : DialogWindow(title, juce::Colours::darkgrey, true, true),
      closeCallback(onCloseCallback)
{
    setUsingNativeTitleBar(true);
}

void ScanDialogWindow::closeButtonPressed() {
    setVisible(false);
    if (closeCallback)
        closeCallback();
}


