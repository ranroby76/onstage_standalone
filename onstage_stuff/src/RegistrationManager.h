// **Changes:** 1.  Cleaned up unnecessary inclusions. 2.  Added `decodeMidiToString` and `noteToChar` helpers. <!-- end list -->

#pragma once
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>

class RegistrationManager
{
public:
    static RegistrationManager& getInstance() {
        static RegistrationManager instance;
        return instance;
    }

    // Main Check
    void checkRegistration();
    bool isProMode() const { return isRegistered; }

    // User Action
    bool tryRegister(const juce::String& serialInput);
    juce::String getMachineIDString(); 

private:
    RegistrationManager() = default;
    ~RegistrationManager() = default;
    bool isRegistered = false;
    
    // Core Logic
    long long calculateExpectedSerial();
    long long evaluateFormula(const juce::String& formula);
    
    // MIDI Steganography Helpers
    juce::String decodeMidiToString(const void* data, size_t size);
    char noteToChar(int note);

    // Hardware ID
    int getMachineIDNumber();
    juce::String getSystemVolumeSerial();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RegistrationManager)
};