#pragma once

#include <JuceHeader.h>

// =============================================================================
// RegistrationManager - Handles app registration and demo mode
// =============================================================================
class RegistrationManager {
public:
    static RegistrationManager& getInstance() {
        static RegistrationManager instance;
        return instance;
    }
    
    // Check if license file exists and is valid
    void checkRegistration();
    
    // Check registration status
    bool isRegistered() const { return registered; }
    
    // Try to register with a serial number
    bool tryRegister(const juce::String& serialInput);
    
    // Get machine ID for display to user
    juce::String getMachineIDString();
    int getMachineIDNumber();
    
    // Demo mode control
    bool isDemoSilenceActive() const { return demoSilenceActive.load(); }
    void updateDemoMode();  // Call this periodically to manage demo silencing
    
private:
    RegistrationManager() = default;
    ~RegistrationManager() = default;
    
    bool registered = false;
    
    // Demo mode timing
    std::atomic<bool> demoSilenceActive { false };
    double lastSilenceToggleTime = 0.0;
    bool inSilencePeriod = false;
    
    // Serial calculation
    long long calculateExpectedSerial();
    
    // Hardware ID helpers
    juce::String getSystemVolumeSerial();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RegistrationManager)
};
