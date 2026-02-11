// ==============================================================================
//  MainComponent.cpp
//  OnStage — Main application component
//
//  MODIFIED: Added DragAndDropContainer inheritance for plugin browser drag-drop
//  MODIFIED: Added InternalPluginBrowser panel to the right of master meters
//  MODIFIED: Right area widened to accommodate browser (visible on Studio tab)
//  MODIFIED: SidebarPanel now shows ASIO/REG LEDs, CPU, RAM, latency
// ==============================================================================

#include "MainComponent.h"
#include "../AudioEngine.h"
#include "../PresetManager.h"
#include "../RegistrationManager.h"
#include "../AppLogger.h"
#include "IOPage.h"
#include "WiringCanvas.h"
#include "MediaPage.h"

#if JUCE_WINDOWS
 #include <windows.h>
 #include <psapi.h>
#elif JUCE_MAC
 #include <mach/mach.h>
#endif

MainComponent::MainComponent (AudioEngine& engine, PresetManager& presets)
    : audioEngine (engine),
      presetManager (presets),
      header (engine),
      sidebar (engine),
      masterVolumeSlider (juce::Slider::LinearVertical, juce::Slider::TextBoxBelow),
      masterMeter (engine)
{
    LOG_INFO ("=== MainComponent Constructor START ===");
    try {
        LOG_INFO ("Step 0a: Checking License...");
        RegistrationManager::getInstance().checkRegistration();
        bool isReg = RegistrationManager::getInstance().isProMode();
        if (isReg)
            LOG_INFO ("License Status: REGISTERED (PRO MODE)");
        else
            LOG_INFO ("License Status: DEMO MODE");

        // Pass registration status to sidebar
        sidebar.isRegisteredCached = isReg;

        LOG_INFO ("Step 0b: Allocating GoldenSliderLookAndFeel...");
        goldenLookAndFeel = std::make_unique<GoldenSliderLookAndFeel>();

        LOG_INFO ("Step 1: Adding header");
        addAndMakeVisible (header);

        // --- Sidebar (replaces TabbedComponent) ---------------------------------
        LOG_INFO ("Step 2: Adding sidebar");
        addAndMakeVisible (sidebar);
        sidebar.onTabChanged = [this] (int index) { showPage (index); };

        LOG_INFO ("Step 3: Creating IOPage");
        ioPage = std::make_unique<IOPage> (audioEngine, audioEngine.getIOSettings());
        addChildComponent (*ioPage);

        LOG_INFO ("Step 4: Creating WiringCanvas (Studio tab)");
        wiringCanvas = std::make_unique<WiringCanvas> (audioEngine.getGraph(), presetManager);
        addChildComponent (*wiringCanvas);

        LOG_INFO ("Step 5: Creating MediaPage");
        mediaPage = std::make_unique<MediaPage> (audioEngine, audioEngine.getIOSettings());
        addChildComponent (*mediaPage);

        // Show the first page
        showPage (0);

        LOG_INFO ("Step 9: Adding master meter");
        addAndMakeVisible (masterMeter);

        LOG_INFO ("Step 10: Setting up master volume slider");
        addAndMakeVisible (masterVolumeSlider);
        masterVolumeSlider.setRange (0.0, 1.0, 0.01);
        masterVolumeSlider.setValue (0.5, juce::dontSendNotification);
        masterVolumeSlider.setMidiInfo ("MIDI: CC 7");
        masterVolumeSlider.setColour (juce::Slider::thumbColourId, juce::Colour (0xFFD4AF37));
        masterVolumeSlider.setColour (juce::Slider::trackColourId, juce::Colour (0xFF404040));
        masterVolumeSlider.setColour (juce::Slider::backgroundColourId, juce::Colour (0xFF202020));
        masterVolumeSlider.onValueChange = [this]() {
            audioEngine.setMasterVolume (static_cast<float> (masterVolumeSlider.getValue()));
        };

        LOG_INFO ("Step 11: Adding master volume label");
        addAndMakeVisible (masterVolumeLabel);
        masterVolumeLabel.setText ("MASTER", juce::dontSendNotification);
        masterVolumeLabel.setFont (juce::Font (12.0f, juce::Font::bold));
        masterVolumeLabel.setColour (juce::Label::textColourId, juce::Colour (0xFFD4AF37));
        masterVolumeLabel.setJustificationType (juce::Justification::centred);
        masterVolumeLabel.setMidiInfo ("MIDI: CC 7");

        // --- Internal Plugin Browser -------------------------------------------
        LOG_INFO ("Step 12a: Setting up InternalPluginBrowser");
        addAndMakeVisible (pluginBrowser);
        pluginBrowser.setVisible (false);   // hidden by default, shown on Studio tab

        pluginBrowser.onEffectDoubleClick = [this] (const InternalEffectInfo& info)
        {
            // Add at a default position on the canvas
            audioEngine.getGraph().addEffect (info.typeID, 300.0f, 300.0f);
            if (wiringCanvas)
                wiringCanvas->markDirty();
        };

        LOG_INFO ("Step 12b: Setting up header callbacks");
        header.onSavePreset = [this]() { savePreset(); };
        header.onLoadPreset = [this]() { loadPreset(); };

        LOG_INFO ("Step 13: Setting window size");
        setSize (1280, 720);

        LOG_INFO ("=== MainComponent Constructor COMPLETE ===");
    }
    catch (const std::exception& e) {
        LOG_ERROR ("EXCEPTION in MainComponent constructor: " + juce::String (e.what()));
        juce::NativeMessageBox::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
            "Startup Error",
            "Error in MainComponent: " + juce::String (e.what()));
    }
    catch (...) {
        LOG_ERROR ("UNKNOWN EXCEPTION in MainComponent constructor");
        juce::NativeMessageBox::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
            "Startup Error",
            "Unknown error in MainComponent constructor");
    }
}

MainComponent::~MainComponent()
{
    LOG_INFO ("MainComponent destructor called");
}

// ==============================================================================
//  Page switching
// ==============================================================================

void MainComponent::showPage (int index)
{
    currentPageIndex = index;

    if (ioPage)        ioPage->setVisible        (index == 0);
    if (wiringCanvas)  wiringCanvas->setVisible  (index == 1);
    if (mediaPage)     mediaPage->setVisible     (index == 2);

    // Show plugin browser only on the Studio (wiring) tab
    pluginBrowser.setVisible (index == 1);

    resized();
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xFF202020));

    // --- Right banner: medium gray with subtle vertical gradient ---
    constexpr int rightBannerWidth = 56;
    constexpr int browserWidth     = 180;

    // Determine total right area width based on current tab
    int totalRightWidth = rightBannerWidth;
    if (currentPageIndex == 1)
        totalRightWidth += browserWidth;

    auto fullRightArea = getLocalBounds().removeFromRight (totalRightWidth).toFloat();

    // Paint the meters/slider column (rightmost 56px strip)
    auto bannerArea = fullRightArea.removeFromRight ((float) rightBannerWidth);
    juce::ColourGradient grad (
        juce::Colour (0xFF3A3A3A), bannerArea.getX(), bannerArea.getY(),
        juce::Colour (0xFF2E2E2E), bannerArea.getX(), bannerArea.getBottom(),
        false);
    g.setGradientFill (grad);
    g.fillRect (bannerArea);

    // Left edge line (separator) of the meters column
    g.setColour (juce::Colour (0xFF1A1A1A));
    g.drawVerticalLine ((int) bannerArea.getX(), 0.0f, (float) getHeight());

    // If browser is visible, paint separator between browser and meters
    if (currentPageIndex == 1)
    {
        g.setColour (juce::Colour (0xFF1A1A1A));
        g.drawVerticalLine ((int) fullRightArea.getX(), 0.0f, (float) getHeight());
    }
}

void MainComponent::resized()
{
    auto bounds = getLocalBounds();

    // --- Right area sizes ---
    constexpr int rightBannerWidth = 56;
    constexpr int browserWidth     = 180;

    // Calculate total right area width based on current tab
    int totalRightWidth = rightBannerWidth;
    if (currentPageIndex == 1)
        totalRightWidth += browserWidth;

    auto rightArea = bounds.removeFromRight (totalRightWidth);

    // --- The rightmost 56px is always the meters/volume banner ---
    auto rightBanner = rightArea.removeFromRight (rightBannerWidth);

    // --- Plugin browser occupies the remaining left portion (when visible) ---
    if (currentPageIndex == 1)
    {
        pluginBrowser.setBounds (rightArea);
    }

    // --- Header bar at top ---
    constexpr int headerHeight = 60;
    header.setBounds (bounds.removeFromTop (headerHeight));

    // --- Sidebar on the left (full height below header) ---
    constexpr int sidebarWidth = 100;

    auto sidebarColumn = bounds.removeFromLeft (sidebarWidth);
    sidebar.setBounds (sidebarColumn);

    // --- Content area — all pages share the same bounds ---
    auto contentArea = bounds;

    if (ioPage)        ioPage->setBounds        (contentArea);
    if (wiringCanvas)  wiringCanvas->setBounds  (contentArea);
    if (mediaPage)     mediaPage->setBounds     (contentArea);

    // --- Right banner internal layout ----------------------------------------
    int bannerH  = rightBanner.getHeight();
    int meterH   = juce::roundToInt (bannerH * 0.45f);
    int sliderH  = juce::roundToInt (bannerH * 0.45f);

    constexpr int pad = 6;

    auto meterArea = rightBanner.removeFromTop (meterH).reduced (pad, pad);
    masterMeter.setBounds (meterArea);

    auto gap = rightBanner.removeFromTop (bannerH - meterH - sliderH);
    masterVolumeLabel.setBounds (gap.reduced (2, 0));

    auto sliderArea = rightBanner.reduced (pad, pad);
    masterVolumeSlider.setBounds (sliderArea.withSizeKeepingCentre (
        juce::jmin (40, sliderArea.getWidth()), sliderArea.getHeight()));
}

float MainComponent::sliderValueToDb (double v)
{
    return v <= 0.0 ? -100.0f : static_cast<float> ((v - 0.5) * 44.0);
}

// ==============================================================================
//  Presets
// ==============================================================================

void MainComponent::savePreset()
{
    auto c = std::make_shared<juce::FileChooser> ("Save",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.onspreset", true);
    c->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
        [this, c] (const juce::FileChooser& fc) {
            auto f = fc.getResult();
            if (f != juce::File{})
            {
                audioEngine.saveGraphState (f);
                header.setPresetName (f.getFileNameWithoutExtension());
            }
        }
    );
}

void MainComponent::loadPreset()
{
    auto c = std::make_shared<juce::FileChooser> ("Load",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.onspreset", true);
    c->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this, c] (const juce::FileChooser& fc) {
            auto f = fc.getResult();
            if (f != juce::File{})
            {
                audioEngine.loadGraphState (f, presetManager);
                header.setPresetName (f.getFileNameWithoutExtension());
            }
        }
    );
}
