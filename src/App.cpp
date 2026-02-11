#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_events/juce_events.h>
#include "App.h"
#include "UI/MainComponent.h"
#include "AppLogger.h"

#if JUCE_WINDOWS
    #include <Windows.h>
#endif

// ==============================================================================
// OnStageApplication Implementation
// ==============================================================================

OnStageApplication::OnStageApplication()
{
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

    auto options = juce::MessageBoxOptions()
        .withIconType (juce::MessageBoxIconType::QuestionIcon)
        .withTitle ("Exit OnStage")
        .withMessage ("Are you sure you want to exit?")
        .withButton ("Yes")
        .withButton ("No")
        .withAssociatedComponent (mainWindow.get());

    juce::NativeMessageBox::showAsync (options, [] (int result)
    {
        if (result == 0)
            juce::JUCEApplication::getInstance()->quit();
    });
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
        
        LOG_INFO("Step 2: Initialising AudioEngine...");
        audioEngine.initialise();

        LOG_INFO("Step 3: Creating PresetManager...");
        presetMgr = std::make_unique<PresetManager>(audioEngine);

        LOG_INFO("Step 4: Creating MainComponent...");
        auto* mainComp = new MainComponent(audioEngine, *presetMgr);

        LOG_INFO("Step 5: MainComponent created, setting as content");
        setContentOwned(mainComp, true);
        
        LOG_INFO("Step 6: Window set as resizable");
        setResizable(true, false);

        LOG_INFO("Step 7: Setting initial size before maximize");
        centreWithSize(1280, 720);

        LOG_INFO("Step 8: Window set to visible");
        setVisible(true);
        
        LOG_INFO("Step 9: Maximizing window");
        #if JUCE_WINDOWS
            // Use Windows API to maximize (keeps title bar, minimize/restore functional)
            if (auto* peer = getPeer())
            {
                if (auto hwnd = (HWND) peer->getNativeHandle())
                {
                    ShowWindow(hwnd, SW_MAXIMIZE);
                }
            }
        #elif JUCE_MAC
            // On Mac, use JUCE's built-in method
            setFullScreen(true);
        #else
            // Linux fallback - set to screen bounds
            auto display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
            if (display != nullptr)
                setBounds(display->userArea);
        #endif
        
        LOG_INFO("MainWindow constructor completed successfully");
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

OnStageApplication::MainWindow::~MainWindow()
{
    LOG_INFO("MainWindow destructor starting");
    
    // CRITICAL: Clear content FIRST to destroy MainComponent
    // while AudioEngine is still valid (MainComponent references it)
    clearContentComponent();
    
    // Now PresetManager can be destroyed (it references AudioEngine)
    presetMgr.reset();
    
    // Finally, AudioEngine destructor will be called automatically
    // (it's a member, so it gets destroyed after this destructor body)
    
    LOG_INFO("MainWindow destructor complete");
}

void OnStageApplication::MainWindow::closeButtonPressed()
{
    LOG_INFO("Close button pressed - quitting application");
    juce::JUCEApplication::getInstance()->systemRequestedQuit();
}

// ==============================================================================
// This macro generates the main() routine that launches the app.
// ==============================================================================
START_JUCE_APPLICATION(OnStageApplication)