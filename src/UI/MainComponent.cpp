// ==============================================================================
//  MainComponent.cpp
//  OnStage — Main application component
//
//  FIX #1: Workspace buttons now have right-click context menu
//  FIX #2: Workspace bar background limited to plugin browser left edge
//  FIX #3: Plugin browser visible on startup (correct initialization order)
//  FIX #4: Zoom slider relocated between left logo and Manual button in header
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
        if (isReg) LOG_INFO ("License Status: REGISTERED (PRO MODE)");
        else       LOG_INFO ("License Status: DEMO MODE");
        sidebar.isRegisteredCached = isReg;

        LOG_INFO ("Step 0b: Allocating GoldenSliderLookAndFeel...");
        goldenLookAndFeel = std::make_unique<GoldenSliderLookAndFeel>();

        LOG_INFO ("Step 1: Adding header");
        addAndMakeVisible (header);

        LOG_INFO ("Step 2: Adding sidebar");
        addAndMakeVisible (sidebar);
        sidebar.onTabChanged = [this] (int index) { showPage (index); };

        LOG_INFO ("Step 3: Creating WiringCanvas (Rack tab)");
        wiringCanvas = std::make_unique<WiringCanvas> (audioEngine.getGraph(), presetManager);
        addChildComponent (*wiringCanvas);

        LOG_INFO ("Step 4: Creating MediaPage");
        mediaPage = std::make_unique<MediaPage> (audioEngine, audioEngine.getIOSettings());
        addChildComponent (*mediaPage);

        LOG_INFO ("Step 5: Creating IOPage");
        ioPage = std::make_unique<IOPage> (audioEngine, audioEngine.getIOSettings());
        addChildComponent (*ioPage);

        LOG_INFO ("Step 6: Adding master meter");
        addAndMakeVisible (masterMeter);

        LOG_INFO ("Step 7: Setting up master volume slider");
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

        LOG_INFO ("Step 8: Adding master volume label");
        addAndMakeVisible (masterVolumeLabel);
        masterVolumeLabel.setText ("MASTER", juce::dontSendNotification);
        masterVolumeLabel.setFont (juce::Font (12.0f, juce::Font::bold));
        masterVolumeLabel.setColour (juce::Label::textColourId, juce::Colour (0xFFD4AF37));
        masterVolumeLabel.setJustificationType (juce::Justification::centred);
        masterVolumeLabel.setMidiInfo ("MIDI: CC 7");

        // --- Internal Plugin Browser -------------------------------------------
        // FIX #3: Add FIRST, then showPage(0) will set it visible
        LOG_INFO ("Step 9: Setting up InternalPluginBrowser");
        addChildComponent (pluginBrowser);   // starts hidden

        pluginBrowser.onEffectDoubleClick = [this] (const InternalEffectInfo& info)
        {
            audioEngine.getGraph().addEffect (info.typeID, 300.0f, 300.0f);
            if (wiringCanvas) wiringCanvas->markDirty();
        };

        // --- Zoom Slider (FIX #4: in header between logo and Manual) -----------
        LOG_INFO ("Step 10: Setting up zoom slider");
        zoomSlider.setSliderStyle (juce::Slider::LinearHorizontal);
        zoomSlider.setRange (0.25, 1.0, 0.75 / 75.0);
        zoomSlider.setValue (1.0, juce::dontSendNotification);
        zoomSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        zoomSlider.setColour (juce::Slider::trackColourId, juce::Colour (80, 80, 90));
        zoomSlider.setColour (juce::Slider::thumbColourId, juce::Colour (0xFFFFD700));
        zoomSlider.setColour (juce::Slider::backgroundColourId, juce::Colour (40, 40, 48));

        zoomSlider.onValueChange = [this]()
        {
            float zoom = (float) zoomSlider.getValue();
            if (wiringCanvas) wiringCanvas->setZoomLevel (zoom);
            int pct = juce::roundToInt (zoom * 100.0f);
            zoomLabel.setText (juce::String (pct) + "%", juce::dontSendNotification);
            bool atDefault = std::abs (zoom - 1.0f) < 0.01f;
            zoomSlider.setColour (juce::Slider::thumbColourId,
                atDefault ? juce::Colour (0xFFFFD700) : juce::Colours::white);
        };

        zoomSlider.setDoubleClickReturnValue (true, 1.0);
        addAndMakeVisible (zoomSlider);

        zoomLabel.setFont (juce::Font (juce::FontOptions (9.0f, juce::Font::bold)));
        zoomLabel.setJustificationType (juce::Justification::centred);
        zoomLabel.setColour (juce::Label::textColourId, juce::Colour (160, 160, 180));
        addAndMakeVisible (zoomLabel);

        // --- Workspace Selector Bar --------------------------------------------
        LOG_INFO ("Step 11: Setting up workspace bar");
        workspaceManager = std::make_unique<WorkspaceManager> (audioEngine.getGraph(), presetManager);

        for (int i = 0; i < WorkspaceManager::maxWorkspaces; ++i)
        {
            workspaceButtons[i].setButtonText (workspaceManager->getName (i));
            workspaceButtons[i].setColour (juce::TextButton::buttonColourId, juce::Colour (40, 40, 45));
            workspaceButtons[i].setColour (juce::TextButton::textColourOffId, juce::Colour (190, 190, 200));
            workspaceButtons[i].onClick = [this, i]()
            {
                if (! workspaceManager->isEnabled (i)) return;
                workspaceManager->switchWorkspace (i);
                zoomSlider.setValue (1.0, juce::sendNotificationSync);
                updateWorkspaceButtonColors();
            };
            addAndMakeVisible (workspaceButtons[i]);
            // FIX #1: Register mouse listener so MainComponent::mouseDown gets right-clicks
            workspaceButtons[i].addMouseListener (this, false);
        }

        workspacesLabel.setFont (juce::Font (juce::FontOptions (9.0f, juce::Font::bold)));
        workspacesLabel.setJustificationType (juce::Justification::centredLeft);
        workspacesLabel.setColour (juce::Label::textColourId, juce::Colour (200, 200, 220));
        addAndMakeVisible (workspacesLabel);
        updateWorkspaceButtonColors();

        // --- Header callbacks ---------------------------------------------------
        LOG_INFO ("Step 12: Setting up header callbacks");
        header.onSavePreset = [this]() { savePreset(); };
        header.onLoadPreset = [this]() { loadPreset(); };

        LOG_INFO ("Step 13: Setting window size");
        setSize (1280, 720);

        // FIX #3: Show initial page LAST so plugin browser gets proper bounds
        showPage (0);

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
//  Page switching — Rack (0), Media (1), I/O (2)
// ==============================================================================

void MainComponent::showPage (int index)
{
    currentPageIndex = index;

    if (wiringCanvas)  wiringCanvas->setVisible  (index == 0);
    if (mediaPage)     mediaPage->setVisible     (index == 1);
    if (ioPage)        ioPage->setVisible        (index == 2);

    pluginBrowser.setVisible (index == 0);

    zoomSlider.setVisible (index == 0);
    zoomLabel.setVisible  (index == 0);

    resized();
}

// ==============================================================================
//  FIX #1: Right-click on workspace buttons → context menu
// ==============================================================================

void MainComponent::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        // Check if the click originated from a workspace button
        for (int i = 0; i < WorkspaceManager::maxWorkspaces; ++i)
        {
            if (e.eventComponent == &workspaceButtons[i])
            {
                showWorkspaceContextMenu (i);
                return;
            }
        }
    }
}

// ==============================================================================
//  Paint
// ==============================================================================

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xFF202020));

    // --- Right banner: medium gray with subtle vertical gradient ---
    constexpr int rightBannerWidth = 56;
    constexpr int browserWidth     = 180;

    int totalRightWidth = rightBannerWidth;
    if (currentPageIndex == 0)
        totalRightWidth += browserWidth;

    auto fullRightArea = getLocalBounds().removeFromRight (totalRightWidth).toFloat();

    auto bannerArea = fullRightArea.removeFromRight ((float) rightBannerWidth);
    juce::ColourGradient grad (
        juce::Colour (0xFF3A3A3A), bannerArea.getX(), bannerArea.getY(),
        juce::Colour (0xFF2E2E2E), bannerArea.getX(), bannerArea.getBottom(),
        false);
    g.setGradientFill (grad);
    g.fillRect (bannerArea);

    g.setColour (juce::Colour (0xFF1A1A1A));
    g.drawVerticalLine ((int) bannerArea.getX(), 0.0f, (float) getHeight());

    if (currentPageIndex == 0)
    {
        g.setColour (juce::Colour (0xFF1A1A1A));
        g.drawVerticalLine ((int) fullRightArea.getX(), 0.0f, (float) getHeight());
    }

    // --- Workspace bar background ---
    // FIX #2: Only draw from sidebar to plugin browser left edge (not full width)
    constexpr int sidebarWidth  = 100;
    constexpr int headerHeight  = 60;

    int wsBarRightEdge = getWidth() - totalRightWidth;

    g.setColour (juce::Colour (0xFF1A1A1F));
    g.fillRect (sidebarWidth, headerHeight,
                wsBarRightEdge - sidebarWidth, workspaceBarHeight);
}

void MainComponent::resized()
{
    auto bounds = getLocalBounds();

    // --- Right area sizes ---
    constexpr int rightBannerWidth = 56;
    constexpr int browserWidth     = 180;

    int totalRightWidth = rightBannerWidth;
    if (currentPageIndex == 0)
        totalRightWidth += browserWidth;

    auto rightArea = bounds.removeFromRight (totalRightWidth);
    auto rightBanner = rightArea.removeFromRight (rightBannerWidth);

    if (currentPageIndex == 0)
        pluginBrowser.setBounds (rightArea);

    // --- Header bar at top ---
    constexpr int headerHeight = 60;
    auto headerArea = bounds.removeFromTop (headerHeight);

    // FIX #4: Zoom slider in header between left logo and Manual button
    // Left logo (Fanan) right edge is at approx x=55 + (height-20)*5.668
    if (currentPageIndex == 0)
    {
        int h = headerArea.getHeight();
        int fananLogoRight = 55 + (int) ((h - 20) * 5.668f);

        int zoomSliderWidth  = 90;
        int zoomSliderHeight = 16;
        int zoomX = fananLogoRight + 12;
        int headerCenterY = headerArea.getY() + h / 2;
        int labelHeight = 12;

        zoomSlider.setBounds (zoomX, headerCenterY - zoomSliderHeight / 2 + 4,
                              zoomSliderWidth, zoomSliderHeight);
        zoomLabel.setBounds  (zoomX, headerCenterY - labelHeight - 2,
                              zoomSliderWidth, labelHeight);
    }

    header.setBounds (headerArea);

    // --- Workspace bar (below header, above content) ---
    auto wsBar = bounds.removeFromTop (workspaceBarHeight);

    // --- Sidebar on the left ---
    constexpr int sidebarWidth = 100;
    auto sidebarColumn = bounds.removeFromLeft (sidebarWidth);
    sidebar.setBounds (sidebarColumn);

    // FIX #1: Workspace bar layout — buttons stretch vertically to plugin browser border
    {
        int startX = sidebarWidth;
        int labelW = 85;
        workspacesLabel.setBounds (startX, wsBar.getY(), labelW, workspaceBarHeight);

        int btnStartX = startX + labelW + 4;
        // FIX #2: Buttons end at the plugin browser left edge
        int btnEndX = getWidth() - totalRightWidth;
        int availableW = btnEndX - btnStartX - 4;
        int btnGap = 2;
        int btnW = (availableW - (WorkspaceManager::maxWorkspaces - 1) * btnGap)
                    / WorkspaceManager::maxWorkspaces;
        if (btnW < 20) btnW = 20;

        for (int i = 0; i < WorkspaceManager::maxWorkspaces; ++i)
        {
            workspaceButtons[i].setBounds (
                btnStartX + i * (btnW + btnGap),
                wsBar.getY() + 2,
                btnW,
                workspaceBarHeight - 4);
        }
    }

    // --- Content area — all pages share the same bounds ---
    auto contentArea = bounds;
    if (wiringCanvas)  wiringCanvas->setBounds  (contentArea);
    if (mediaPage)     mediaPage->setBounds     (contentArea);
    if (ioPage)        ioPage->setBounds        (contentArea);

    // --- Right banner internal layout ----------------------------------------
    int bannerH  = rightBanner.getHeight();
    int meterH   = juce::roundToInt (bannerH * 0.45f);
    int sliderH  = juce::roundToInt (bannerH * 0.45f);
    constexpr int pad = 6;

    auto meterArea = rightBanner.removeFromTop (meterH).reduced (pad, pad);
    masterMeter.setBounds (meterArea);

    auto gapArea = rightBanner.removeFromTop (bannerH - meterH - sliderH);
    masterVolumeLabel.setBounds (gapArea.reduced (2, 0));

    auto sliderArea = rightBanner.reduced (pad, pad);
    masterVolumeSlider.setBounds (sliderArea.withSizeKeepingCentre (
        juce::jmin (40, sliderArea.getWidth()), sliderArea.getHeight()));
}

float MainComponent::sliderValueToDb (double v)
{
    return v <= 0.0 ? -100.0f : static_cast<float> ((v - 0.5) * 44.0);
}

// ==============================================================================
//  Workspace helpers
// ==============================================================================

void MainComponent::updateWorkspaceButtonColors()
{
    if (! workspaceManager) return;

    int active = workspaceManager->getActiveWorkspace();

    for (int i = 0; i < WorkspaceManager::maxWorkspaces; ++i)
    {
        bool isActive   = (i == active);
        bool isEnabled  = workspaceManager->isEnabled (i);
        bool isOccupied = workspaceManager->isOccupied (i);

        if (isActive)
        {
            workspaceButtons[i].setColour (juce::TextButton::buttonColourId, juce::Colour (0xFFD4AF37));
            workspaceButtons[i].setColour (juce::TextButton::textColourOffId, juce::Colours::black);
        }
        else if (isOccupied)
        {
            workspaceButtons[i].setColour (juce::TextButton::buttonColourId, juce::Colour (60, 60, 68));
            workspaceButtons[i].setColour (juce::TextButton::textColourOffId, juce::Colour (220, 220, 230));
        }
        else if (isEnabled)
        {
            workspaceButtons[i].setColour (juce::TextButton::buttonColourId, juce::Colour (45, 45, 52));
            workspaceButtons[i].setColour (juce::TextButton::textColourOffId, juce::Colour (160, 160, 180));
        }
        else
        {
            workspaceButtons[i].setColour (juce::TextButton::buttonColourId, juce::Colour (30, 30, 35));
            workspaceButtons[i].setColour (juce::TextButton::textColourOffId, juce::Colour (80, 80, 90));
        }

        workspaceButtons[i].setButtonText (workspaceManager->getName (i));
    }
}

void MainComponent::showWorkspaceContextMenu (int idx)
{
    if (! workspaceManager) return;

    juce::PopupMenu menu;

    bool isActive  = (idx == workspaceManager->getActiveWorkspace());
    bool isEnabled = workspaceManager->isEnabled (idx);

    if (! isEnabled)
    {
        menu.addItem (1, "Enable");
    }
    else
    {
        menu.addItem (2, "Rename...");
        menu.addItem (3, "Clear", ! isActive || workspaceManager->isOccupied (idx));
        menu.addSeparator();
        menu.addItem (4, "Duplicate to...");
        menu.addSeparator();
        if (! isActive)
            menu.addItem (5, "Disable");
    }

    menu.showMenuAsync (juce::PopupMenu::Options(),
        [this, idx] (int result)
        {
            switch (result)
            {
                case 1:  // Enable
                    workspaceManager->setEnabled (idx, true);
                    break;
                case 2:  // Rename
                {
                    auto current = workspaceManager->getName (idx);
                    auto* aw = new juce::AlertWindow ("Rename Workspace",
                                                      "Enter name:", juce::MessageBoxIconType::NoIcon);
                    aw->addTextEditor ("name", current);
                    aw->addButton ("OK", 1);
                    aw->addButton ("Cancel", 0);
                    aw->enterModalState (true, juce::ModalCallbackFunction::create (
                        [this, idx, aw] (int r)
                        {
                            if (r == 1)
                                workspaceManager->setName (idx, aw->getTextEditorContents ("name"));
                            delete aw;
                            updateWorkspaceButtonColors();
                        }), false);
                    return;
                }
                case 3:  // Clear
                    workspaceManager->clearWorkspace (idx);
                    break;
                case 4:  // Duplicate
                {
                    juce::PopupMenu dupMenu;
                    for (int i = 0; i < WorkspaceManager::maxWorkspaces; ++i)
                        if (i != idx)
                            dupMenu.addItem (100 + i, "Workspace " + workspaceManager->getName (i));
                    dupMenu.showMenuAsync (juce::PopupMenu::Options(),
                        [this, idx] (int r)
                        {
                            if (r >= 100)
                            {
                                workspaceManager->duplicateWorkspace (idx, r - 100);
                                updateWorkspaceButtonColors();
                            }
                        });
                    return;
                }
                case 5:  // Disable
                    workspaceManager->setEnabled (idx, false);
                    break;
                default: return;
            }
            updateWorkspaceButtonColors();
        });
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
                updateWorkspaceButtonColors();
            }
        }
    );
}
