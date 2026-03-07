// FIX: Plugin Browser Panel is now a fixed 240px panel on the right side of content
// FIX: Removed Studio tab, Instrument Selector, Virtual Keyboard
// FIX: Added Media tab
// FIX: Removed Mixer tab - gain controls moved to Connector/Amp nodes
// Remaining system tools in onToolDropped: Connector, StereoMeter, Recorder, TransientSplitter, VST2/VST3

#include "PluginEditor.h"
#include "SimpleConnectorProcessor.h"
#include "StereoMeterProcessor.h"
#include "RecorderProcessor.h"
#include "TransientSplitterProcessor.h"
#include "OnStageDialog.h"

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
      audioSettingsTab(p), 
      pluginManagerTab(p),
      manualTab(p),
      registrationTab(p),
      mediaPage(p)
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
        fananLogoFile = juce::File("D:/Workspace/onstage_colosseum_upgrade/assets/fanan logo.png");
    }
    
    juce::File colosseumLogoFile = assetsDir.getChildFile("colosseum_logo.png");
    if (!colosseumLogoFile.existsAsFile()) {
        colosseumLogoFile = deployDir.getChildFile("colosseum_logo.png");
    }
    if (!colosseumLogoFile.existsAsFile()) {
        colosseumLogoFile = juce::File("D:/Workspace/onstage_colosseum_upgrade/assets/colosseum_logo.png");
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
    
    // MIDI Panic button - red color to stand out
    panicButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkred);
    panicButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    panicButton.setTooltip("Send All Notes Off to all instruments (stops stuck notes)");
    addAndMakeVisible(panicButton);
    panicButton.addListener(this);
    
    // ASIO LED and label
    addAndMakeVisible(asioLed);
    asioLabel.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
    asioLabel.setJustificationType(juce::Justification::centred);
    asioLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(asioLabel);
    
    // Registration LED and label
    addAndMakeVisible(registrationLED);
    regLabel.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
    regLabel.setJustificationType(juce::Justification::centred);
    regLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(regLabel);
    
    // CPU and RAM labels
    cpuLabel.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    cpuLabel.setJustificationType(juce::Justification::centredRight);
    cpuLabel.setColour(juce::Label::textColourId, juce::Colours::cyan);
    addAndMakeVisible(cpuLabel);
    
    ramLabel.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    ramLabel.setJustificationType(juce::Justification::centredRight);
    ramLabel.setColour(juce::Label::textColourId, juce::Colours::orange);
    addAndMakeVisible(ramLabel);
    
    // Zoom slider for Rack tab
    zoomSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    zoomSlider.setRange(0.10, 2.0, 0.01);  // 10% to 200%, 1% steps
    zoomSlider.setValue(1.0, juce::dontSendNotification);
    zoomSlider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    zoomSlider.setColour(juce::Slider::trackColourId, juce::Colour(80, 80, 90));
    zoomSlider.setColour(juce::Slider::thumbColourId, juce::Colour(0xffFFD700));  // Start yellow at 100%
    zoomSlider.setColour(juce::Slider::backgroundColourId, juce::Colour(40, 40, 48));
    
    auto updateZoomDisplay = [this]() {
        float zoom = (float)zoomSlider.getValue();
        graphCanvas.setZoomLevel(zoom);
        audioProcessor.rackZoomLevel = zoom;
        
        // Update percentage label
        int pct = juce::roundToInt(zoom * 100.0f);
        zoomLabel.setText(juce::String(pct) + "%", juce::dontSendNotification);
        
        // Yellow thumb at 100%, white otherwise
        bool atMiddle = std::abs(zoom - 1.0f) < 0.01f;
        zoomSlider.setColour(juce::Slider::thumbColourId, 
            atMiddle ? juce::Colour(0xffFFD700) : juce::Colours::white);
    };
    
    zoomSlider.onValueChange = updateZoomDisplay;
    zoomSlider.setDoubleClickReturnValue(true, 1.0);
    zoomSlider.setValue((double)audioProcessor.rackZoomLevel, juce::dontSendNotification);
    graphCanvas.setZoomLevel(audioProcessor.rackZoomLevel);
    // Init display
    {
        int pct = juce::roundToInt(audioProcessor.rackZoomLevel * 100.0f);
        zoomLabel.setText(juce::String(pct) + "%", juce::dontSendNotification);
        bool atMiddle = std::abs(audioProcessor.rackZoomLevel - 1.0f) < 0.01f;
        zoomSlider.setColour(juce::Slider::thumbColourId, 
            atMiddle ? juce::Colour(0xffFFD700) : juce::Colours::white);
    }
    addAndMakeVisible(zoomSlider);
    
    zoomLabel.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
    zoomLabel.setJustificationType(juce::Justification::centred);
    zoomLabel.setColour(juce::Label::textColourId, juce::Colour(160, 160, 180));
    addAndMakeVisible(zoomLabel);
    
    // Left green menu tab buttons - FIX: 6 buttons now (removed Mixer)
    addAndMakeVisible(rackButton); rackButton.addListener(this);
    addAndMakeVisible(mediaButton); mediaButton.addListener(this);
    addAndMakeVisible(settingsButton); settingsButton.addListener(this);
    addAndMakeVisible(pluginsButton); pluginsButton.addListener(this);
    addAndMakeVisible(manualButton); manualButton.addListener(this);
    addAndMakeVisible(registerButton); registerButton.addListener(this);
    
    // Setup tabs - FIX: 6 tabs now (removed Mixer)
    tabs.setOutline(0);
    tabs.setTabBarDepth(0);
    tabs.setColour(juce::TabbedComponent::outlineColourId, juce::Colours::transparentBlack);
    
    tabs.addTab("Rack", Style::colBackground, &graphCanvas, false); 
    tabs.addTab("Media", Style::colBackground, &mediaPage, false);
    tabs.addTab("Settings", Style::colBackground, &audioSettingsTab, false);
    tabs.addTab("Plugins", Style::colBackground, &pluginManagerTab, false);
    tabs.addTab("Manual", Style::colBackground, &manualTab, false);
    tabs.addTab("Register", Style::colBackground, &registrationTab, false);
    addAndMakeVisible(tabs);
    
    tabs.setCurrentTabIndex(0, false);
    updateTabButtonColors();
    
    // =========================================================================
    // Plugin Browser Panel - Fixed 240px on right side of content area
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
            case SystemToolType::Recorder:
                nodePtr = audioProcessor.mainGraph->addNode(std::unique_ptr<juce::AudioProcessor>(new RecorderProcessor()));
                break;
            case SystemToolType::TransientSplitter:
                nodePtr = audioProcessor.mainGraph->addNode(std::unique_ptr<juce::AudioProcessor>(new TransientSplitterProcessor()));
                break;
            case SystemToolType::VST2Plugin:
                // VST2 opens file chooser via GraphCanvas
                graphCanvas.loadVST2Plugin(juce::Point<float>((float)pos.x, (float)pos.y));
                return;  // No node created directly
            case SystemToolType::VST3Plugin:
                // VST3 opens file chooser via GraphCanvas
                graphCanvas.loadVST3Plugin(juce::Point<float>((float)pos.x, (float)pos.y));
                return;
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
    
    // Set fullscreen on startup, then refresh plugin browser after layout is complete
    juce::MessageManager::callAsync([this]() {
        if (auto* peer = getPeer()) {
            peer->setFullScreen(true);
        }
        // Deferred refresh: plugin browser needs valid bounds before it can populate
        juce::MessageManager::callAsync([this]() {
            if (pluginBrowserPanel)
                pluginBrowserPanel->refresh();
            
            // Startup quick scan — checks for new/updated plugins
            pluginManagerTab.runStartupQuickScan();
        });
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

bool SubterraneumAudioProcessorEditor::keyPressed(const juce::KeyPress& /*key*/) {
    // Keyboard shortcuts can be added here
    return false;
}

void SubterraneumAudioProcessorEditor::updatePluginBrowserVisibility() {
    bool onRackTab = (tabs.getCurrentTabIndex() == 0);
    if (pluginBrowserPanel) {
        // Only visible on Rack tab (index 0)
        pluginBrowserPanel->setVisible(onRackTab);
    }
    // Zoom slider only relevant for Rack tab
    zoomSlider.setVisible(onRackTab);
    zoomLabel.setVisible(onRackTab);
}

// =============================================================================
// NEW: MIDI Panic - Send All Notes Off to all instruments
// =============================================================================
void SubterraneumAudioProcessorEditor::sendMidiPanic() {
    if (!audioProcessor.mainGraph) return;
    
    // Iterate through all nodes in the graph
    for (auto* node : audioProcessor.mainGraph->getNodes()) {
        if (auto* meteringProc = dynamic_cast<MeteringProcessor*>(node->getProcessor())) {
            // Send panic to the inner plugin
            meteringProc->sendAllNotesOffToPlugin();
        }
    }
    
    // Also clear the keyboard state
    audioProcessor.keyboardState.allNotesOff(0);
    
    // Flash the button to provide visual feedback
    panicButton.setColour(juce::TextButton::buttonColourId, juce::Colours::red);
    juce::Timer::callAfterDelay(200, [this]() {
        panicButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkred);
    });
}

void SubterraneumAudioProcessorEditor::buttonClicked(juce::Button* b) { 
    // FIX: Handle left menu tab buttons - now 6 buttons (removed Mixer)
    if (b == &rackButton) {
        tabs.setCurrentTabIndex(0);
        updateTabButtonColors();
        updatePluginBrowserVisibility();
        resized();
    } else if (b == &mediaButton) {
        tabs.setCurrentTabIndex(1);
        updateTabButtonColors();
        updatePluginBrowserVisibility();
        resized();
    } else if (b == &settingsButton) {
        tabs.setCurrentTabIndex(2);
        updateTabButtonColors();
        updatePluginBrowserVisibility();
        resized();
    } else if (b == &pluginsButton) {
        tabs.setCurrentTabIndex(3);
        updateTabButtonColors();
        updatePluginBrowserVisibility();
        resized();
    } else if (b == &manualButton) {
        tabs.setCurrentTabIndex(4);
        updateTabButtonColors();
        updatePluginBrowserVisibility();
        resized();
    } else if (b == &registerButton) {
        tabs.setCurrentTabIndex(5);
        updateTabButtonColors();
        updatePluginBrowserVisibility();
        resized();
    } else if (b == &loadButton) { 
        fileChooser = std::make_unique<juce::FileChooser>("Load Patch", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.ons");
        fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this](const juce::FileChooser& fc) { 
            auto file = fc.getResult(); 
            if (file != juce::File()) { 
                graphCanvas.closeAllPluginWindows();
                audioProcessor.loadUserPreset(file); 
                graphCanvas.refreshCache();
                // Sync zoom to loaded preset value
                zoomSlider.setValue((double)audioProcessor.rackZoomLevel, juce::sendNotificationSync);
                repaint(); 
            } 
        });
    } else if (b == &saveButton) { 
        fileChooser = std::make_unique<juce::FileChooser>("Save Patch", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.ons");
        fileChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles, [this](const juce::FileChooser& fc) { 
            auto file = fc.getResult(); 
            if (file != juce::File()) { 
                if (!file.hasFileExtension(".ons")) 
                    file = file.withFileExtension(".ons");
                audioProcessor.saveUserPreset(file); 
            } 
        });
    } else if (b == &resetButton) { 
        OnStageDialog::showOkCancel(
            "Reset",
            "Reset the entire application to initial state?",
            "Yes", "No",
            this,
            [this](bool confirmed) {
                if (confirmed) { 
                    graphCanvas.closeAllPluginWindows();
                    audioProcessor.resetGraph(); 
                    graphCanvas.refreshCache();
                    // Reset zoom to default
                    audioProcessor.rackZoomLevel = 1.0f;
                    zoomSlider.setValue(1.0, juce::sendNotificationSync);
                    repaint(); 
                } 
            });
    } else if (b == &panicButton) {
        // NEW: Handle panic button
        sendMidiPanic();
    }
}

void SubterraneumAudioProcessorEditor::paint(juce::Graphics& g) { 
    g.fillAll(Style::colBackground); 
    
    // Draw main header
    g.setColour(juce::Colour(0xff1a1a1a));
    g.fillRect(Style::leftMenuWidth, 0, getWidth() - Style::leftMenuWidth, Style::mainHeaderHeight); 
    
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
        // Original size: 1128x273, aspect ratio ~4.13
        // Scale to fit header height while maintaining aspect ratio
        float srcAspect = (float)colosseumLogo.getWidth() / (float)colosseumLogo.getHeight();
        int colosseumHeight = logoHeight;
        int colosseumWidth = (int)(colosseumHeight * srcAspect);
        
        // Center in header area
        int centerArea = getWidth() - Style::leftMenuWidth;
        int colosseumX = Style::leftMenuWidth + (centerArea - colosseumWidth) / 2;
        int colosseumY = logoY + (logoHeight - colosseumHeight) / 2;
        
        // Draw with proper source and destination rectangles
        g.drawImage(colosseumLogo, 
                    colosseumX, colosseumY, colosseumWidth, colosseumHeight,
                    0, 0, colosseumLogo.getWidth(), colosseumLogo.getHeight(),
                    false);  // Don't use filtering that might distort
    }
    
    // Draw left gold gradient menu
    juce::ColourGradient goldGradient(
        Style::colLeftMenuTop, 0.0f, 0.0f,
        Style::colLeftMenuBottom, 0.0f, (float)getHeight(),
        false);
    g.setGradientFill(goldGradient);
    g.fillRect(0, 0, Style::leftMenuWidth, getHeight());
    
    // Draw white separator line between tab buttons and utility buttons
    {
        // 6 tab buttons (50h + 5gap), last has no gap: 6*50 + 5*5 = 325
        int separatorY = Style::mainHeaderHeight + 6 * 50 + 5 * 5 + 5;  // +5 = center of 10px gap
        g.setColour(juce::Colours::white.withAlpha(0.6f));
        g.drawLine(8.0f, (float)separatorY, (float)(Style::leftMenuWidth - 8), (float)separatorY, 1.5f);
    }
    
    // Draw plugin browser panel background (only on Rack tab)
    if (tabs.getCurrentTabIndex() == 0) {
        g.setColour(juce::Colour(0xFF252525));
        g.fillRect(getWidth() - pluginBrowserWidth, 
                   Style::mainHeaderHeight, 
                   pluginBrowserWidth, 
                   getHeight() - Style::mainHeaderHeight);
    }
}

void SubterraneumAudioProcessorEditor::resized() { 
    auto area = getLocalBounds(); 
    
    // Reserve left green menu
    auto leftMenu = area.removeFromLeft(Style::leftMenuWidth);
    
    // Left menu buttons - tab buttons
    leftMenu.removeFromTop(Style::mainHeaderHeight);
    int leftBtnH = 50;
    int leftGap = 5;
    rackButton.setBounds(leftMenu.removeFromTop(leftBtnH).reduced(8, 4));
    leftMenu.removeFromTop(leftGap);
    mediaButton.setBounds(leftMenu.removeFromTop(leftBtnH).reduced(8, 4));
    leftMenu.removeFromTop(leftGap);
    settingsButton.setBounds(leftMenu.removeFromTop(leftBtnH).reduced(8, 4));
    leftMenu.removeFromTop(leftGap);
    pluginsButton.setBounds(leftMenu.removeFromTop(leftBtnH).reduced(8, 4));
    leftMenu.removeFromTop(leftGap);
    manualButton.setBounds(leftMenu.removeFromTop(leftBtnH).reduced(8, 4));
    leftMenu.removeFromTop(leftGap);
    registerButton.setBounds(leftMenu.removeFromTop(leftBtnH).reduced(8, 4));
    
    // Separator gap (white line drawn in paint)
    leftMenu.removeFromTop(10);
    
    // Utility buttons (relocated from right panel)
    int utilBtnH = 36;
    int utilGap = 4;
    loadButton.setBounds(leftMenu.removeFromTop(utilBtnH).reduced(8, 3));
    leftMenu.removeFromTop(utilGap);
    saveButton.setBounds(leftMenu.removeFromTop(utilBtnH).reduced(8, 3));
    leftMenu.removeFromTop(utilGap);
    resetButton.setBounds(leftMenu.removeFromTop(utilBtnH).reduced(8, 3));
    leftMenu.removeFromTop(utilGap);
    panicButton.setBounds(leftMenu.removeFromTop(utilBtnH).reduced(8, 3));
    
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
    
    // Zoom slider - left of CPU label
    int zoomSliderWidth = 90;
    int zoomSliderHeight = 16;
    int zoomX = cpuX - zoomSliderWidth - 10;
    zoomSlider.setBounds(zoomX, headerCenterY - zoomSliderHeight / 2 + 4, zoomSliderWidth, zoomSliderHeight);
    zoomLabel.setBounds(zoomX, headerCenterY - labelHeight - 2, zoomSliderWidth, labelHeight);
    
    // =========================================================================
    // Plugin Browser Panel - Fixed 240px on right side of content (Rack tab only)
    // =========================================================================
    bool onRackTab = (tabs.getCurrentTabIndex() == 0);
    
    if (onRackTab) {
        // Reserve space for plugin browser panel on the right side of the content area
        auto browserArea = area.removeFromRight(pluginBrowserWidth);
        if (pluginBrowserPanel) {
            pluginBrowserPanel->setBounds(browserArea);
        }
    }
    
    // Main tabs area (what's left after header/menus/browser panel)
    tabs.setBounds(area);
    
    // Update visibility based on current tab
    updatePluginBrowserVisibility();
    
    // =========================================================================
    // FIX: Keep browser panel on top of zoomed canvas
    // setTransform(scale) on GraphCanvas can overflow its bounds
    // =========================================================================
    if (pluginBrowserPanel)
        pluginBrowserPanel->toFront(false);
}

void SubterraneumAudioProcessorEditor::updateTabButtonColors() {
    int currentTab = tabs.getCurrentTabIndex();
    
    // FIX: 6 buttons now (removed mixerButton)
    juce::TextButton* tabButtons[] = { &rackButton, &mediaButton,
                                       &settingsButton, &pluginsButton, &manualButton, &registerButton };
    
    for (int i = 0; i < 6; ++i) {
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

void SubterraneumAudioProcessorEditor::mouseDown(const juce::MouseEvent& e)
{
    juce::AudioProcessorEditor::mouseDown(e);
}

