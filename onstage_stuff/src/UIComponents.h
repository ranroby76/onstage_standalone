#pragma once

#include <JuceHeader.h>
#include "Style.h"

// Forward declaration
class SubterraneumAudioProcessor;

// ==============================================================================
// ASIO Status LED Component
// ==============================================================================
class AsioStatusLed : public juce::Component, public juce::Timer {
public:
    AsioStatusLed();
    ~AsioStatusLed() override;
    void paint(juce::Graphics& g) override;
    void timerCallback() override;
    bool isAsioActive() const { return asioActive; }
private:
    bool asioActive = false;
};

// ==============================================================================
// Status ToolTip for pins and wires
// ==============================================================================
class StatusToolTip : public juce::Component, public juce::Timer {
public: 
    StatusToolTip(const juce::String& title, bool isStreaming, std::function<void()> onDelete = nullptr);
    ~StatusToolTip() override;
    void paint(juce::Graphics& g) override; 
    void resized() override; 
    void timerCallback() override;
private: 
    juce::Label titleLabel;
    juce::Label statusLabel;
    juce::TextButton deleteButton { "Delete Wire" }; 
    bool streaming; 
    float alpha = 0.0f; 
    float delta = 0.1f; 
    std::function<void()> deleteCallback;
};

// ==============================================================================
// Plugin Window for hosting plugin UIs
// ==============================================================================
class PluginWindow : public juce::DocumentWindow {
public: 
    PluginWindow(const juce::String& name, juce::Colour backgroundColour, int buttonsNeeded) 
        : DocumentWindow(name, backgroundColour, buttonsNeeded) { 
        setUsingNativeTitleBar(true);
    } 
    void closeButtonPressed() override { setVisible(false); } 
};

// ==============================================================================
// Tree LookAndFeel for plugin manager
// ==============================================================================
class TreeLookAndFeel : public juce::LookAndFeel_V4 {
public:
    void drawTreeviewPlusMinusBox(juce::Graphics& g, const juce::Rectangle<float>& area, 
                                   juce::Colour backgroundColour, bool isOpen, bool isMouseOver) override;
};

// ==============================================================================
// Custom dialog window for plugin scanning
// ==============================================================================
class ScanDialogWindow : public juce::DialogWindow {
public:
    ScanDialogWindow(const juce::String& title, std::function<void()> onCloseCallback);
    void closeButtonPressed() override;
private:
    std::function<void()> closeCallback;
};
