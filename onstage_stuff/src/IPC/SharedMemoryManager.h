// **2. Create the missing implementation file.**

/*
  ==============================================================================

    SharedMemoryManager.h
    OnStage

    Handles Inter-Process Communication (IPC) logic.
    Currently stubbed to fix build errors.

  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>

class SharedMemoryManager
{
public:
    enum class Mode
    {
        Plugin_Client,
        Engine_Host
    };

    struct EngineStatus
    {
        bool playing = false;
        bool finished = false;
        bool winOpen = false;
        float pos = 0.0f;
        int64_t len = 0;
    };

    SharedMemoryManager(Mode mode);
    ~SharedMemoryManager();

    void initialize();
    bool isConnected() const;

    // Commands
    void sendCommand(const juce::String& commandJson);

    // Audio I/O
    void pushAudio(const juce::AudioBuffer<float>& buffer);
    void popAudio(juce::AudioBuffer<float>& buffer);

    // Status
    EngineStatus getEngineStatus();

private:
    Mode currentMode;
    bool connected = false;

    juce::CriticalSection lock;
    
    // Placeholder for real IPC members (NamedPipe, MemoryBlock, etc.)
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SharedMemoryManager)
};