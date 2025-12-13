#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_events/juce_events.h>
#include <juce_core/juce_core.h>
#include "AppLogger.h"
#include "UI/MainComponent.h"

// ======================================================================
// OnStageApplication - JUCE's auto-generated WinMain entry point
// ======================================================================
class OnStageApplication : public juce::JUCEApplication
{
public:
    OnStageApplication();
    ~OnStageApplication() override;

    // JUCE Required Overrides
    const juce::String getApplicationName() override;
    const juce::String getApplicationVersion() override;
    bool moreThanOneInstanceAllowed() override;

    // Initialize app (creates main window)
    void initialise(const juce::String& commandLine) override;

    // Shutdown app
    void shutdown() override;

    // Handle system quit (e.g., close button)
    void systemRequestedQuit() override;
    
    // Handle another instance (needed since we defined it in cpp previously)
    void anotherInstanceStarted(const juce::String& commandLine) override;

    // Suspend/resume (for mobile, ignore on desktop)
    void suspended() override {}
    void resumed() override {}

private:
    // Main Window Class Declaration
    // Implementation will be in App.cpp
    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow(juce::String name);
        void closeButtonPressed() override;

    private:
        MainComponent* mainComponentPtr = nullptr;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
    };

    // Main window instance
    std::unique_ptr<MainWindow> mainWindow;
};