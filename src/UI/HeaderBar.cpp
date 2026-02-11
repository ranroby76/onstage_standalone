#include "HeaderBar.h"
#include <juce_graphics/juce_graphics.h>
#include "BinaryData.h"
#include "RegistrationComponent.h"
#include "ManualComponent.h"

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
        opt.componentToCentreAround = getTopLevelComponent();
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
    registerButton.setColour(TextButton::buttonColourId, Colour(0xFF8B0000));
    registerButton.setColour(TextButton::textColourOffId, Colours::white);
    registerButton.onClick = [this]() { 
        DialogWindow::LaunchOptions opt;
        opt.content.setOwned(new RegistrationComponent());
        opt.dialogTitle = "Registration";
        opt.componentToCentreAround = getTopLevelComponent();
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
        int logoWidth = (int)(logoHeight * 5.668f);
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
    int width = getWidth();
    
    // ---------------------------------------------------------
    // 1. Calculate Logos Dimensions (Must match paint() logic)
    // ---------------------------------------------------------
    
    // Left Logo (Fanan)
    int leftMargin = 55;
    int fananHeight = height - 20;
    int fananWidth = (int)(fananHeight * 5.668f);
    int fananRightEdge = leftMargin + fananWidth;
    
    // Right Logo (OnStage)
    int rightMargin = 15;
    int onstageHeight = (int)((height - 20) * 0.805f);
    int onstageWidth = (int)(onstageHeight * 6.486f);
    int onstageLeftEdge = width - rightMargin - onstageWidth;

    // ---------------------------------------------------------
    // 2. Calculate Gap
    // ---------------------------------------------------------
    int availableSpaceBetweenLogos = onstageLeftEdge - fananRightEdge;
    
    // ---------------------------------------------------------
    // 3. Calculate Center Components Width
    // ---------------------------------------------------------
    int buttonWidth = 100;
    int buttonHeight = 30;
    int manualWidth = 80;
    int spacing = 10;
    int buttonY = (height - buttonHeight) / 2;
    int modeLabelWidth = 50;
    
    // Total width: Manual | Save | Load | Label | Register | Mode
    int totalCenterGroupWidth = manualWidth + (buttonWidth * 2) + 150 + 80 + (spacing * 5) + modeLabelWidth;

    // ---------------------------------------------------------
    // 4. Calculate Identical Gap
    // ---------------------------------------------------------
    int gap = (availableSpaceBetweenLogos - totalCenterGroupWidth) / 2;
    if (gap < 0) gap = 0;
    
    // ---------------------------------------------------------
    // 5. Position Components (left to right)
    // ---------------------------------------------------------
    int startX = fananRightEdge + gap;

    // 1. Manual Button
    manualButton.setBounds(startX, buttonY, manualWidth, buttonHeight);
    
    // 2. Save Preset
    savePresetButton.setBounds(manualButton.getRight() + spacing, buttonY, buttonWidth, buttonHeight);
    
    // 3. Load Preset
    loadPresetButton.setBounds(savePresetButton.getRight() + spacing, buttonY, buttonWidth, buttonHeight);
    
    // 4. Preset Name Label
    presetNameLabel.setBounds(loadPresetButton.getRight() + spacing, buttonY, 150, buttonHeight);
    
    // 5. Register Button
    registerButton.setBounds(presetNameLabel.getRight() + spacing, buttonY, 80, buttonHeight);
    
    // 6. Mode Label
    modeLabel.setBounds(registerButton.getRight() + spacing, buttonY, modeLabelWidth, buttonHeight);
}

void HeaderBar::setPresetName(const juce::String& name)
{
    currentPresetName = name.isEmpty() ? "No Preset" : name;
    presetNameLabel.setText(currentPresetName, juce::dontSendNotification);
}