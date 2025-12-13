// **3. Update the build configuration to include the new files.**

/*
  ==============================================================================

    SharedMemoryManager.cpp
    OnStage

  ==============================================================================
*/

#include "SharedMemoryManager.h"

SharedMemoryManager::SharedMemoryManager(Mode mode)
    : currentMode(mode)
{
}

SharedMemoryManager::~SharedMemoryManager()
{
}

void SharedMemoryManager::initialize()
{
    // Stub: Simulate a successful connection for now so logic flows
    // In a real implementation, this would open NamedPipes or SharedMemory
    connected = true;
}

bool SharedMemoryManager::isConnected() const
{
    return connected;
}

void SharedMemoryManager::sendCommand(const juce::String& commandJson)
{
    juce::ignoreUnused(commandJson);
    // Stub: Send logic
}

void SharedMemoryManager::pushAudio(const juce::AudioBuffer<float>& buffer)
{
    const juce::ScopedLock sl(lock);
    juce::ignoreUnused(buffer);
    // Stub: Push audio logic
}

void SharedMemoryManager::popAudio(juce::AudioBuffer<float>& buffer)
{
    const juce::ScopedLock sl(lock);
    // Stub: Clear buffer (silence) since we have no real audio coming in yet
    buffer.clear();
}

SharedMemoryManager::EngineStatus SharedMemoryManager::getEngineStatus()
{
    // Stub: Return default status
    return EngineStatus();
}