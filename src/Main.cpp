// **Fix:** Wrapped `WinMain` in a `try/catch` block to capture and log the crash reason.

// ======================================================================
// Main.cpp - OnStage Entry Point (Explicit WinMain)
// ======================================================================
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_events/juce_events.h>
#include <juce_core/juce_core.h>
#include "App.h"
#include "AppLogger.h" // Include logger to report crashes

// ======================================================================
// Factory function for creating the app
// ======================================================================
juce::JUCEApplicationBase* juce_CreateApplication()
{
    return new OnStageApplication();
}

// ======================================================================
// Explicit WinMain Entry Point for Windows
// ======================================================================
#if defined(_WIN32) || defined(_WIN64)

#include <windows.h>

int __stdcall WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    juce::JUCEApplicationBase::createInstance = &juce_CreateApplication;
    
    try 
    {
        return juce::JUCEApplicationBase::main();
    }
    catch (const std::exception& e)
    {
        // NAIL THE PROBLEM: Log the standard exception
        juce::String errorMsg = "CRITICAL CRASH: " + juce::String(e.what());
        
        // Log to file if possible
        // We try-catch this too in case Logger isn't ready
        try { AppLogger::getInstance().logError(errorMsg); } catch(...) {}
        
        // Show a loud box to the user so they know exactly what happened
        juce::NativeMessageBox::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
            "Application Crashed", 
            errorMsg + "\n\nPlease check the log file.");
            
        return -1;
    }
    catch (...)
    {
        // Catch non-standard exceptions
        juce::String errorMsg = "CRITICAL CRASH: Unknown Exception occurred!";
        try { AppLogger::getInstance().logError(errorMsg); } catch(...) {}
        
        juce::NativeMessageBox::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
            "Application Crashed", 
            "An unknown system error occurred.");
            
        return -1;
    }
}

#else

// ======================================================================
// Standard main() for non-Windows platforms
// ======================================================================
int main(int argc, char* argv[])
{
    juce::JUCEApplicationBase::createInstance = &juce_CreateApplication;
    return juce::JUCEApplicationBase::main(argc, (const char**)argv);
}

#endif