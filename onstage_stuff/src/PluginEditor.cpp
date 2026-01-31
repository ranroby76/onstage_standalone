#include "PluginEditor.h"

SubterraneumAudioProcessorEditor::SubterraneumAudioProcessorEditor(SubterraneumAudioProcessor& p) 
    : AudioProcessorEditor(&p), audioProcessor(p), graphCanvas(p), mixerView(p), 
      audioSettingsTab(p), pluginManagerTab(p), studioTab(p), manualTab(p), 
      registrationTab(p), instrumentSelector(p) 
{ 
    setSize(1024, 768);
    setResizable(true, true); 
    setResizeLimits(800, 600, 3000, 2000); 
    
    // Load logo images from assets folder
    juce::File appDir = juce::File::getSpecialLocation(juce::File::currentApplicationFile).getParentDirectory();
    
    // Try multiple possible locations for assets
    juce::File assetsDir;
    if (appDir.getChildFile("assets").exists()) {
        assetsDir = appDir.getChildFile("assets");
    } else if (appDir.getParentDirectory().getChildFile("assets").exists()) {
        assetsDir = appDir.getParentDirectory().getChildFile("assets");
    } else {
        // Development path
        assetsDir = juce::File("D:/Workspace/Subterraneum_plugins_daw/assets");
    }
    
    juce::File subcoreFile = assetsDir.getChildFile("subcore_logo.png");
    juce::File colosseumFile = assetsDir.getChildFile("colosseum_logo.png");
    
    if (subcoreFile.existsAsFile()) {
        subcoreLogo = juce::ImageFileFormat::loadFrom(subcoreFile);
    }
    if (colosseumFile.existsAsFile()) {
        colosseumLogo = juce::ImageFileFormat::loadFrom(colosseumFile);
    }
    
    // ASIO Status LED with label
    asioLabel.setFont(juce::Font(9.0f));
    asioLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    asioLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(asioLabel);
    addAndMakeVisible(asioLed);
    
    // Registration LED with label
    regLabel.setFont(juce::Font(9.0f));
    regLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    regLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(regLabel);
    addAndMakeVisible(registrationLED);
    registrationLED.setActive(RegistrationManager::getInstance().isRegistered());
    
    addAndMakeVisible(loadButton); loadButton.addListener(this); 
    addAndMakeVisible(saveButton); saveButton.addListener(this); 
    addAndMakeVisible(resetButton); resetButton.addListener(this); 
    addAndMakeVisible(keysButton); keysButton.addListener(this); 
    
    addAndMakeVisible(instrumentSelector); 
    instrumentSelector.updateList(); 
    
    tabs.setOutline(0);
    tabs.setColour(juce::TabbedComponent::outlineColourId, juce::Colours::transparentBlack);
    tabs.addTab("Rack", Style::colBackground, &graphCanvas, false); 
    tabs.addTab("Mixer", Style::colBackground, &mixerView, false);
    tabs.addTab("Studio", Style::colBackground, &studioTab, false);
    tabs.addTab("Audio/MIDI", Style::colBackground, &audioSettingsTab, false);
    tabs.addTab("Plugins", Style::colBackground, &pluginManagerTab, false);
    tabs.addTab("Manual", Style::colBackground, &manualTab, false);
    tabs.addTab("Registration", Style::colBackground, &registrationTab, false);
    addAndMakeVisible(tabs);
    
    // Start timer for LED updates
    startTimer(500);
}

SubterraneumAudioProcessorEditor::~SubterraneumAudioProcessorEditor() {
    stopTimer();
}

void SubterraneumAudioProcessorEditor::timerCallback() {
    // Update registration LED
    bool isReg = RegistrationManager::getInstance().isRegistered();
    if (registrationLED.getActive() != isReg) {
        registrationLED.setActive(isReg);
    }
    
    // Update demo mode
    RegistrationManager::getInstance().updateDemoMode();
}

void SubterraneumAudioProcessorEditor::updateInstrumentSelector() { 
    instrumentSelector.updateList(); 
    resized();
}

void SubterraneumAudioProcessorEditor::buttonClicked(juce::Button* b) { 
    if (b == &loadButton) { 
        fileChooser = std::make_unique<juce::FileChooser>("Load Patch", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.subt");
        fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this](const juce::FileChooser& fc) { 
            auto file = fc.getResult(); 
            if (file != juce::File()) { 
                graphCanvas.closeAllPluginWindows();
                audioProcessor.loadUserPreset(file); 
                repaint(); 
                updateInstrumentSelector(); 
            } 
        });
    } else if (b == &saveButton) { 
        fileChooser = std::make_unique<juce::FileChooser>("Save Patch", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.subt");
        fileChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles, [this](const juce::FileChooser& fc) { 
            auto file = fc.getResult(); 
            if (file != juce::File()) audioProcessor.saveUserPreset(file); 
        });
    } else if (b == &resetButton) { 
        juce::NativeMessageBox::showAsync(juce::MessageBoxOptions()
            .withIconType(juce::MessageBoxIconType::QuestionIcon)
            .withTitle("Reset")
            .withMessage("Reset to default?")
            .withAssociatedComponent(this)
            .withButton("Yes")
            .withButton("No"), [this](int r) { 
            if (r == 0) { 
                graphCanvas.closeAllPluginWindows();
                audioProcessor.resetGraph(); 
                repaint(); 
                updateInstrumentSelector(); 
            } 
        });
    } else if (b == &keysButton) { 
        if (keyboardWindow == nullptr) 
            keyboardWindow = std::make_unique<VirtualKeyboardWindow>(audioProcessor);
        keyboardWindow->setVisible(!keyboardWindow->isVisible());
    } 
}

void SubterraneumAudioProcessorEditor::paint(juce::Graphics& g) { 
    g.fillAll(Style::colBackground); 
    
    // Draw main header (doubled height)
    g.setColour(juce::Colour(0xff1a1a1a));
    g.fillRect(0, 0, getWidth(), Style::mainHeaderHeight); 
    
    // Draw logos in header
    int logoY = 5;
    int logoHeight = Style::mainHeaderHeight - 10;
    int logoX = 10;
    
    // Draw Subcore logo (brand name) - aligned left, 52.5% size (75% of previous 70%)
    if (subcoreLogo.isValid()) {
        float aspectRatio = (float)subcoreLogo.getWidth() / (float)subcoreLogo.getHeight();
        int subcoreHeight = (int)(logoHeight * 0.525f);  // 75% of previous 70%
        int subcoreWidth = (int)(subcoreHeight * aspectRatio);
        int subcoreY = logoY + (logoHeight - subcoreHeight) / 2;  // Center vertically
        g.drawImage(subcoreLogo, logoX, subcoreY, subcoreWidth, subcoreHeight, 
                    0, 0, subcoreLogo.getWidth(), subcoreLogo.getHeight());
    }
    
    // Draw Colosseum logo - exact center of header
    if (colosseumLogo.isValid()) {
        float aspectRatio = (float)colosseumLogo.getWidth() / (float)colosseumLogo.getHeight();
        int colosseumWidth = (int)(logoHeight * aspectRatio);
        int colosseumX = (getWidth() - colosseumWidth) / 2;  // Center horizontally
        g.drawImage(colosseumLogo, colosseumX, logoY, colosseumWidth, logoHeight, 
                    0, 0, colosseumLogo.getWidth(), colosseumLogo.getHeight());
    }
    
    // If logos not loaded, show text fallback
    if (!subcoreLogo.isValid() && !colosseumLogo.isValid()) {
        g.setColour(juce::Colours::gold);
        g.setFont(juce::Font(24.0f, juce::Font::bold));
        g.drawText("COLOSSEUM", 10, 0, 300, Style::mainHeaderHeight, juce::Justification::centredLeft);
    }
    
    // Draw instrument header
    g.setColour(juce::Colours::black.withAlpha(0.8f));
    g.fillRect(0, Style::mainHeaderHeight, getWidth(), Style::instrHeaderHeight); 
    
    // Draw right menu background
    g.setColour(juce::Colours::gold.darker(0.6f));
    g.fillRect(getWidth() - Style::rightMenuWidth, 0, Style::rightMenuWidth, getHeight());
}

void SubterraneumAudioProcessorEditor::resized() { 
    auto area = getLocalBounds(); 
    auto rightMenu = area.removeFromRight(Style::rightMenuWidth); 
    rightMenu.removeFromTop(Style::mainHeaderHeight + Style::instrHeaderHeight);
    int btnH = 40; 
    int gap = 10;
    loadButton.setBounds(rightMenu.removeFromTop(btnH).reduced(10, 5)); 
    rightMenu.removeFromTop(gap); 
    saveButton.setBounds(rightMenu.removeFromTop(btnH).reduced(10, 5)); 
    rightMenu.removeFromTop(gap); 
    resetButton.setBounds(rightMenu.removeFromTop(btnH).reduced(10, 5)); 
    rightMenu.removeFromTop(gap); 
    keysButton.setBounds(rightMenu.removeFromTop(btnH).reduced(10, 5));
    
    auto mainHeader = area.removeFromTop(Style::mainHeaderHeight); 
    
    // Status LEDs in header (right side, before right menu)
    // Layout: [... logos ...] [REG LED] [ASIO LED] [margin]
    int ledSize = 14;
    int ledSpacing = 35;
    int rightMargin = 15;
    int headerCenterY = Style::mainHeaderHeight / 2;
    
    // ASIO LED (rightmost)
    int asioLedX = mainHeader.getRight() - rightMargin - ledSize;
    asioLed.setBounds(asioLedX, headerCenterY + 2, ledSize, ledSize);
    asioLabel.setBounds(asioLedX - 5, headerCenterY - 14, ledSize + 10, 14);
    
    // Registration LED (left of ASIO)
    int regLedX = asioLedX - ledSpacing;
    registrationLED.setBounds(regLedX, headerCenterY + 2, ledSize, ledSize);
    regLabel.setBounds(regLedX - 5, headerCenterY - 14, ledSize + 10, 14);
    
    auto instrHeader = area.removeFromTop(Style::instrHeaderHeight); 
    instrumentSelector.setBounds(instrHeader);
    tabs.setBounds(area);
}

// VirtualKeyboardWindow Implementation
SubterraneumAudioProcessorEditor::VirtualKeyboardWindow::VirtualKeyboardWindow(SubterraneumAudioProcessor& p) 
    : DocumentWindow("Virtual Keyboard", juce::Colours::black, DocumentWindow::allButtons), 
      keyboardComp(p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard) 
{ 
    setSize(600, 100);
    setUsingNativeTitleBar(true); 
    setContentOwned(&keyboardComp, false);
    setAlwaysOnTop(true);
}

SubterraneumAudioProcessorEditor::VirtualKeyboardWindow::~VirtualKeyboardWindow() {}
void SubterraneumAudioProcessorEditor::VirtualKeyboardWindow::closeButtonPressed() { setVisible(false); }