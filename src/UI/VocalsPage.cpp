#include "VocalsPage.h"
#include "../RegistrationManager.h"

using namespace juce;

VocalsPage::VocalsPage(AudioEngine& engineRef, PresetManager& presetMgr)
    : audioEngine(engineRef), presetManager(presetMgr)
{
    goldenLookAndFeel = std::make_unique<GoldenSliderLookAndFeel>();
    setupTabbedComponent();
    
    updateAllControlsFromEngine();
    
    startTimer(200); 
}

VocalsPage::~VocalsPage()
{
    stopTimer();
    if (tabbedComponent) tabbedComponent->setLookAndFeel(nullptr);
    eqPanel1 = nullptr; eqPanel2 = nullptr;
    compPanel1 = nullptr; compPanel2 = nullptr;
    excPanel1 = nullptr; excPanel2 = nullptr;
    sculptPanel1 = nullptr; sculptPanel2 = nullptr;
    harmonizerPanel = nullptr; reverbPanel = nullptr;
    delayPanel = nullptr; dynEqPanel = nullptr;
}

void VocalsPage::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF202020));
}

void VocalsPage::resized()
{
    auto area = getLocalBounds().reduced(20);
    
    // Tabs take full area
    if (tabbedComponent)
        tabbedComponent->setBounds(area);
}

void VocalsPage::setupTabbedComponent()
{
    tabbedComponent = std::make_unique<TabbedComponent>(TabbedButtonBar::TabsAtTop);
    tabbedComponent->setTabBarDepth(40);
    tabbedComponent->setLookAndFeel(goldenLookAndFeel.get());
    tabbedComponent->setColour(juce::TabbedComponent::outlineColourId, juce::Colours::transparentBlack);
    juce::Colour tabBg = juce::Colour(0xFF202020);

    // Mic 1 Chain: AIR → SCULPT → EQ → COMP
    excPanel1 = new ExciterPanel(audioEngine, 0, "Mic 1");
    tabbedComponent->addTab("Mic 1 Air", tabBg, excPanel1, true);
    
    sculptPanel1 = new SculptPanel(audioEngine, 0, "Mic 1");
    tabbedComponent->addTab("Mic 1 Sculpt", tabBg, sculptPanel1, true);
    
    eqPanel1 = new EQPanel(audioEngine.getEQProcessor(0), 0, "Mic 1");
    tabbedComponent->addTab("Mic 1 EQ", tabBg, eqPanel1, true);
    
    compPanel1 = new CompressorPanel(audioEngine, 0, "Mic 1");
    tabbedComponent->addTab("Mic 1 Comp", tabBg, compPanel1, true);
    
    // Mic 2 Chain: AIR → SCULPT → EQ → COMP
    excPanel2 = new ExciterPanel(audioEngine, 1, "Mic 2");
    tabbedComponent->addTab("Mic 2 Air", tabBg, excPanel2, true);
    
    sculptPanel2 = new SculptPanel(audioEngine, 1, "Mic 2");
    tabbedComponent->addTab("Mic 2 Sculpt", tabBg, sculptPanel2, true);
    
    eqPanel2 = new EQPanel(audioEngine.getEQProcessor(1), 1, "Mic 2");
    tabbedComponent->addTab("Mic 2 EQ", tabBg, eqPanel2, true);
    
    compPanel2 = new CompressorPanel(audioEngine, 1, "Mic 2");
    tabbedComponent->addTab("Mic 2 Comp", tabBg, compPanel2, true);
    
    // Global Effects
    harmonizerPanel = new HarmonizerPanel(audioEngine);
    tabbedComponent->addTab("Harmonizer", tabBg, harmonizerPanel, true);
    reverbPanel = new ReverbPanel(audioEngine);
    tabbedComponent->addTab("Reverb", tabBg, reverbPanel, true);
    delayPanel = new DelayPanel(audioEngine.getDelayProcessor());
    tabbedComponent->addTab("Delay", tabBg, delayPanel, true);
    dynEqPanel = new DynamicEQPanel(audioEngine);
    tabbedComponent->addTab("Sidechain", tabBg, dynEqPanel, true);
    addAndMakeVisible(tabbedComponent.get());
    
    // ========================================================================
    // COLOR-CODE TAB BUTTONS
    // ========================================================================
    auto& tabBar = tabbedComponent->getTabbedButtonBar();
    
    // Define colors
    juce::Colour greenActive(0xFF00CC66);      // Green for Mic 1
    juce::Colour blueActive(0xFF66B3FF);       // Light blue for Mic 2
    juce::Colour goldenActive(0xFFD4AF37);     // Golden for Global
    juce::Colour darkInactive(0xFF2A2A2A);     // Dark gray for all inactive
    
    // Tabs 0-3: Mic 1 (Green)
    for (int i = 0; i < 4; ++i)
    {
        auto* button = tabBar.getTabButton(i);
        if (button)
        {
            button->setColour(TabbedButtonBar::frontOutlineColourId, greenActive);
            button->setColour(TabbedButtonBar::frontTextColourId, juce::Colours::black);
            button->setColour(TabbedButtonBar::tabOutlineColourId, darkInactive);
            button->setColour(TabbedButtonBar::tabTextColourId, juce::Colours::white);
        }
    }
    
    // Tabs 4-7: Mic 2 (Blue)
    for (int i = 4; i < 8; ++i)
    {
        auto* button = tabBar.getTabButton(i);
        if (button)
        {
            button->setColour(TabbedButtonBar::frontOutlineColourId, blueActive);
            button->setColour(TabbedButtonBar::frontTextColourId, juce::Colours::black);
            button->setColour(TabbedButtonBar::tabOutlineColourId, darkInactive);
            button->setColour(TabbedButtonBar::tabTextColourId, juce::Colours::white);
        }
    }
    
    // Tabs 8-11: Global (Golden)
    for (int i = 8; i < 12; ++i)
    {
        auto* button = tabBar.getTabButton(i);
        if (button)
        {
            button->setColour(TabbedButtonBar::frontOutlineColourId, goldenActive);
            button->setColour(TabbedButtonBar::frontTextColourId, juce::Colours::black);
            button->setColour(TabbedButtonBar::tabOutlineColourId, darkInactive);
            button->setColour(TabbedButtonBar::tabTextColourId, juce::Colours::white);
        }
    }
}

void VocalsPage::timerCallback()
{
    // No longer needed - preamp sliders removed
}

void VocalsPage::updateAllControlsFromEngine()
{
    if (eqPanel1) eqPanel1->updateFromPreset();
    if (eqPanel2) eqPanel2->updateFromPreset();
    if (compPanel1) compPanel1->updateFromPreset();
    if (compPanel2) compPanel2->updateFromPreset();
    if (excPanel1) excPanel1->updateFromPreset();
    if (excPanel2) excPanel2->updateFromPreset();
    if (sculptPanel1) sculptPanel1->updateFromPreset();
    if (sculptPanel2) sculptPanel2->updateFromPreset();
    if (harmonizerPanel) harmonizerPanel->updateFromPreset();
    if (reverbPanel) reverbPanel->updateFromPreset();
    if (delayPanel) delayPanel->updateFromPreset();
    if (dynEqPanel) dynEqPanel->updateFromPreset();
}