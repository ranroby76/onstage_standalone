#pragma once

#ifdef _WIN32
 #include <windows.h>
 #include <psapi.h>
#elif defined(__APPLE__)
 #include <mach/mach.h>
#endif

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "HeaderBar.h"
#include "StyledSlider.h" 
#include "MasterMeter.h" 
#include "InternalPluginBrowser.h"

class AudioEngine;
class PresetManager;
class IOPage;
class WiringCanvas;
class MediaPage;

// ==============================================================================
//  SidebarButton — styled rectangle button for the vertical tab selector
// ==============================================================================
class SidebarButton : public juce::Component
{
public:
    SidebarButton (const juce::String& label)
        : text (label)
    {
        setRepaintsOnMouseActivity (true);
    }

    void setSelected (bool shouldBeSelected)
    {
        if (selected != shouldBeSelected)
        {
            selected = shouldBeSelected;
            repaint();
        }
    }

    bool isSelected() const { return selected; }

    std::function<void()> onClick;

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (8.0f, 3.0f);

        if (selected)
        {
            // ON state: black fill, white frame, white text
            g.setColour (juce::Colours::black);
            g.fillRoundedRectangle (bounds, 4.0f);
            g.setColour (juce::Colours::white);
            g.drawRoundedRectangle (bounds, 4.0f, 1.5f);
            g.setColour (juce::Colours::white);
        }
        else
        {
            // OFF state: grayish-white fill, black frame, black text
            bool hover = isMouseOver();
            auto fillColour = hover ? juce::Colour (0xFFD0D0D0) : juce::Colour (0xFFBBBBBB);
            g.setColour (fillColour);
            g.fillRoundedRectangle (bounds, 4.0f);
            g.setColour (juce::Colours::black);
            g.drawRoundedRectangle (bounds, 4.0f, 1.0f);
            g.setColour (juce::Colour (0xFF1A1A1A));
        }

        g.setFont (juce::Font (13.0f, juce::Font::bold));
        g.drawText (text, bounds, juce::Justification::centred, false);
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (e.mouseWasClicked() && getLocalBounds().contains (e.getPosition()))
            if (onClick)
                onClick();
    }

private:
    juce::String text;
    bool selected = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SidebarButton)
};

// ==============================================================================
//  StatusLed — red/green LED indicator
// ==============================================================================
class StatusLed : public juce::Component
{
public:
    StatusLed() { setOpaque (false); }

    void setActive (bool active)
    {
        if (isActive != active) { isActive = active; repaint(); }
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (1.0f);
        float size = juce::jmin (bounds.getWidth(), bounds.getHeight());
        auto r = bounds.withSizeKeepingCentre (size, size);

        auto onColour  = juce::Colour (0xFF00DD00);  // green
        auto offColour = juce::Colour (0xFFDD0000);  // red

        g.setColour (isActive ? onColour : offColour);
        g.fillEllipse (r);

        // Glow
        if (isActive)
        {
            g.setGradientFill (juce::ColourGradient (
                juce::Colours::white.withAlpha (0.6f), r.getCentre(),
                onColour.withAlpha (0.0f), r.getTopLeft(), true));
            g.fillEllipse (r);
        }

        g.setColour (juce::Colour (0xFF333333));
        g.drawEllipse (r, 0.8f);
    }

private:
    bool isActive = false;
};

// ==============================================================================
//  SidebarPanel — tab selector + status LEDs + system meters
// ==============================================================================
class SidebarPanel : public juce::Component,
                     private juce::Timer
{
public:
    SidebarPanel (AudioEngine& engine)
        : audioEngine (engine)
    {
        for (auto* btn : { &ioButton, &studioButton, &mediaButton })
            addAndMakeVisible (btn);

        ioButton.onClick      = [this] { selectTab (0); };
        studioButton.onClick  = [this] { selectTab (1); };
        mediaButton.onClick   = [this] { selectTab (2); };

        addAndMakeVisible (asioLed);
        addAndMakeVisible (asioLabel);
        asioLabel.setText ("ASIO", juce::dontSendNotification);
        asioLabel.setFont (juce::Font (10.0f, juce::Font::bold));
        asioLabel.setColour (juce::Label::textColourId, juce::Colour (0xFF999999));
        asioLabel.setJustificationType (juce::Justification::centredLeft);

        addAndMakeVisible (regLed);
        addAndMakeVisible (regLabel);
        regLabel.setText ("REG", juce::dontSendNotification);
        regLabel.setFont (juce::Font (10.0f, juce::Font::bold));
        regLabel.setColour (juce::Label::textColourId, juce::Colour (0xFF999999));
        regLabel.setJustificationType (juce::Justification::centredLeft);

        addAndMakeVisible (cpuLabel);
        cpuLabel.setFont (juce::Font (9.5f, juce::Font::plain));
        cpuLabel.setColour (juce::Label::textColourId, juce::Colour (0xFF888888));
        cpuLabel.setJustificationType (juce::Justification::centredLeft);

        addAndMakeVisible (ramLabel);
        ramLabel.setFont (juce::Font (9.5f, juce::Font::plain));
        ramLabel.setColour (juce::Label::textColourId, juce::Colour (0xFF888888));
        ramLabel.setJustificationType (juce::Justification::centredLeft);

        selectTab (0);
        startTimerHz (4);  // 4 Hz polling
    }

    ~SidebarPanel() override { stopTimer(); }

    void selectTab (int index)
    {
        currentTab = index;
        ioButton.setSelected      (index == 0);
        studioButton.setSelected  (index == 1);
        mediaButton.setSelected   (index == 2);

        if (onTabChanged)
            onTabChanged (index);
    }

    int getCurrentTab() const { return currentTab; }

    std::function<void (int)> onTabChanged;

    void paint (juce::Graphics&) override
    {
        // No gradient — transparent background, blends with app bg
    }

    void resized() override
    {
        auto area = getLocalBounds();
        int hPad = 10;

        // --- Tab buttons at top ---
        constexpr int btnHeight = 38;
        constexpr int spacing   = 6;

        auto btnArea = area.removeFromTop (btnHeight * 3 + spacing * 2).reduced (hPad, 0);
        ioButton.setBounds      (btnArea.removeFromTop (btnHeight));
        btnArea.removeFromTop (spacing);
        studioButton.setBounds  (btnArea.removeFromTop (btnHeight));
        btnArea.removeFromTop (spacing);
        mediaButton.setBounds   (btnArea.removeFromTop (btnHeight));

        // --- Status LEDs ---
        area.removeFromTop (12);
        auto ledArea = area.reduced (hPad, 0);

        constexpr int ledSize  = 12;
        constexpr int ledRow   = 16;

        // ASIO row
        auto asioRow = ledArea.removeFromTop (ledRow);
        asioLed.setBounds (asioRow.removeFromLeft (ledSize).withSizeKeepingCentre (ledSize, ledSize));
        asioRow.removeFromLeft (4);
        asioLabel.setBounds (asioRow);

        ledArea.removeFromTop (3);

        // REG row
        auto regRow = ledArea.removeFromTop (ledRow);
        regLed.setBounds (regRow.removeFromLeft (ledSize).withSizeKeepingCentre (ledSize, ledSize));
        regRow.removeFromLeft (4);
        regLabel.setBounds (regRow);

        // --- System meters ---
        ledArea.removeFromTop (10);
        auto metersArea = ledArea.reduced (0, 0);

        constexpr int meterRow = 14;
        cpuLabel.setBounds     (metersArea.removeFromTop (meterRow));
        metersArea.removeFromTop (2);
        ramLabel.setBounds     (metersArea.removeFromTop (meterRow));
    }

private:
    void timerCallback() override
    {
        // --- ASIO status ---
        auto* device = audioEngine.getDeviceManager().getCurrentAudioDevice();
        bool asioConnected = (device != nullptr);
        asioLed.setActive (asioConnected);

        // --- Registration ---
        // Use forward-declared singleton — include is in .cpp
        regLed.setActive (isRegisteredCached);

        // --- CPU usage ---
        double cpu = audioEngine.getDeviceManager().getCpuUsage() * 100.0;
        cpuLabel.setText ("CPU: " + juce::String (cpu, 1) + "%", juce::dontSendNotification);

        // --- RAM usage (process working set) ---
        double ramMb = getCurrentProcessMemoryMB();
        ramLabel.setText ("RAM: " + juce::String ((int) ramMb) + " MB", juce::dontSendNotification);
    }

    static double getCurrentProcessMemoryMB()
    {
       #ifdef _WIN32
        PROCESS_MEMORY_COUNTERS pmc;
        if (GetProcessMemoryInfo (GetCurrentProcess(), &pmc, sizeof (pmc)))
            return pmc.WorkingSetSize / (1024.0 * 1024.0);
       #elif defined(__APPLE__)
        struct mach_task_basic_info info;
        mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
        if (task_info (mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t) &info, &count) == KERN_SUCCESS)
            return info.resident_size / (1024.0 * 1024.0);
       #endif
        return 0.0;
    }

    AudioEngine& audioEngine;

    SidebarButton ioButton      { "I/O" };
    SidebarButton studioButton  { "Studio" };
    SidebarButton mediaButton   { "Media" };

    StatusLed asioLed;
    juce::Label asioLabel;

    StatusLed regLed;
    juce::Label regLabel;

    juce::Label cpuLabel;
    juce::Label ramLabel;

    bool isRegisteredCached = false;

    int currentTab = 0;

    // Allow MainComponent to update registration cache
    friend class MainComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SidebarPanel)
};

// ==============================================================================
//  MainComponent — now also a DragAndDropContainer for the plugin browser
// ==============================================================================
class MainComponent : public juce::Component,
                      public juce::DragAndDropContainer
{
public:
    MainComponent (AudioEngine& engine, PresetManager& presets);
    ~MainComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void showPage (int index);

    AudioEngine&   audioEngine;
    PresetManager& presetManager;

    std::unique_ptr<GoldenSliderLookAndFeel> goldenLookAndFeel;

    HeaderBar header;

    // --- Sidebar replaces TabbedComponent ---
    SidebarPanel sidebar;

    std::unique_ptr<IOPage> ioPage;
    std::unique_ptr<WiringCanvas> wiringCanvas;
    std::unique_ptr<MediaPage> mediaPage;

    int currentPageIndex = 0;

    StyledSlider masterVolumeSlider;
    MidiTooltipLabel masterVolumeLabel;
    MasterMeter masterMeter;

    // --- Internal Plugin Browser (right-side panel) ---
    InternalPluginBrowser pluginBrowser;

    float sliderValueToDb (double sliderValue);
    void savePreset();
    void loadPreset();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};