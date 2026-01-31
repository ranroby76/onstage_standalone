// * **Fix:** Increased height of preamp section (`removeFromTop(80)` instead of 60) to provide reasonable distance for text boxes. <!-- end list -->

#include "VocalsPage.h"
#include "../RegistrationManager.h"

using namespace juce;

VocalsPage::VocalsPage(AudioEngine& engineRef, PresetManager& presetMgr)
    : audioEngine(engineRef), presetManager(presetMgr)
{
    goldenLookAndFeel = std::make_unique<GoldenSliderLookAndFeel>();
    setupPreampGains();
    setupTabbedComponent();
    
    updateAllControlsFromEngine();
    
    startTimer(200); 
}

VocalsPage::~VocalsPage()
{
    stopTimer();
    if (tabbedComponent) tabbedComponent->setLookAndFeel(nullptr);
    if (mic1GainSlider) mic1GainSlider->setLookAndFeel(nullptr);
    if (mic2GainSlider) mic2GainSlider->setLookAndFeel(nullptr);
    eqPanel1 = nullptr; eqPanel2 = nullptr;
    compPanel1 = nullptr; compPanel2 = nullptr;
    excPanel1 = nullptr; excPanel2 = nullptr;
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

    // Preamp Section at top
    // FIX: Increased height to 80 to prevent text box clipping at bottom
    auto preampArea = area.removeFromTop(80);
    
    // Mic 1
    auto m1Area = preampArea.removeFromLeft(300);
    mic1GainLabel.setBounds(m1Area.removeFromTop(20));
    mic1GainSlider->setBounds(m1Area);

    preampArea.removeFromLeft(40);
    // Gap

    // Mic 2
    auto m2Area = preampArea.removeFromLeft(300);
    mic2GainLabel.setBounds(m2Area.removeFromTop(20));
    mic2GainSlider->setBounds(m2Area);
    
    area.removeFromTop(10);
    // Tabs
    if (tabbedComponent)
        tabbedComponent->setBounds(area);
}

void VocalsPage::setupPreampGains()
{
    addAndMakeVisible(mic1GainLabel);
    mic1GainLabel.setText("Mic 1 Preamp", dontSendNotification);
    mic1GainLabel.setColour(Label::textColourId, Colours::white);
    mic1GainSlider = std::make_unique<StyledSlider>(Slider::LinearHorizontal, Slider::TextBoxRight);
    mic1GainSlider->setRange(-60.0, 24.0, 0.1);
    mic1GainSlider->setValue(0.0);
    mic1GainSlider->setTextValueSuffix(" dB");
    mic1GainSlider->onValueChange = [this] {
        audioEngine.setMicPreampGain(0, (float)mic1GainSlider->getValue());
    };
    addAndMakeVisible(mic1GainSlider.get());
    addAndMakeVisible(mic2GainLabel);
    mic2GainLabel.setText("Mic 2 Preamp", dontSendNotification);
    mic2GainLabel.setColour(Label::textColourId, Colours::white);
    mic2GainSlider = std::make_unique<StyledSlider>(Slider::LinearHorizontal, Slider::TextBoxRight);
    mic2GainSlider->setRange(-60.0, 24.0, 0.1);
    mic2GainSlider->setValue(0.0);
    mic2GainSlider->setTextValueSuffix(" dB");
    mic2GainSlider->onValueChange = [this] {
        audioEngine.setMicPreampGain(1, (float)mic2GainSlider->getValue());
    };
    addAndMakeVisible(mic2GainSlider.get());
}

void VocalsPage::setupTabbedComponent()
{
    tabbedComponent = std::make_unique<TabbedComponent>(TabbedButtonBar::TabsAtTop);
    tabbedComponent->setTabBarDepth(40);
    tabbedComponent->setLookAndFeel(goldenLookAndFeel.get());
    tabbedComponent->setColour(juce::TabbedComponent::outlineColourId, juce::Colours::transparentBlack);
    juce::Colour tabBg = juce::Colour(0xFF202020);

    eqPanel1 = new EQPanel(audioEngine.getEQProcessor(0), 0, "Mic 1");
    tabbedComponent->addTab("Mic 1 EQ", tabBg, eqPanel1, true);
    compPanel1 = new CompressorPanel(audioEngine, 0, "Mic 1");
    tabbedComponent->addTab("Mic 1 Comp", tabBg, compPanel1, true);
    excPanel1 = new ExciterPanel(audioEngine, 0, "Mic 1");
    tabbedComponent->addTab("Mic 1 Air", tabBg, excPanel1, true);
    eqPanel2 = new EQPanel(audioEngine.getEQProcessor(1), 1, "Mic 2");
    tabbedComponent->addTab("Mic 2 EQ", tabBg, eqPanel2, true);
    compPanel2 = new CompressorPanel(audioEngine, 1, "Mic 2");
    tabbedComponent->addTab("Mic 2 Comp", tabBg, compPanel2, true);
    excPanel2 = new ExciterPanel(audioEngine, 1, "Mic 2");
    tabbedComponent->addTab("Mic 2 Air", tabBg, excPanel2, true);
    harmonizerPanel = new HarmonizerPanel(audioEngine);
    tabbedComponent->addTab("Harmonizer", tabBg, harmonizerPanel, true);
    reverbPanel = new ReverbPanel(audioEngine);
    tabbedComponent->addTab("Reverb", tabBg, reverbPanel, true);
    delayPanel = new DelayPanel(audioEngine.getDelayProcessor());
    tabbedComponent->addTab("Delay", tabBg, delayPanel, true);
    dynEqPanel = new DynamicEQPanel(audioEngine);
    tabbedComponent->addTab("Sidechain", tabBg, dynEqPanel, true);
    addAndMakeVisible(tabbedComponent.get());
}

void VocalsPage::timerCallback()
{
    bool isPro = RegistrationManager::getInstance().isProMode();
    if (mic2GainSlider) mic2GainSlider->setEnabled(isPro);
}

void VocalsPage::updateAllControlsFromEngine()
{
    mic1GainSlider->setValue(audioEngine.getMicPreampGain(0), dontSendNotification);
    mic2GainSlider->setValue(audioEngine.getMicPreampGain(1), dontSendNotification);
    if (eqPanel1) eqPanel1->updateFromPreset();
    if (eqPanel2) eqPanel2->updateFromPreset();
    if (compPanel1) compPanel1->updateFromPreset();
    if (compPanel2) compPanel2->updateFromPreset();
    if (excPanel1) excPanel1->updateFromPreset();
    if (excPanel2) excPanel2->updateFromPreset();
    if (harmonizerPanel) harmonizerPanel->updateFromPreset();
    if (reverbPanel) reverbPanel->updateFromPreset();
    if (delayPanel) delayPanel->updateFromPreset();
    if (dynEqPanel) dynEqPanel->updateFromPreset();
}