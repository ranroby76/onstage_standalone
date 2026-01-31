// FIX: Plugin Browser Panel is now a fixed 240px panel to the left of yellow menu
// No toggle button - always visible on Rack tab, hidden on other tabs

#include "PluginEditor.h"
#include "SimpleConnectorProcessor.h"
#include "StereoMeterProcessor.h"
#include "MidiMonitorProcessor.h"

#if JUCE_WINDOWS
#include <windows.h>
#include <psapi.h>
#elif JUCE_MAC
#include <mach/mach.h>
#elif JUCE_LINUX
#include <sys/sysinfo.h>
#include <fstream>
#include <sstream>
#endif

SubterraneumAudioProcessorEditor::SubterraneumAudioProcessorEditor(SubterraneumAudioProcessor& p)
    : AudioProcessorEditor(&p), 
      audioProcessor(p), 
      graphCanvas(p), 
      mixerView(p),
      audioSettingsTab(p), 
      pluginManagerTab(p),
      studioTab(p),
      manualTab(p),
      registrationTab(p),
      instrumentSelector(p)
{
    setSize(1920, 1080);
    
    // Load logos - try multiple paths
    auto exePath = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
    auto exeDir = exePath.getParentDirectory();
    
    auto assetsDir = exeDir.getParentDirectory().getChildFile("assets");
    
    #if JUCE_MAC
    auto deployDir = exeDir.getParentDirectory().getChildFile("Resources");
    #else
    auto deployDir = exeDir;
    #endif
    
    juce::File fananLogoFile = assetsDir.getChildFile("fanan logo.png");
    if (!fananLogoFile.existsAsFile()) {
        fananLogoFile = deployDir.getChildFile("fanan logo.png");
    }
    if (!fananLogoFile.existsAsFile()) {
        fananLogoFile = juce::File("D:/Workspace/Subterraneum_plugins_daw/assets/fanan logo.png");
    }
    
    juce::File colosseumLogoFile = assetsDir.getChildFile("colosseum_logo.png");
    if (!colosseumLogoFile.existsAsFile()) {
        colosseumLogoFile = deployDir.getChildFile("colosseum_logo.png");
    }
    if (!colosseumLogoFile.existsAsFile()) {
        colosseumLogoFile = juce::File("D:/Workspace/Subterraneum_plugins_daw/assets/colosseum_logo.png");
    }
    
    if (fananLogoFile.existsAsFile()) {
        fananLogo = juce::ImageFileFormat::loadFrom(fananLogoFile);
    }
    
    if (colosseumLogoFile.existsAsFile()) {
        colosseumLogo = juce::ImageFileFormat::loadFrom(colosseumLogoFile);
    }
    
    addAndMakeVisible(loadButton); loadButton.addListener(this);
    addAndMakeVisible(saveButton); saveButton.addListener(this);
    addAndMakeVisible(resetButton); resetButton.addListener(this);
    addAndMakeVisible(keysButton); keysButton.addListener(this);
    
    // ASIO LED and label
    addAndMakeVisible(asioLed);
    asioLabel.setFont(juce::Font(10.0f, juce::Font::bold));
    asioLabel.setJustificationType(juce::Justification::centred);
    asioLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(asioLabel);
    
    // Registration LED and label
    addAndMakeVisible(registrationLED);
    regLabel.setFont(juce::Font(10.0f, juce::Font::bold));
    regLabel.setJustificationType(juce::Justification::centred);
    regLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(regLabel);
    
    // CPU and RAM labels
    cpuLabel.setFont(juce::Font(12.0f, juce::Font::bold));
    cpuLabel.setJustificationType(juce::Justification::centredRight);
    cpuLabel.setColour(juce::Label::textColourId, juce::Colours::cyan);
    addAndMakeVisible(cpuLabel);
    
    ramLabel.setFont(juce::Font(12.0f, juce::Font::bold));
    ramLabel.setJustificationType(juce::Justification::centredRight);
    ramLabel.setColour(juce::Label::textColourId, juce::Colours::orange);
    addAndMakeVisible(ramLabel);
    
    // Left green menu tab buttons
    addAndMakeVisible(rackButton); rackButton.addListener(this);
    addAndMakeVisible(mixerButton); mixerButton.addListener(this);
    addAndMakeVisible(studioButton); studioButton.addListener(this);
    addAndMakeVisible(settingsButton); settingsButton.addListener(this);
    addAndMakeVisible(pluginsButton); pluginsButton.addListener(this);
    addAndMakeVisible(manualButton); manualButton.addListener(this);
    addAndMakeVisible(registerButton); registerButton.addListener(this);
    
    addAndMakeVisible(instrumentSelector); 
    instrumentSelector.updateList(); 
    
    // Setup tabs
    tabs.setOutline(0);
    tabs.setTabBarDepth(0);
    tabs.setColour(juce::TabbedComponent::outlineColourId, juce::Colours::transparentBlack);
    tabs.addTab("Rack", Style::colBackground, &graphCanvas, false); 
    tabs.addTab("Mixer", Style::colBackground, &mixerView, false);
    tabs.addTab("Studio", Style::colBackground, &studioTab, false);
    tabs.addTab("Settings", Style::colBackground, &audioSettingsTab, false);
    tabs.addTab("Plugins", Style::colBackground, &pluginManagerTab, false);
    tabs.addTab("Manual", Style::colBackground, &manualTab, false);
    tabs.addTab("Register", Style::colBackground, &registrationTab, false);
    addAndMakeVisible(tabs);
    
    tabs.setCurrentTabIndex(0, false);
    updateTabButtonColors();
    
    // =========================================================================
    // Plugin Browser Panel - Fixed 240px panel to left of yellow menu
    // =========================================================================
    pluginBrowserPanel = std::make_unique<PluginBrowserPanel>(audioProcessor);
    pluginBrowserPanel->onPluginDropped = [this](const juce::PluginDescription& desc, juce::Point<int> pos) {
        graphCanvas.addPluginAtPosition(desc, pos);
    };
    pluginBrowserPanel->onToolDropped = [this](SystemToolType toolType, juce::Point<int> pos) {
        // Add system tool to graph at the position
        juce::AudioProcessorGraph::Node::Ptr nodePtr;
        
        switch (toolType) {
            case SystemToolType::Connector:
                nodePtr = audioProcessor.mainGraph->addNode(std::unique_ptr<juce::AudioProcessor>(new SimpleConnectorProcessor()));
                break;
            case SystemToolType::StereoMeter:
                nodePtr = audioProcessor.mainGraph->addNode(std::unique_ptr<juce::AudioProcessor>(new StereoMeterProcessor()));
                break;
            case SystemToolType::MidiMonitor:
                nodePtr = audioProcessor.mainGraph->addNode(std::unique_ptr<juce::AudioProcessor>(new MidiMonitorProcessor()));
                break;
            default:
                return;
        }
        
        if (nodePtr) {
            nodePtr->properties.set("x", (double)pos.x);
            nodePtr->properties.set("y", (double)pos.y);
            graphCanvas.markDirty();
        }
    };
    addAndMakeVisible(pluginBrowserPanel.get());  // Visible by default on Rack tab
    
    // Start timer
    startTimer(500);
    
    // Set fullscreen on startup
    juce::MessageManager::callAsync([this]() {
        if (auto* peer = getPeer()) {
            peer->setFullScreen(true);
        }
    });
    
    // Enable keyboard focus for shortcuts
    setWantsKeyboardFocus(true);
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
    
    RegistrationManager::getInstance().updateDemoMode();
    
    // Update CPU and RAM meters
    #if JUCE_WINDOWS
    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
        SIZE_T usedMemory = pmc.WorkingSetSize;
        int ramMB = (int)(usedMemory / (1024 * 1024));
        ramLabel.setText("RAM: " + juce::String(ramMB) + "MB", juce::dontSendNotification);
    }
    #elif JUCE_MAC
    struct task_basic_info info;
    mach_msg_type_number_t size = TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t)&info, &size) == KERN_SUCCESS) {
        int ramMB = (int)(info.resident_size / (1024 * 1024));
        ramLabel.setText("RAM: " + juce::String(ramMB) + "MB", juce::dontSendNotification);
    }
    #elif JUCE_LINUX
    std::ifstream statusFile("/proc/self/status");
    std::string line;
    int ramKB = 0;
    while (std::getline(statusFile, line)) {
        if (line.substr(0, 6) == "VmRSS:") {
            std::istringstream iss(line);
            std::string label;
            iss >> label >> ramKB;
            break;
        }
    }
    ramLabel.setText("RAM: " + juce::String(ramKB / 1024) + "MB", juce::dontSendNotification);
    #else
    int ramMB = juce::SystemStats::getMemorySizeInMegabytes();
    ramLabel.setText("RAM: " + juce::String(ramMB) + "MB", juce::dontSendNotification);
    #endif
    
    double cpuPercent = 0.0;
    if (SubterraneumAudioProcessor::standaloneDeviceManager != nullptr) {
        cpuPercent = SubterraneumAudioProcessor::standaloneDeviceManager->getCpuUsage() * 100.0;
    }
    cpuLabel.setText("CPU: " + juce::String(cpuPercent, 1) + "%", juce::dontSendNotification);
}

void SubterraneumAudioProcessorEditor::updateInstrumentSelector() { 
    instrumentSelector.updateList(); 
    resized();
}

// =============================================================================
// Show/Hide Plugin Browser Panel Based on Current Tab
// =============================================================================
void SubterraneumAudioProcessorEditor::updatePluginBrowserVisibility() {
    bool onRackTab = (tabs.getCurrentTabIndex() == 0);
    
    if (pluginBrowserPanel) {
        pluginBrowserPanel->setVisible(onRackTab);
        if (onRackTab) {
            pluginBrowserPanel->refresh();
        }
    }
}

// =============================================================================
// Keyboard Shortcuts
// =============================================================================
bool SubterraneumAudioProcessorEditor::keyPressed(const juce::KeyPress& key) {
    // Escape clears search in plugin browser (when on Rack tab)
    if (key == juce::KeyPress::escapeKey && tabs.getCurrentTabIndex() == 0) {
        if (pluginBrowserPanel) {
            pluginBrowserPanel->grabKeyboardFocus();
        }
        return true;
    }
    
    return false;
}

void SubterraneumAudioProcessorEditor::buttonClicked(juce::Button* b) { 
    // Handle left menu tab buttons
    if (b == &rackButton) {
        tabs.setCurrentTabIndex(0);
        updateTabButtonColors();
        updatePluginBrowserVisibility();
    } else if (b == &mixerButton) {
        tabs.setCurrentTabIndex(1);
        updateTabButtonColors();
        updatePluginBrowserVisibility();
    } else if (b == &studioButton) {
        tabs.setCurrentTabIndex(2);
        updateTabButtonColors();
        updatePluginBrowserVisibility();
    } else if (b == &settingsButton) {
        tabs.setCurrentTabIndex(3);
        updateTabButtonColors();
        updatePluginBrowserVisibility();
    } else if (b == &pluginsButton) {
        tabs.setCurrentTabIndex(4);
        updateTabButtonColors();
        updatePluginBrowserVisibility();
    } else if (b == &manualButton) {
        tabs.setCurrentTabIndex(5);
        updateTabButtonColors();
        updatePluginBrowserVisibility();
    } else if (b == &registerButton) {
        tabs.setCurrentTabIndex(6);
        updateTabButtonColors();
        updatePluginBrowserVisibility();
    } else if (b == &loadButton) { 
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
            if (file != juce::File()) { 
                if (!file.hasFileExtension(".subt")) 
                    file = file.withFileExtension(".subt");
                audioProcessor.saveUserPreset(file); 
            } 
        });
    } else if (b == &resetButton) { 
        juce::AlertWindow::showOkCancelBox(juce::MessageBoxIconType::QuestionIcon, "Reset Graph", "Reset the entire plugin graph?", 
            "Yes", "No", nullptr, juce::ModalCallbackFunction::create([this](int result) { 
                if (result == 1) { 
                    graphCanvas.closeAllPluginWindows();
                    audioProcessor.resetGraph(); 
                    updateInstrumentSelector(); 
                    repaint(); 
                } 
        }));
    } else if (b == &keysButton) { 
        if (keyboardWindow == nullptr) 
            keyboardWindow = std::make_unique<VirtualKeyboardWindow>(audioProcessor);
        keyboardWindow->setVisible(!keyboardWindow->isVisible());
    } 
}

void SubterraneumAudioProcessorEditor::paint(juce::Graphics& g) { 
    g.fillAll(Style::colBackground); 
    
    // Draw main header
    g.setColour(juce::Colour(0xff1a1a1a));
    g.fillRect(Style::leftMenuWidth, 0, getWidth() - Style::leftMenuWidth - Style::rightMenuWidth, Style::mainHeaderHeight); 
    
    // Draw logos
    int logoY = 5;
    int logoHeight = Style::mainHeaderHeight - 10;
    int logoX = Style::leftMenuWidth + 10;
    
    if (fananLogo.isValid()) {
        float aspectRatio = (float)fananLogo.getWidth() / (float)fananLogo.getHeight();
        int fananHeight = (int)(logoHeight * 0.6f);
        int fananWidth = (int)(fananHeight * aspectRatio);
        int fananY = logoY + (logoHeight - fananHeight) / 2;
        g.drawImage(fananLogo, logoX, fananY, fananWidth, fananHeight, 
                    0, 0, fananLogo.getWidth(), fananLogo.getHeight());
    }
    
    if (colosseumLogo.isValid()) {
        float aspectRatio = (float)colosseumLogo.getWidth() / (float)colosseumLogo.getHeight();
        int colosseumWidth = (int)(logoHeight * aspectRatio);
        int centerArea = getWidth() - Style::leftMenuWidth - Style::rightMenuWidth;
        int colosseumX = Style::leftMenuWidth + (centerArea - colosseumWidth) / 2;
        g.drawImage(colosseumLogo, colosseumX, logoY, colosseumWidth, logoHeight, 
                    0, 0, colosseumLogo.getWidth(), colosseumLogo.getHeight());
    }
    
    // Draw footer background
    g.setColour(juce::Colours::black.withAlpha(0.8f));
    g.fillRect(Style::leftMenuWidth, getHeight() - Style::instrHeaderHeight, 
               getWidth() - Style::leftMenuWidth - Style::rightMenuWidth, Style::instrHeaderHeight);
    
    // Draw left green menu
    g.setColour(Style::colLeftMenu);
    g.fillRect(0, 0, Style::leftMenuWidth, getHeight());
    
    // Draw right menu (yellow)
    g.setColour(juce::Colours::gold.darker(0.6f));
    g.fillRect(getWidth() - Style::rightMenuWidth, 0, Style::rightMenuWidth, getHeight());
    
    // Draw plugin browser panel background (only on Rack tab)
    if (tabs.getCurrentTabIndex() == 0) {
        g.setColour(juce::Colour(0xFF252525));
        g.fillRect(getWidth() - Style::rightMenuWidth - pluginBrowserWidth, 
                   Style::mainHeaderHeight, 
                   pluginBrowserWidth, 
                   getHeight() - Style::mainHeaderHeight - Style::instrHeaderHeight);
    }
}

void SubterraneumAudioProcessorEditor::resized() { 
    auto area = getLocalBounds(); 
    
    // Reserve left green menu
    auto leftMenu = area.removeFromLeft(Style::leftMenuWidth);
    
    // Right yellow menu
    auto rightMenu = area.removeFromRight(Style::rightMenuWidth); 
    rightMenu.removeFromTop(Style::mainHeaderHeight);
    
    int btnH = 40; 
    int gap = 10;
    loadButton.setBounds(rightMenu.removeFromTop(btnH).reduced(10, 5)); 
    rightMenu.removeFromTop(gap); 
    saveButton.setBounds(rightMenu.removeFromTop(btnH).reduced(10, 5)); 
    rightMenu.removeFromTop(gap); 
    resetButton.setBounds(rightMenu.removeFromTop(btnH).reduced(10, 5)); 
    rightMenu.removeFromTop(gap); 
    keysButton.setBounds(rightMenu.removeFromTop(btnH).reduced(10, 5));
    
    // Left menu buttons
    leftMenu.removeFromTop(Style::mainHeaderHeight);
    int leftBtnH = 50;
    int leftGap = 5;
    rackButton.setBounds(leftMenu.removeFromTop(leftBtnH).reduced(8, 4));
    leftMenu.removeFromTop(leftGap);
    mixerButton.setBounds(leftMenu.removeFromTop(leftBtnH).reduced(8, 4));
    leftMenu.removeFromTop(leftGap);
    studioButton.setBounds(leftMenu.removeFromTop(leftBtnH).reduced(8, 4));
    leftMenu.removeFromTop(leftGap);
    settingsButton.setBounds(leftMenu.removeFromTop(leftBtnH).reduced(8, 4));
    leftMenu.removeFromTop(leftGap);
    pluginsButton.setBounds(leftMenu.removeFromTop(leftBtnH).reduced(8, 4));
    leftMenu.removeFromTop(leftGap);
    manualButton.setBounds(leftMenu.removeFromTop(leftBtnH).reduced(8, 4));
    leftMenu.removeFromTop(leftGap);
    registerButton.setBounds(leftMenu.removeFromTop(leftBtnH).reduced(8, 4));
    
    auto mainHeader = area.removeFromTop(Style::mainHeaderHeight); 
    
    // Status LEDs in header
    int ledSize = 14;
    int ledSpacing = 40;
    int rightMargin = 10;
    int headerCenterY = Style::mainHeaderHeight / 2;
    int labelWidth = 30;
    int labelHeight = 14;
    
    int asioLedX = mainHeader.getRight() - rightMargin - ledSize;
    asioLed.setBounds(asioLedX, headerCenterY, ledSize, ledSize);
    asioLabel.setBounds(asioLedX - (labelWidth - ledSize) / 2, headerCenterY - labelHeight - 2, labelWidth, labelHeight);
    
    int regLedX = asioLedX - ledSpacing;
    registrationLED.setBounds(regLedX, headerCenterY, ledSize, ledSize);
    regLabel.setBounds(regLedX - (labelWidth - ledSize) / 2, headerCenterY - labelHeight - 2, labelWidth, labelHeight);
    
    int meterWidth = 85;
    int meterHeight = 20;
    int meterSpacing = 5;
    
    int ramX = regLedX - meterWidth - 15;
    ramLabel.setBounds(ramX, headerCenterY - meterHeight / 2, meterWidth, meterHeight);
    
    int cpuX = ramX - meterWidth - meterSpacing;
    cpuLabel.setBounds(cpuX, headerCenterY - meterHeight / 2, meterWidth, meterHeight);
    
    // Footer - instrument selector
    auto footer = area.removeFromBottom(Style::instrHeaderHeight);
    instrumentSelector.setBounds(footer);
    
    // =========================================================================
    // Plugin Browser Panel - Fixed 240px to left of yellow menu (Rack tab only)
    // =========================================================================
    bool onRackTab = (tabs.getCurrentTabIndex() == 0);
    
    if (onRackTab) {
        // Reserve space for plugin browser panel on the right side of the content area
        auto browserArea = area.removeFromRight(pluginBrowserWidth);
        if (pluginBrowserPanel) {
            pluginBrowserPanel->setBounds(browserArea);
        }
    }
    
    // Main tabs area (what's left after header/footer/menus/browser panel)
    tabs.setBounds(area);
    
    // Update visibility based on current tab
    updatePluginBrowserVisibility();
}

void SubterraneumAudioProcessorEditor::updateTabButtonColors() {
    int currentTab = tabs.getCurrentTabIndex();
    
    juce::TextButton* tabButtons[] = { &rackButton, &mixerButton, &studioButton, 
                                       &settingsButton, &pluginsButton, &manualButton, &registerButton };
    
    for (int i = 0; i < 7; ++i) {
        if (i == currentTab) {
            tabButtons[i]->setColour(juce::TextButton::buttonColourId, juce::Colour(0xffC0C0C0));
            tabButtons[i]->setColour(juce::TextButton::textColourOffId, juce::Colours::black);
            tabButtons[i]->setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffC0C0C0));
            tabButtons[i]->setLookAndFeel(nullptr);
        } else {
            tabButtons[i]->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a2a2a));
            tabButtons[i]->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
            tabButtons[i]->setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff2a2a2a));
        }
    }
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