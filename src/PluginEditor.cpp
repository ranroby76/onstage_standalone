

// FIX: Plugin Browser Panel is now a fixed 240px panel on the right side of content
// No toggle button - always visible on Rack tab, hidden on other tabs
// FIX: Removed Studio tab - tempo/metronome moved to AudioSettingsTab
// FIX: Added Recorder to onToolDropped handler
// FIX: Removed yellow right panel - utility buttons relocated to left green panel
// FIX: Added MIDI Panic button under Keys button

#include "PluginEditor.h"
#include "SimpleConnectorProcessor.h"
#include "StereoMeterProcessor.h"
#include "MidiMonitorProcessor.h"
#include "RecorderProcessor.h"
#include "ManualSamplerProcessor.h"
#include "AutoSamplerProcessor.h"
#include "MidiPlayerProcessor.h"
#include "CCStepperProcessor.h"
#include "TransientSplitterProcessor.h"

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
    
    // NEW: MIDI Panic button - red color to stand out
    panicButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkred);
    panicButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    panicButton.setTooltip("Send All Notes Off to all instruments (stops stuck notes)");
    addAndMakeVisible(panicButton);
    panicButton.addListener(this);
    
    // Workspace selector buttons
    for (int i = 0; i < 16; ++i)
    {
        workspaceButtons[i].setButtonText(audioProcessor.getWorkspaceName(i));
        workspaceButtons[i].setColour(juce::TextButton::buttonColourId, juce::Colour(40, 40, 45));
        workspaceButtons[i].setColour(juce::TextButton::textColourOffId, juce::Colour(190, 190, 200));
        workspaceButtons[i].onClick = [this, i]() {
            if (!audioProcessor.isWorkspaceEnabled(i)) return;
            graphCanvas.closeAllPluginWindows();
            audioProcessor.switchWorkspace(i);
            graphCanvas.refreshCache();
            graphCanvas.repaint();
            // Sync zoom to restored workspace value
            zoomSlider.setValue((double)audioProcessor.rackZoomLevel, juce::sendNotificationSync);
            updateWorkspaceButtonColors();
        };
        workspaceButtons[i].onStateChange = [this, i]() {
            // Right-click detection via mouse event
        };
        addAndMakeVisible(workspaceButtons[i]);
        workspaceButtons[i].addMouseListener(this, false);
    }
    
    // Workspace label
    workspacesLabel.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
    workspacesLabel.setJustificationType(juce::Justification::centredLeft);
    workspacesLabel.setColour(juce::Label::textColourId, juce::Colour(200, 200, 220));
    addAndMakeVisible(workspacesLabel);
    
    updateWorkspaceButtonColors();
    
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
    zoomSlider.setRange(0.25, 1.0, 0.75 / 75.0);  // 76 positions (25% to 100%), 75 steps
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
    
    // Left green menu tab buttons - FIX: Only 6 buttons now (removed Studio)
    addAndMakeVisible(rackButton); rackButton.addListener(this);
    addAndMakeVisible(mixerButton); mixerButton.addListener(this);
    addAndMakeVisible(settingsButton); settingsButton.addListener(this);
    addAndMakeVisible(pluginsButton); pluginsButton.addListener(this);
    addAndMakeVisible(manualButton); manualButton.addListener(this);
    addAndMakeVisible(registerButton); registerButton.addListener(this);
    
    addAndMakeVisible(instrumentSelector); 
    instrumentSelector.updateList(); 
    
    // Setup tabs - FIX: Removed Studio tab (now only 6 tabs)
    tabs.setOutline(0);
    tabs.setTabBarDepth(0);
    tabs.setColour(juce::TabbedComponent::outlineColourId, juce::Colours::transparentBlack);
    tabs.addTab("Rack", Style::colBackground, &graphCanvas, false); 
    tabs.addTab("Mixer", Style::colBackground, &mixerView, false);
    tabs.addTab("Settings", Style::colBackground, &audioSettingsTab, false);  // Now includes tempo/metronome
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
            case SystemToolType::MidiMonitor:
                nodePtr = audioProcessor.mainGraph->addNode(std::unique_ptr<juce::AudioProcessor>(new MidiMonitorProcessor()));
                break;
            case SystemToolType::Recorder:
                nodePtr = audioProcessor.mainGraph->addNode(std::unique_ptr<juce::AudioProcessor>(new RecorderProcessor()));
                break;
            case SystemToolType::ManualSampler:
                nodePtr = audioProcessor.mainGraph->addNode(std::unique_ptr<juce::AudioProcessor>(new ManualSamplerProcessor()));
                break;
            case SystemToolType::AutoSampler:
                nodePtr = audioProcessor.mainGraph->addNode(std::unique_ptr<juce::AudioProcessor>(new AutoSamplerProcessor(audioProcessor.mainGraph.get(), &audioProcessor)));
                break;
            case SystemToolType::MidiPlayer:
                nodePtr = audioProcessor.mainGraph->addNode(std::unique_ptr<juce::AudioProcessor>(new MidiPlayerProcessor()));
                break;
            case SystemToolType::StepSeq:
                nodePtr = audioProcessor.mainGraph->addNode(std::unique_ptr<juce::AudioProcessor>(new CCStepperProcessor()));
                break;
            case SystemToolType::TransientSplitter:
                nodePtr = audioProcessor.mainGraph->addNode(std::unique_ptr<juce::AudioProcessor>(new TransientSplitterProcessor()));
                break;
            case SystemToolType::VST2Plugin:
                // VST2 opens file chooser via GraphCanvas
                graphCanvas.loadVST2Plugin(juce::Point<float>((float)pos.x, (float)pos.y));
                return;  // No node created directly
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

void SubterraneumAudioProcessorEditor::updateInstrumentSelector() { 
    instrumentSelector.updateList(); 
    resized();
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
    // FIX: Handle left menu tab buttons - now only 6 buttons (removed Studio)
    if (b == &rackButton) {
        tabs.setCurrentTabIndex(0);
        updateTabButtonColors();
        updatePluginBrowserVisibility();
        resized();
    } else if (b == &mixerButton) {
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
        fileChooser = std::make_unique<juce::FileChooser>("Load Patch", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.subt");
        fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this](const juce::FileChooser& fc) { 
            auto file = fc.getResult(); 
            if (file != juce::File()) { 
                graphCanvas.closeAllPluginWindows();
                audioProcessor.loadUserPreset(file); 
                graphCanvas.refreshCache();
                // Sync zoom to loaded preset value
                zoomSlider.setValue((double)audioProcessor.rackZoomLevel, juce::sendNotificationSync);
                repaint(); 
                updateInstrumentSelector(); 
                updateWorkspaceButtonColors();
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
        juce::AlertWindow::showOkCancelBox(juce::MessageBoxIconType::QuestionIcon, "Reset", "Reset the entire application to initial state?\nAll workspaces will be cleared.", 
            "Yes", "No", nullptr, juce::ModalCallbackFunction::create([this](int result) { 
                if (result == 1) { 
                    graphCanvas.closeAllPluginWindows();
                    audioProcessor.resetAllWorkspaces(); 
                    graphCanvas.refreshCache();
                    // Reset zoom to default
                    audioProcessor.rackZoomLevel = 1.0f;
                    zoomSlider.setValue(1.0, juce::sendNotificationSync);
                    updateInstrumentSelector(); 
                    updateWorkspaceButtonColors();
                    repaint(); 
                } 
        }));
    } else if (b == &keysButton) { 
        if (keyboardWindow == nullptr) 
            keyboardWindow = std::make_unique<VirtualKeyboardWindow>(audioProcessor);
        keyboardWindow->setVisible(!keyboardWindow->isVisible());
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
    
    // Workspace bar background
    g.setColour(juce::Colour(25, 25, 30));
    g.fillRect(Style::leftMenuWidth, Style::mainHeaderHeight, 
               getWidth() - Style::leftMenuWidth, workspaceBarHeight);
    
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
        int centerArea = getWidth() - Style::leftMenuWidth;
        int colosseumX = Style::leftMenuWidth + (centerArea - colosseumWidth) / 2;
        g.drawImage(colosseumLogo, colosseumX, logoY, colosseumWidth, logoHeight, 
                    0, 0, colosseumLogo.getWidth(), colosseumLogo.getHeight());
    }
    
    // Draw footer background
    g.setColour(juce::Colours::black.withAlpha(0.8f));
    g.fillRect(Style::leftMenuWidth, getHeight() - Style::instrHeaderHeight, 
               getWidth() - Style::leftMenuWidth, Style::instrHeaderHeight);
    
    // Draw left green menu
    g.setColour(Style::colLeftMenu);
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
                   getHeight() - Style::mainHeaderHeight - Style::instrHeaderHeight);
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
    mixerButton.setBounds(leftMenu.removeFromTop(leftBtnH).reduced(8, 4));
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
    keysButton.setBounds(leftMenu.removeFromTop(utilBtnH).reduced(8, 3));
    leftMenu.removeFromTop(utilGap);
    panicButton.setBounds(leftMenu.removeFromTop(utilBtnH).reduced(8, 3));
    
    auto mainHeader = area.removeFromTop(Style::mainHeaderHeight); 
    
    // Workspace selector bar
    auto wsBar = area.removeFromTop(workspaceBarHeight);
    {
        int labelW = 80;
        int gap = 2;
        int availW = wsBar.getWidth() - labelW - gap * 2;
        int btnW = (availW - 15 * gap) / 16;
        if (btnW < 40) btnW = 40;
        int startX = wsBar.getX();
        workspacesLabel.setBounds(startX, wsBar.getY(), labelW, workspaceBarHeight);
        int btnStartX = startX + labelW + gap * 2;
        for (int i = 0; i < 16; ++i)
        {
            workspaceButtons[i].setBounds(btnStartX + i * (btnW + gap), 
                                           wsBar.getY() + 2, btnW, workspaceBarHeight - 4);
        }
    }
    
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
    
    // Footer - instrument selector
    auto footer = area.removeFromBottom(Style::instrHeaderHeight);
    instrumentSelector.setBounds(footer);
    
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
    
    // Main tabs area (what's left after header/footer/menus/browser panel)
    tabs.setBounds(area);
    
    // Update visibility based on current tab
    updatePluginBrowserVisibility();
}

void SubterraneumAudioProcessorEditor::updateTabButtonColors() {
    int currentTab = tabs.getCurrentTabIndex();
    
    // FIX: Only 6 buttons now (removed studioButton)
    juce::TextButton* tabButtons[] = { &rackButton, &mixerButton, &settingsButton, 
                                       &pluginsButton, &manualButton, &registerButton };
    
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

void SubterraneumAudioProcessorEditor::updateWorkspaceButtonColors()
{
    int active = audioProcessor.getActiveWorkspace();
    for (int i = 0; i < 16; ++i)
    {
        bool isActive = (i == active);
        bool isEnabled = audioProcessor.isWorkspaceEnabled(i);
        bool isOccupied = audioProcessor.isWorkspaceOccupied(i);
        
        workspaceButtons[i].setButtonText(audioProcessor.getWorkspaceName(i));
        
        if (!isEnabled) {
            // Disabled — dark, dimmer than others but still readable
            workspaceButtons[i].setColour(juce::TextButton::buttonColourId, juce::Colour(28, 28, 30));
            workspaceButtons[i].setColour(juce::TextButton::textColourOffId, juce::Colour(90, 90, 100));
        } else if (isActive) {
            workspaceButtons[i].setColour(juce::TextButton::buttonColourId, juce::Colour(0, 120, 200));
            workspaceButtons[i].setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        } else if (isOccupied) {
            workspaceButtons[i].setColour(juce::TextButton::buttonColourId, juce::Colour(60, 60, 70));
            workspaceButtons[i].setColour(juce::TextButton::textColourOffId, juce::Colour(220, 220, 230));
        } else {
            // Enabled but empty
            workspaceButtons[i].setColour(juce::TextButton::buttonColourId, juce::Colour(40, 40, 45));
            workspaceButtons[i].setColour(juce::TextButton::textColourOffId, juce::Colour(190, 190, 200));
        }
    }
}

// =============================================================================
// Workspace Context Menu (right-click)
// =============================================================================
void SubterraneumAudioProcessorEditor::showWorkspaceContextMenu(int idx)
{
    juce::PopupMenu menu;
    
    bool isEnabled = audioProcessor.isWorkspaceEnabled(idx);
    bool isActive = (idx == audioProcessor.getActiveWorkspace());
    
    // Enable / Disable toggle
    if (!isEnabled)
        menu.addItem(1, "Enable");
    else if (!isActive)
        menu.addItem(1, "Disable");
    else
        menu.addItem(1, "Disable", false); // Can't disable active workspace
    
    menu.addItem(2, "Rename");
    
    // Clear — only if enabled
    menu.addItem(3, "Clear", isEnabled);
    
    // Duplicate submenu — only if enabled
    juce::PopupMenu dupMenu;
    for (int i = 0; i < 16; ++i)
    {
        if (i == idx) continue;
        dupMenu.addItem(100 + i, "Workspace " + audioProcessor.getWorkspaceName(i));
    }
    menu.addSubMenu("Duplicate to...", dupMenu, isEnabled);
    
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&workspaceButtons[idx]),
        [this, idx](int result)
        {
            if (result == 0) return;
            
            if (result == 1)
            {
                // Toggle enable
                bool wasEnabled = audioProcessor.isWorkspaceEnabled(idx);
                audioProcessor.setWorkspaceEnabled(idx, !wasEnabled);
                updateWorkspaceButtonColors();
            }
            else if (result == 2)
            {
                // Rename — popup text editor
                auto* editor = new juce::TextEditor();
                editor->setSize(200, 26);
                editor->setText(audioProcessor.getWorkspaceName(idx));
                editor->selectAll();
                editor->setColour(juce::TextEditor::backgroundColourId, juce::Colour(45, 45, 50));
                editor->setColour(juce::TextEditor::textColourId, juce::Colours::white);
                editor->setColour(juce::TextEditor::outlineColourId, juce::Colour(80, 80, 90));
                
                auto* okBtn = new juce::TextButton("OK");
                okBtn->setSize(50, 26);
                okBtn->setColour(juce::TextButton::buttonColourId, juce::Colour(0, 120, 200));
                
                auto* container = new juce::Component();
                container->setSize(260, 30);
                container->addAndMakeVisible(editor);
                container->addAndMakeVisible(okBtn);
                editor->setBounds(0, 2, 200, 26);
                okBtn->setBounds(206, 2, 50, 26);
                
                auto screenBounds = workspaceButtons[idx].getScreenBounds();
                
                auto* editorPtr = editor;
                auto& procRef = audioProcessor;
                auto updateFn = [this]() { updateWorkspaceButtonColors(); };
                int wsIdx = idx;
                
                auto doRename = [editorPtr, &procRef, wsIdx, updateFn]() {
                    auto newName = editorPtr->getText().trim();
                    if (newName.isNotEmpty())
                        procRef.setWorkspaceName(wsIdx, newName.substring(0, 15));
                    updateFn();
                    if (auto* callout = editorPtr->findParentComponentOfClass<juce::CallOutBox>())
                        callout->dismiss();
                };
                
                okBtn->onClick = doRename;
                editor->onReturnKey = doRename;
                editor->onEscapeKey = [editorPtr]() {
                    if (auto* callout = editorPtr->findParentComponentOfClass<juce::CallOutBox>())
                        callout->dismiss();
                };
                
                juce::CallOutBox::launchAsynchronously(
                    std::unique_ptr<juce::Component>(container),
                    screenBounds, nullptr);
            }
            else if (result == 3)
            {
                // Clear
                graphCanvas.closeAllPluginWindows();
                audioProcessor.clearWorkspace(idx);
                if (idx == audioProcessor.getActiveWorkspace())
                {
                    graphCanvas.refreshCache();
                    graphCanvas.repaint();
                }
                updateWorkspaceButtonColors();
            }
            else if (result >= 100 && result < 116)
            {
                // Duplicate to workspace (result - 100)
                int dstIdx = result - 100;
                graphCanvas.closeAllPluginWindows();
                audioProcessor.duplicateWorkspace(idx, dstIdx);
                if (dstIdx == audioProcessor.getActiveWorkspace())
                {
                    graphCanvas.refreshCache();
                    graphCanvas.repaint();
                }
                updateWorkspaceButtonColors();
            }
        });
}

void SubterraneumAudioProcessorEditor::mouseDown(const juce::MouseEvent& e)
{
    if (e.mods.isRightButtonDown())
    {
        for (int i = 0; i < 16; ++i)
        {
            if (e.eventComponent == &workspaceButtons[i])
            {
                showWorkspaceContextMenu(i);
                return;
            }
        }
    }
    juce::AudioProcessorEditor::mouseDown(e);
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




