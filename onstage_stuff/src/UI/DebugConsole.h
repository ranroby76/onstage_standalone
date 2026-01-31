#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class DebugConsole : public juce::Component,
                     public juce::Logger
{
public:
    DebugConsole()
    {
        addAndMakeVisible(clearButton);
        clearButton.setButtonText("Clear");
        clearButton.onClick = [this]() { clearLog(); };
        
        addAndMakeVisible(logText);
        logText.setMultiLine(true);
        logText.setReadOnly(true);
        logText.setCaretVisible(false);
        logText.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFF1A1A1A));
        logText.setColour(juce::TextEditor::textColourId, juce::Colours::white);
        logText.setColour(juce::TextEditor::highlightColourId, juce::Colour(0xFFD4AF37));
        logText.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain));
        
        // Set this as the global logger
        juce::Logger::setCurrentLogger(this);
        
        juce::Logger::writeToLog("=== Debug Console Initialized ===");
    }
    
    ~DebugConsole() override
    {
        // Remove as logger before destruction
        if (juce::Logger::getCurrentLogger() == this)
            juce::Logger::setCurrentLogger(nullptr);
    }
    
    void resized() override
    {
        auto area = getLocalBounds();
        auto buttonArea = area.removeFromTop(30);
        clearButton.setBounds(buttonArea.removeFromRight(80).reduced(5));
        logText.setBounds(area);
    }
    
    void logMessage(const juce::String& message) override
    {
        // THREAD SAFE: Use weak reference to avoid accessing deleted component
        juce::Component::SafePointer<DebugConsole> safeThis(this);
        
        juce::MessageManager::callAsync([safeThis, msg = juce::String(message)]() {
            // Check if component still exists
            if (safeThis == nullptr)
                return;
                
            juce::String timestamp = juce::Time::getCurrentTime().formatted("%H:%M:%S.%ms");
            juce::String formattedMessage = "[" + timestamp + "] " + msg + "\n";
            
            // Append text
            safeThis->logText.moveCaretToEnd();
            safeThis->logText.insertTextAtCaret(formattedMessage);
            
            // Auto-scroll to bottom
            safeThis->logText.moveCaretToEnd();
            
            // Limit log size to prevent memory issues (keep last 10000 chars)
            if (safeThis->logText.getTotalNumChars() > 10000)
            {
                safeThis->logText.setCaretPosition(0);
                safeThis->logText.setHighlightedRegion(juce::Range<int>(0, safeThis->logText.getTotalNumChars() - 8000));
                safeThis->logText.insertTextAtCaret("");
                safeThis->logText.moveCaretToEnd();
            }
        });
    }
    
    void clearLog()
    {
        logText.clear();
        juce::Logger::writeToLog("=== Debug Console Cleared ===");
    }
    
private:
    juce::TextButton clearButton;
    juce::TextEditor logText;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DebugConsole)
};
