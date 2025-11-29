// *(Formerly `src/MainComponent.cpp` - Renamed class/includes)*

#include "LegacyMainComponent.h"
#include "UI/RegistrationComponent.h" 

LegacyMainComponent::LegacyMainComponent()
{
    addAndMakeVisible (headerRegisterButton);
    headerRegisterButton.addListener (this);
    
    if (registrationManager.isProMode())
    {
        headerRegisterButton.setButtonText("License Info");
    }
    else
    {
        headerRegisterButton.setButtonText("REGISTER");
        headerRegisterButton.setColour(juce::TextButton::textColourOffId, juce::Colours::orange);
    }

    setSize (800, 600);
}

LegacyMainComponent::~LegacyMainComponent()
{
    headerRegisterButton.removeListener (this);
}

void LegacyMainComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::darkgrey);
    g.setColour (juce::Colours::black);
    g.fillRect (getLocalBounds().removeFromTop (50));
    
    g.setColour (juce::Colours::white);
    g.setFont (20.0f);
    g.drawText ("Panduri VSTi (Legacy View)", 20, 0, 200, 50, juce::Justification::centredLeft);
}

void LegacyMainComponent::resized()
{
    auto headerArea = getLocalBounds().removeFromTop (50);
    headerRegisterButton.setBounds (headerArea.removeFromRight (120).reduced (10));
}

void LegacyMainComponent::buttonClicked (juce::Button* button)
{
    if (button == &headerRegisterButton)
    {
        juce::DialogWindow::LaunchOptions options;
        auto* dialogContent = new RegistrationComponent();
        options.content.setOwned (dialogContent);
        options.dialogTitle = "Registration";
        options.componentToCentreAround = this;
        options.dialogBackgroundColour = juce::Colour(0xffd35400);
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = false;
        options.resizable = false;
        options.launchAsync();
    }
}