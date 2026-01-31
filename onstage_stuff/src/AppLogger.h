#pragma once
#include <juce_core/juce_core.h>
#include <fstream>
#include <mutex>

class AppLogger
{
public:
    enum class Level
    {
        Info,
        Warning,
        Error,
        Debug
    };

    static AppLogger& getInstance()
    {
        static AppLogger instance;
        return instance;
    }

    void log(Level level, const juce::String& message)
    {
        std::lock_guard<std::mutex> lock(mutex);
        
        if (!logFile.is_open())
        {
            // Try to reopen if closed
            tryOpenLogFile();
        }
        
        if (!logFile.is_open())
        {
            // If still can't open, at least output to debugger
            DBG("[LOGFILE FAILED] " + message);
            return;
        }
            
        auto time = juce::Time::getCurrentTime().toString(true, true, true, true);
        juce::String levelStr;
        
        switch (level)
        {
            case Level::Info:    levelStr = "INFO"; break;
            case Level::Warning: levelStr = "WARN"; break;
            case Level::Error:   levelStr = "ERROR"; break;
            case Level::Debug:   levelStr = "DEBUG"; break;
        }
        
        juce::String logLine = "[" + time + "] [" + levelStr + "] " + message;
        logFile << logLine.toStdString() << std::endl;
        logFile.flush(); // Force immediate write
        
        // Also output to debugger
        DBG(logLine);
    }

    void logInfo(const juce::String& message)    { log(Level::Info, message); }
    void logWarning(const juce::String& message) { log(Level::Warning, message); }
    void logError(const juce::String& message)   { log(Level::Error, message); }
    void logDebug(const juce::String& message)   { log(Level::Debug, message); }

private:
    AppLogger()
    {
        tryOpenLogFile();
        
        if (logFile.is_open())
        {
            log(Level::Info, "========================================");
            log(Level::Info, "OnStage Application Started");
            log(Level::Info, "Log file: " + logFilePath.getFullPathName());
            log(Level::Info, "Working directory: " + juce::File::getCurrentWorkingDirectory().getFullPathName());
            log(Level::Info, "========================================");
        }
        else
        {
            DBG("CRITICAL: Failed to create log file at: " + logFilePath.getFullPathName());
        }
    }

    ~AppLogger()
    {
        if (logFile.is_open())
        {
            log(Level::Info, "OnStage Application Closed");
            log(Level::Info, "========================================");
            logFile.close();
        }
    }
    
    void tryOpenLogFile()
    {
        // Try multiple locations for log file
        juce::File possibleLocations[] = {
            juce::File::getCurrentWorkingDirectory().getChildFile("onstage.log"),
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("onstage.log"),
            juce::File::getSpecialLocation(juce::File::userDesktopDirectory).getChildFile("onstage.log"),
            juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("onstage.log")
        };
        
        for (auto& location : possibleLocations)
        {
            logFilePath = location;
            logFile.open(logFilePath.getFullPathName().toStdString(), 
                        std::ios::out | std::ios::app);
            
            if (logFile.is_open())
            {
                logFile << "\n" << std::endl; // Test write
                logFile.flush();
                DBG("Log file opened successfully at: " + logFilePath.getFullPathName());
                return;
            }
        }
        
        DBG("CRITICAL: Could not open log file at any location!");
    }

    std::ofstream logFile;
    std::mutex mutex;
    juce::File logFilePath;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AppLogger)
};

// Convenience macros
#define LOG_INFO(msg)    AppLogger::getInstance().logInfo(msg)
#define LOG_WARNING(msg) AppLogger::getInstance().logWarning(msg)
#define LOG_ERROR(msg)   AppLogger::getInstance().logError(msg)
#define LOG_DEBUG(msg)   AppLogger::getInstance().logDebug(msg)
