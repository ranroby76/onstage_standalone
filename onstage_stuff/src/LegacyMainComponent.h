// *(Formerly `src/MainComponent.h` - Renamed class to avoid conflict)*

#pragma once
#include <JuceHeader.h>
#include "RegistrationManager.h"
// NOTE: This is the legacy component. The active one is in src/UI/MainComponent.h

class LegacyMainComponent  : public juce::Component,
                             public juce::Button::Listener
{
public:
    LegacyMainComponent();
    ~LegacyMainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    
    void buttonClicked (juce::Button* button) override;

private:
    RegistrationManager& registrationManager = RegistrationManager::getInstance();
    
    juce::TextButton headerRegisterButton { "Register" };
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LegacyMainComponent)
};