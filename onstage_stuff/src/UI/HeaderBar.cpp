// **Changes:** 1.  Implemented `manualButton` initialization and click handler (opens `ManualComponent` in a Dialog). 2.  Updated `resized()` to place the Manual button to the left of the Save button. <!-- end list -->

#include "HeaderBar.h"
#include <juce_graphics/juce_graphics.h>
#include "BinaryData.h"
#include "RegistrationComponent.h"
#include "ManualComponent.h" // NEW

using namespace juce;

HeaderBar::HeaderBar(AudioEngine& engine) : audioEngine(engine)
{
    fananLogo   = ImageFileFormat::loadFrom(BinaryData::logo_png, BinaryData::logo_pngSize);
    onStageLogo = ImageFileFormat::loadFrom(BinaryData::On_stage_logo_png, BinaryData::On_stage_logo_pngSize);
    
    // MANUAL BUTTON
    addAndMakeVisible(manualButton);
    manualButton.setButtonText("Manual");
    manualButton.setColour(TextButton::buttonColourId, Colour(0xFF2A2A2A));
    manualButton.setColour(TextButton::textColourOnId, Colour(0xFFD4AF37));
    manualButton.setColour(TextButton::textColourOffId, Colour(0xFFD4AF37));
    manualButton.onClick = [this]() {
        DialogWindow::LaunchOptions opt;
        opt.content.setOwned(new ManualComponent());
        opt.dialogTitle = "OnStage User Manual";
        opt.componentToCentreAround = this;
        opt.dialogBackgroundColour = Colour(0xFF202020);
        opt.useNativeTitleBar = true;
        opt.resizable = false;
        opt.launchAsync();
    };

    addAndMakeVisible(savePresetButton);
    savePresetButton.setButtonText("Save Preset");
    savePresetButton.setColour(TextButton::buttonColourId, Colour(0xFF2A2A2A));
    savePresetButton.setColour(TextButton::textColourOnId, Colour(0xFFD4AF37));
    savePresetButton.setColour(TextButton::textColourOffId, Colour(0xFFD4AF37));
    savePresetButton.onClick = [this]() { if (onSavePreset) onSavePreset(); };
    
    addAndMakeVisible(loadPresetButton);
    loadPresetButton.setButtonText("Load Preset");
    loadPresetButton.setColour(TextButton::buttonColourId, Colour(0xFF2A2A2A));
    loadPresetButton.setColour(TextButton::textColourOnId, Colour(0xFFD4AF37));
    loadPresetButton.setColour(TextButton::textColourOffId, Colour(0xFFD4AF37));
    loadPresetButton.onClick = [this]() { if (onLoadPreset) onLoadPreset(); };
    
    addAndMakeVisible(presetNameLabel);
    presetNameLabel.setText("No Preset", dontSendNotification);
    presetNameLabel.setJustificationType(Justification::centred);
    presetNameLabel.setColour(Label::textColourId, Colour(0xFFD4AF37));
    presetNameLabel.setColour(Label::backgroundColourId, Colour(0xFF1A1A1A));
    presetNameLabel.setColour(Label::outlineColourId, Colour(0xFF404040));
    presetNameLabel.setFont(Font(14.0f, Font::bold));
    
    // REGISTER BUTTON
    addAndMakeVisible(registerButton);
    registerButton.setButtonText("REGISTER");
    registerButton.setColour(TextButton::buttonColourId, Colour(0xFF8B0000)); // Dark Red
    registerButton.setColour(TextButton::textColourOffId, Colours::white);
    registerButton.onClick = [this]() { 
        DialogWindow::LaunchOptions opt;
        opt.content.setOwned(new RegistrationComponent());
        opt.dialogTitle = "Registration";
        opt.componentToCentreAround = this;
        opt.dialogBackgroundColour = Colour(0xFFE08020);
        opt.useNativeTitleBar = true;
        opt.resizable = false;
        opt.launchAsync();
    };
    
    addAndMakeVisible(modeLabel);
    modeLabel.setFont(Font(14.0f, Font::bold));
    modeLabel.setJustificationType(Justification::centredLeft);
    
    currentPresetName = "No Preset";
    startTimer(1000);
    timerCallback();
}

void HeaderBar::timerCallback()
{
    bool isPro = RegistrationManager::getInstance().isProMode();
    if (isPro) {
        modeLabel.setText("PRO", dontSendNotification);
        modeLabel.setColour(Label::textColourId, Colours::lightgreen);
    } else {
        modeLabel.setText("DEMO", dontSendNotification);
        modeLabel.setColour(Label::textColourId, Colours::red);
    }
}

void HeaderBar::paint(Graphics& g)
{
    g.fillAll(Colour(0xFF202020));

    auto area = getLocalBounds();
    int height = area.getHeight();

    if (fananLogo.isValid())
    {
        int logoHeight = height - 20;
        int logoWidth = (int)(logoHeight * 2.303f);
        Rectangle<int> fananArea(55, (height - logoHeight) / 2, logoWidth, logoHeight);
        g.drawImageWithin(fananLogo, fananArea.getX(), fananArea.getY(), 
                         fananArea.getWidth(), fananArea.getHeight(),
                         RectanglePlacement::centred);
    }

    if (onStageLogo.isValid())
    {
        int logoHeight = (int)((height - 20) * 0.805f);
        int logoWidth = (int)(logoHeight * 6.486f);
        int xPos = getWidth() - logoWidth - 15;
        Rectangle<int> onstageArea(xPos, (height - logoHeight) / 2, logoWidth, logoHeight);
        g.drawImageWithin(onStageLogo, onstageArea.getX(), onstageArea.getY(),
                         onstageArea.getWidth(), onstageArea.getHeight(),
                         RectanglePlacement::centred);
    }
}

void HeaderBar::resized()
{
    auto area = getLocalBounds();
    int height = area.getHeight();
    
    int buttonWidth = 100;
    int buttonHeight = 30;
    int manualWidth = 80; // Slightly smaller for Manual
    int spacing = 10;
    int buttonY = (height - buttonHeight) / 2;

    // Calculate total width of center block
    int totalCenterGroupWidth = manualWidth + (buttonWidth * 2) + 150 + 80 + (spacing * 4);
    int modeLabelWidth = 50;
    
    totalCenterGroupWidth += modeLabelWidth + spacing;
    int startX = (getWidth() - totalCenterGroupWidth) / 2;

    // 1. Manual Button
    manualButton.setBounds(startX, buttonY, manualWidth, buttonHeight);
    
    // 2. Save Preset
    savePresetButton.setBounds(manualButton.getRight() + spacing, buttonY, buttonWidth, buttonHeight);
    
    // 3. Load Preset
    loadPresetButton.setBounds(savePresetButton.getRight() + spacing, buttonY, buttonWidth, buttonHeight);
    
    // 4. Label
    presetNameLabel.setBounds(loadPresetButton.getRight() + spacing, buttonY, 150, buttonHeight);
    
    // 5. Register
    registerButton.setBounds(presetNameLabel.getRight() + spacing, buttonY, 80, buttonHeight);
    
    // 6. Mode Label
    modeLabel.setBounds(registerButton.getRight() + spacing, buttonY, modeLabelWidth, buttonHeight);
}

void HeaderBar::setPresetName(const juce::String& name)
{
    currentPresetName = name.isEmpty() ? "No Preset" : name;
    presetNameLabel.setText(currentPresetName, juce::dontSendNotification);
}