#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_events/juce_events.h>
#include "App.h"
#include "UI/MainComponent.h"
#include "AppLogger.h"

// ==============================================================================
// OnStageApplication Implementation
// ==============================================================================

OnStageApplication::OnStageApplication()
{
    // Logger is initialized via singleton when needed, 
    // but we can log the start here.
    // Note: AppLogger::getInstance() creates the log file.
}

OnStageApplication::~OnStageApplication()
{
}

const juce::String OnStageApplication::getApplicationName()
{
    return "OnStage";
}

const juce::String OnStageApplication::getApplicationVersion()
{
    return "1.0.0";
}

bool OnStageApplication::moreThanOneInstanceAllowed()
{
    return true;
}

void OnStageApplication::initialise(const juce::String& commandLine)
{
    juce::ignoreUnused(commandLine);
    
    LOG_INFO("=== OnStageApplication::initialise START ===");

    try {
        LOG_INFO("Creating MainWindow...");
        mainWindow.reset(new MainWindow(getApplicationName()));
        LOG_INFO("MainWindow created successfully");
    }
    catch (const std::exception& e) {
        LOG_ERROR("EXCEPTION during initialise: " + juce::String(e.what()));
        juce::NativeMessageBox::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
            "Startup Error", 
            "Failed to create main window: " + juce::String(e.what()));
        quit();
    }
    catch (...) {
        LOG_ERROR("UNKNOWN EXCEPTION during initialise");
        juce::NativeMessageBox::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
            "Startup Error", 
            "Unknown error occurred during startup");
        quit();
    }
    
    LOG_INFO("=== OnStageApplication::initialise COMPLETE ===");
}

void OnStageApplication::shutdown()
{
    LOG_INFO("OnStageApplication shutdown - closing MainWindow");
    mainWindow = nullptr;
}

void OnStageApplication::systemRequestedQuit()
{
    LOG_INFO("System requested quit");
    quit();
}

void OnStageApplication::anotherInstanceStarted(const juce::String& commandLine)
{
    juce::ignoreUnused(commandLine);
}

// ==============================================================================
// MainWindow Implementation
// ==============================================================================

OnStageApplication::MainWindow::MainWindow(juce::String name)
    : DocumentWindow(name,
        juce::Desktop::getInstance().getDefaultLookAndFeel()
            .findColour(juce::ResizableWindow::backgroundColourId),
        DocumentWindow::allButtons)
{
    LOG_INFO("MainWindow constructor starting");
    
    try {
        LOG_INFO("Step 1: Setting native title bar");
        setUsingNativeTitleBar(true);
        
        LOG_INFO("Step 2: Creating MainComponent...");
        auto* mainComp = new MainComponent();
        mainComponentPtr = mainComp; // Store pointer for restoration callback

        LOG_INFO("Step 3: MainComponent created, setting as content");
        setContentOwned(mainComp, true);
        
        LOG_INFO("Step 4: Window set as resizable");
        setResizable(true, false);

        LOG_INFO("Step 5: Window centered with size 1280x720");
        centreWithSize(1280, 720);

        LOG_INFO("Step 6: Window set to visible");
        setVisible(true);
        
        LOG_INFO("MainWindow constructor completed successfully");
        
        // CRITICAL FIX: Restore ASIO settings AFTER window is fully shown
        // This prevents race conditions during construction
        juce::Timer::callAfterDelay(200, [this]() {
            if (mainComponentPtr != nullptr)
            {
                LOG_INFO("Triggering restoreIOSettings() from MainWindow");
                mainComponentPtr->restoreIOSettings();
            }
        });
    }
    catch (const std::exception& e) {
        LOG_ERROR("Exception in MainWindow constructor: " + juce::String(e.what()));
        throw;
    }
    catch (...) {
        LOG_ERROR("Unknown exception in MainWindow constructor");
        throw;
    }
}

void OnStageApplication::MainWindow::closeButtonPressed()
{
    LOG_INFO("Close button pressed - quitting application");
    juce::JUCEApplication::getInstance()->systemRequestedQuit();
}