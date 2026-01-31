#pragma once

#include <JuceHeader.h>
#include <fstream>
#include <mutex>

// =============================================================================
// DEBUG LOGGER - DISABLED FOR RELEASE
// =============================================================================
/*
class SubLogger {
public:
    static SubLogger& getInstance() {
        static SubLogger instance;
        return instance;
    }
    
    void log(const juce::String& message) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!logFile_.is_open()) {
            openLogFile();
        }
        
        if (logFile_.is_open()) {
            auto now = juce::Time::getCurrentTime();
            logFile_ << "[" << now.formatted("%Y-%m-%d %H:%M:%S").toStdString() << "] " 
                     << message.toStdString() << std::endl;
            // REMOVED: logFile_.flush();
            // Flush is expensive - let the OS buffer handle it
            // File will auto-flush on close or when buffer is full
        }
    }
    
    void logSeparator() {
        log("================================================");
    }
    
    // Call this explicitly if you need to ensure data is written (e.g., before crash)
    void flush() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (logFile_.is_open()) {
            logFile_.flush();
        }
    }
    
private:
    SubLogger() {
        openLogFile();
    }
    
    ~SubLogger() {
        if (logFile_.is_open()) {
            logFile_.flush();  // Flush on destruction is fine
            logFile_.close();
        }
    }
    
    void openLogFile() {
        // Get executable directory
        auto exeFile = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
        auto logDir = exeFile.getParentDirectory();
        auto logFile = logDir.getChildFile("subterraneum_debug.log");
        
        logFile_ = std::ofstream(logFile.getFullPathName().toStdString(), 
                                 std::ios::app); // Append mode
        
        if (logFile_.is_open()) {
            logSeparator();
            log("Log started: " + juce::String(logFile.getFullPathName()));
            logSeparator();
        }
    }
    
    std::ofstream logFile_;
    std::mutex mutex_;
    
    SubLogger(const SubLogger&) = delete;
    SubLogger& operator=(const SubLogger&) = delete;
};

// Convenience macro
#define LOG(msg) SubLogger::getInstance().log(msg)
*/

// Debug logging disabled - no-op macro
#define LOG(msg) ((void)0)

