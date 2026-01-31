#include "ManualTab.h"
#include "PluginProcessor.h"

ManualTab::ManualTab(SubterraneumAudioProcessor& p) : processor(p) {
    // Setup chapter buttons
    chapterButtons = { &btnIntro, &btnRack, &btnMixer, &btnStudio, 
                       &btnAudioMidi, &btnPlugins, &btnSystemTools, 
                       &btnShortcuts, &btnTroubleshooting };
    
    for (auto* btn : chapterButtons) {
        addAndMakeVisible(btn);
        btn->addListener(this);
    }
    
    // Setup content view
    addAndMakeVisible(contentView);
    contentView.setMultiLine(true);
    contentView.setReadOnly(true);
    contentView.setScrollbarsShown(true);
    contentView.setCaretVisible(false);
    contentView.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff252525));
    contentView.setColour(juce::TextEditor::textColourId, juce::Colours::lightgrey);
    contentView.setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    contentView.setFont(juce::Font(14.0f));
    
    // Show intro by default
    showChapter(Introduction);
}

void ManualTab::paint(juce::Graphics& g) {
    g.fillAll(Style::colBackground);
    
    // Draw chapter navigation header
    g.setColour(juce::Colour(0xff333333));
    g.fillRect(0, 0, getWidth(), 50);
    
    // Title
    g.setColour(juce::Colours::gold);
    g.setFont(juce::Font(16.0f, juce::Font::bold));
    g.drawText("COLOSSEUM USER MANUAL", 10, 10, 250, 30, juce::Justification::centredLeft);
}

void ManualTab::resized() {
    auto area = getLocalBounds();
    
    // Chapter buttons at top
    auto buttonArea = area.removeFromTop(50).reduced(5);
    buttonArea.removeFromLeft(260);  // Space for title
    
    int buttonWidth = 90;
    int gap = 5;
    
    for (auto* btn : chapterButtons) {
        btn->setBounds(buttonArea.removeFromLeft(buttonWidth));
        buttonArea.removeFromLeft(gap);
    }
    
    // Content area
    area.removeFromTop(5);
    contentView.setBounds(area.reduced(10));
}

void ManualTab::buttonClicked(juce::Button* b) {
    if (b == &btnIntro) showChapter(Introduction);
    else if (b == &btnRack) showChapter(RackView);
    else if (b == &btnMixer) showChapter(Mixer);
    else if (b == &btnStudio) showChapter(Studio);
    else if (b == &btnAudioMidi) showChapter(AudioMidi);
    else if (b == &btnPlugins) showChapter(Plugins);
    else if (b == &btnSystemTools) showChapter(SystemTools);
    else if (b == &btnShortcuts) showChapter(Shortcuts);
    else if (b == &btnTroubleshooting) showChapter(Troubleshooting);
}

void ManualTab::showChapter(Chapter chapter) {
    currentChapter = chapter;
    contentView.setText(getChapterContent(chapter));
    contentView.setCaretPosition(0);
    highlightButton(chapter);
}

void ManualTab::highlightButton(Chapter chapter) {
    for (auto* btn : chapterButtons) {
        btn->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff444444));
    }
    
    juce::TextButton* activeBtn = nullptr;
    switch (chapter) {
        case Introduction: activeBtn = &btnIntro; break;
        case RackView: activeBtn = &btnRack; break;
        case Mixer: activeBtn = &btnMixer; break;
        case Studio: activeBtn = &btnStudio; break;
        case AudioMidi: activeBtn = &btnAudioMidi; break;
        case Plugins: activeBtn = &btnPlugins; break;
        case SystemTools: activeBtn = &btnSystemTools; break;
        case Shortcuts: activeBtn = &btnShortcuts; break;
        case Troubleshooting: activeBtn = &btnTroubleshooting; break;
    }
    
    if (activeBtn)
        activeBtn->setColour(juce::TextButton::buttonColourId, juce::Colours::orange.darker());
}

juce::String ManualTab::getChapterContent(Chapter chapter) {
    switch (chapter) {
        case Introduction:
            return juce::String(
                "===============================================================\n"
                "                    WELCOME TO COLOSSEUM\n"
                "===============================================================\n\n"
                "Colosseum is a powerful mini DAW (Digital Audio Workstation) designed\n"
                "for hosting VST instruments and effects in a flexible, modular environment.\n\n"
                "MAIN FEATURES\n"
                "---------------------------------------------------------------\n"
                "* Modular Rack View - Visual node-based plugin routing\n"
                "* Multi-format Plugin Support - VST3, VST2, AudioUnit (macOS), CLAP\n"
                "* ASIO Audio Engine - Low-latency professional audio\n"
                "* Integrated Mixer - Per-channel volume and metering\n"
                "* Studio Tools - Recording, tempo, and time signature control\n"
                "* System Tools - Built-in utility modules\n\n"
                "QUICK START\n"
                "---------------------------------------------------------------\n"
                "1. Go to Audio/MIDI tab and select your ASIO audio device\n"
                "2. Go to Plugins tab and click 'Scan Plugins' to find your VST plugins\n"
                "3. Go to Rack tab and right-click to add instruments and effects\n"
                "4. Connect modules by dragging from output pins to input pins\n"
                "5. Use the virtual keyboard (Keys button) to play instruments\n\n"
                "INTERFACE OVERVIEW\n"
                "---------------------------------------------------------------\n"
                "* Top Bar - Logo, ASIO status indicator\n"
                "* Instrument Selector - Quick access to loaded instruments\n"
                "* Main Tabs - Rack, Mixer, Studio, Audio/MIDI, Plugins, Manual, Registration\n"
                "* Right Panel - Load/Save Patch, Reset, Virtual Keyboard\n"
            );
            
        case RackView:
            return juce::String(
                "===============================================================\n"
                "                         RACK VIEW\n"
                "===============================================================\n\n"
                "The Rack View is the main workspace for building your audio signal flow.\n\n"
                "ADDING MODULES\n"
                "---------------------------------------------------------------\n"
                "* Right-click on empty space to open the Add Module menu\n"
                "* Choose from INSTRUMENTS, EFFECTS, or SYSTEM TOOLS\n"
                "* Modules appear at the center of the view\n\n"
                "NODE TYPES\n"
                "---------------------------------------------------------------\n"
                "* Audio Input - Your audio interface inputs (gray header)\n"
                "* Audio Output - Your audio interface outputs (gray header)\n"
                "* MIDI Input - Receives MIDI from connected devices\n"
                "* MIDI Output - Sends MIDI to external devices\n"
                "* Instruments - VST instruments (green nodes)\n"
                "* Effects - VST effects (blue nodes)\n"
                "* System Tools - Built-in utility modules (purple/dark)\n\n"
                "PIN COLORS\n"
                "---------------------------------------------------------------\n"
                "* Blue Pins - Audio signals\n"
                "* Green Pins - Sidechain inputs\n"
                "* Red Pins - MIDI signals\n"
                "* Bright/Glowing - Signal present\n\n"
                "MODULE BUTTONS\n"
                "---------------------------------------------------------------\n"
                "* E (Edit) - Open plugin's GUI window\n"
                "* M (Mute/Bypass) - Bypass the plugin\n"
                "* X (Delete) - Remove the module\n"
                "* P (Pass-through) - Effects only: pass audio unchanged\n\n"
                "CONNECTIONS\n"
                "---------------------------------------------------------------\n"
                "* Click and drag from an output pin to an input pin\n"
                "* Right-click a connection to see info or delete it\n"
                "* Connections glow when signal is passing through\n\n"
                "MIDI CHANNEL ROUTING\n"
                "---------------------------------------------------------------\n"
                "* Click the orange channel indicator on instruments\n"
                "* Select which MIDI channels the instrument responds to\n"
                "* 'ALL' = all 16 channels, or select specific channels\n"
            );
            
        case Mixer:
            return juce::String(
                "===============================================================\n"
                "                           MIXER\n"
                "===============================================================\n\n"
                "The Mixer provides channel strips for your audio interface I/O.\n\n"
                "CHANNEL STRIPS\n"
                "---------------------------------------------------------------\n"
                "Each channel strip displays:\n"
                "* Channel name from audio interface\n"
                "* Level meter showing current signal\n"
                "* Volume fader (vertical slider)\n"
                "* Gain value in dB\n\n"
                "INPUT CHANNELS\n"
                "---------------------------------------------------------------\n"
                "* Shows all available input channels from your audio interface\n"
                "* Adjust input gain before signal enters the processing chain\n"
                "* Useful for gain staging microphones and instruments\n\n"
                "OUTPUT CHANNELS\n"
                "---------------------------------------------------------------\n"
                "* Shows all available output channels\n"
                "* Adjust final output levels\n"
                "* Master volume control for your monitors\n\n"
                "METERING\n"
                "---------------------------------------------------------------\n"
                "* Green = Safe signal level (-18dB to -6dB)\n"
                "* Yellow = Moderate level (-6dB to -3dB)\n"
                "* Red = Near clipping (above -3dB)\n"
                "* Clip indicator lights when signal exceeds 0dB\n"
            );
            
        case Studio:
            return juce::String(
                "===============================================================\n"
                "                          STUDIO\n"
                "===============================================================\n\n"
                "The Studio tab provides recording and tempo/timing controls.\n\n"
                "RECORDING\n"
                "---------------------------------------------------------------\n"
                "* REC - Start recording the master output\n"
                "* STOP - Stop recording\n"
                "* Export WAV - Save recording as 24-bit/44.1kHz WAV file\n\n"
                "Recording captures the final stereo output of your session.\n"
                "Maximum recording length: 30 minutes\n\n"
                "RECORDING INDICATOR\n"
                "---------------------------------------------------------------\n"
                "* Red pulsing LED indicates active recording\n"
                "* Time display shows elapsed recording time (HH:MM:SS.mmm)\n"
                "* Format display shows current recording settings\n\n"
                "MASTER TEMPO\n"
                "---------------------------------------------------------------\n"
                "* BPM Slider - Set tempo from 20 to 300 BPM\n"
                "* TAP Button - Tap to set tempo manually\n"
                "* Digital Display - Shows current BPM value\n\n"
                "The master tempo is sent to all tempo-synced plugins via MIDI clock.\n\n"
                "TIME SIGNATURE\n"
                "---------------------------------------------------------------\n"
                "* Numerator - Beats per bar (1-16)\n"
                "* Denominator - Note value (2, 4, 8, 16)\n"
                "* Common signatures: 4/4, 3/4, 6/8\n\n"
                "METRONOME\n"
                "---------------------------------------------------------------\n"
                "* Enable/Disable toggle\n"
                "* Volume slider for metronome click level\n"
            );
            
        case AudioMidi:
            return juce::String(
                "===============================================================\n"
                "                        AUDIO/MIDI\n"
                "===============================================================\n\n"
                "Configure your audio and MIDI devices in this tab.\n\n"
                "DRIVER SETTINGS\n"
                "---------------------------------------------------------------\n"
                "* ASIO Device - Select your audio interface\n"
                "* Control Panel - Open driver's native settings\n"
                "* Status - Shows current connection state\n\n"
                "ASIO REQUIREMENTS\n"
                "---------------------------------------------------------------\n"
                "Colosseum requires ASIO for low-latency audio:\n"
                "* Professional interfaces include native ASIO\n"
                "* Consumer devices can use ASIO4ALL driver\n\n"
                "SAMPLE RATE & BUFFER\n"
                "---------------------------------------------------------------\n"
                "Configure in your ASIO driver's control panel:\n"
                "* 44100 Hz - Standard CD quality\n"
                "* 48000 Hz - Standard video/broadcast\n"
                "* 88200/96000 Hz - High resolution\n"
                "* Buffer: Lower = less latency, higher CPU\n\n"
                "MIDI INPUTS\n"
                "---------------------------------------------------------------\n"
                "* Lists all connected MIDI devices\n"
                "* Channel filter button for each device\n"
                "* Click to select which MIDI channels to receive\n\n"
                "MIDI CHANNEL FILTERING\n"
                "---------------------------------------------------------------\n"
                "Click the channel button next to a MIDI device to:\n"
                "* Enable/disable specific MIDI channels (1-16)\n"
                "* ALL = receive all channels\n"
                "* Useful for multi-instrument setups\n"
            );
            
        case Plugins:
            return juce::String(
                "===============================================================\n"
                "                          PLUGINS\n"
                "===============================================================\n\n"
                "Manage your plugin library in this tab.\n\n"
                "PLUGIN LIST\n"
                "---------------------------------------------------------------\n"
                "* Sort By - Organize by Type or Vendor\n"
                "* INSTRUMENTS - Filter to show only instruments\n"
                "* EFFECTS - Filter to show only effects\n"
                "* ALL - Show all plugins\n"
                "* +/- - Expand/Collapse all categories\n\n"
                "SCANNING PLUGINS\n"
                "---------------------------------------------------------------\n"
                "* Scan Plugins - Search for new plugins\n"
                "* A warning will appear before scanning\n\n"
                "CRASH PROTECTION\n"
                "---------------------------------------------------------------\n"
                "If the app crashes during scanning:\n"
                "1. Reopen Colosseum\n"
                "2. You'll be prompted to blacklist the problematic plugin\n"
                "3. Click 'Yes' to blacklist, then scan again\n\n"
                "PLUGIN FOLDERS\n"
                "---------------------------------------------------------------\n"
                "Click 'Plugin Folders...' to configure:\n"
                "* VST3 search paths\n"
                "* VST2 search paths\n"
                "* AudioUnit paths (macOS)\n"
                "* CLAP paths\n"
                "* Use 'Add Defaults' for standard locations\n\n"
                "BLACKLIST\n"
                "---------------------------------------------------------------\n"
                "* Reset Blacklist - Clear all blacklisted plugins\n"
                "* Use if you've fixed a previously problematic plugin\n\n"
                "SUPPORTED FORMATS\n"
                "---------------------------------------------------------------\n"
                "* VST3 - Modern VST standard (recommended)\n"
                "* VST2 - Legacy VST format\n"
                "* AudioUnit - macOS native format\n"
                "* CLAP - New open plugin standard\n"
            );
            
        case SystemTools:
            return juce::String(
                "===============================================================\n"
                "                       SYSTEM TOOLS\n"
                "===============================================================\n\n"
                "Built-in utility modules available from the right-click menu.\n\n"
                "SIMPLE CONNECTOR (Bus)\n"
                "---------------------------------------------------------------\n"
                "A routing utility for summing and level control.\n\n"
                "Appearance: Purple module\n\n"
                "Controls:\n"
                "* Volume Knob - Drag up/down to adjust\n"
                "  - Minimum: Silence (-inf dB)\n"
                "  - Center: Unity gain (0 dB) - Green indicator\n"
                "  - Maximum: +25 dB boost\n"
                "* M Button - Mute toggle (yellow when active)\n"
                "* X Button - Delete module\n\n"
                "Use Cases:\n"
                "* Sum multiple plugin outputs before master\n"
                "* Create submix groups\n"
                "* Add gain staging points\n\n"
                "STEREO METER\n"
                "---------------------------------------------------------------\n"
                "Visual level metering for stereo signals.\n\n"
                "Appearance: Tall dark module (3x height)\n\n"
                "Features:\n"
                "* L/R Bar Meters - Real-time level display\n"
                "  - Green: Safe level\n"
                "  - Yellow: Moderate\n"
                "  - Orange: High\n"
                "  - Red: Near clipping\n"
                "* Peak Hold - White line shows recent peaks\n"
                "* CLIP LED - Lights red when clipping detected\n"
                "* Click CLIP to reset\n\n"
                "Controls:\n"
                "* X Button - Delete module\n\n"
                "Use Cases:\n"
                "* Monitor levels at any point in chain\n"
                "* Check for clipping before output\n"
                "* Visual feedback during mixing\n"
            );
            
        case Shortcuts:
            return juce::String(
                "===============================================================\n"
                "                        SHORTCUTS\n"
                "===============================================================\n\n"
                "RACK VIEW\n"
                "---------------------------------------------------------------\n"
                "* Right-Click (empty) - Open Add Module menu\n"
                "* Right-Click (pin) - Show pin information\n"
                "* Right-Click (wire) - Wire info / Delete connection\n"
                "* Double-Click (wire) - Delete connection\n"
                "* Drag (pin to pin) - Create connection\n"
                "* Drag (module) - Move module position\n"
                "* Click E button - Open plugin editor\n"
                "* Click M button - Toggle bypass/mute\n"
                "* Click X button - Delete module\n"
                "* Click P button - Toggle pass-through (effects)\n\n"
                "GENERAL\n"
                "---------------------------------------------------------------\n"
                "* Load Patch - Open saved session\n"
                "* Save Patch - Save current session\n"
                "* Reset - Clear all and start fresh\n"
                "* Keys - Toggle virtual MIDI keyboard\n\n"
                "PLUGIN EDITOR WINDOWS\n"
                "---------------------------------------------------------------\n"
                "* Each plugin opens in its own window\n"
                "* Windows can be moved and resized\n"
                "* Close with X button or window close\n"
            );
            
        case Troubleshooting:
            return juce::String(
                "===============================================================\n"
                "                     TROUBLESHOOTING\n"
                "===============================================================\n\n"
                "NO SOUND\n"
                "---------------------------------------------------------------\n"
                "1. Check ASIO status LED (top right) - should be green\n"
                "2. Go to Audio/MIDI tab, verify device is selected\n"
                "3. Check that Audio Output node is visible in Rack\n"
                "4. Click ON button on Audio Output if grayed out\n"
                "5. Verify connections from instruments to output\n"
                "6. Check mixer output levels\n\n"
                "ASIO NOT WORKING\n"
                "---------------------------------------------------------------\n"
                "1. Ensure no other application is using the audio device\n"
                "2. Try opening the ASIO Control Panel and adjusting buffer size\n"
                "3. For USB interfaces, try different USB ports\n"
                "4. Consider using ASIO4ALL if native ASIO unavailable\n\n"
                "PLUGIN CRASHES DURING SCAN\n"
                "---------------------------------------------------------------\n"
                "1. Reopen Colosseum after crash\n"
                "2. Accept the prompt to blacklist the problem plugin\n"
                "3. Run scan again\n"
                "4. If needed, manually remove plugin from folder\n\n"
                "PLUGIN NOT APPEARING\n"
                "---------------------------------------------------------------\n"
                "1. Go to Plugins tab > Plugin Folders\n"
                "2. Verify the correct folders are added\n"
                "3. Click 'Add Defaults' for standard locations\n"
                "4. Run 'Scan Plugins' again\n"
                "5. Check if plugin is 64-bit (32-bit not supported)\n\n"
                "HIGH CPU USAGE\n"
                "---------------------------------------------------------------\n"
                "1. Increase ASIO buffer size in Control Panel\n"
                "2. Reduce number of active plugins\n"
                "3. Use bypass (M) on unused plugins\n"
                "4. Consider freezing tracks in your main DAW\n\n"
                "MIDI NOT WORKING\n"
                "---------------------------------------------------------------\n"
                "1. Go to Audio/MIDI tab\n"
                "2. Verify MIDI device appears in list\n"
                "3. Check channel filter settings\n"
                "4. Try the virtual keyboard (Keys button)\n"
                "5. Check instrument's MIDI channel setting (orange indicator)\n\n"
                "CONNECTIONS NOT WORKING\n"
                "---------------------------------------------------------------\n"
                "* Ensure you're connecting output to input (not same direction)\n"
                "* Audio pins (blue) only connect to audio pins\n"
                "* MIDI pins (red) only connect to MIDI pins\n"
                "* Check that modules aren't bypassed\n"
            );
            
        default:
            return "Select a chapter from the navigation above.";
    }
}
