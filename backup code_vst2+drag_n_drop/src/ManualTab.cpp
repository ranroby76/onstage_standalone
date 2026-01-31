#include "ManualTab.h"
#include "PluginProcessor.h"

ManualTab::ManualTab(SubterraneumAudioProcessor& p) : processor(p) {
    // Setup chapter buttons
    chapterButtons = { &btnIntro, &btnRack, &btnMixer, &btnStudio, 
                       &btnAudioMidi, &btnPlugins, &btnSystemTools, 
                       &btnResourceMeters, &btnShortcuts, &btnTroubleshooting, &btnDemoLimitations };
    
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
    else if (b == &btnResourceMeters) showChapter(ResourceMeters);
    else if (b == &btnShortcuts) showChapter(Shortcuts);
    else if (b == &btnTroubleshooting) showChapter(Troubleshooting);
    else if (b == &btnDemoLimitations) showChapter(DemoLimitations);
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
        case ResourceMeters: activeBtn = &btnResourceMeters; break;
        case Shortcuts: activeBtn = &btnShortcuts; break;
        case Troubleshooting: activeBtn = &btnTroubleshooting; break;
        case DemoLimitations: activeBtn = &btnDemoLimitations; break;
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
                "* Multi-format Plugin Support - VST3, AudioUnit (macOS), LADSPA (Linux)\n"
                "* ASIO Audio Engine - Low-latency professional audio\n"
                "* Integrated Mixer - Per-channel gain control\n"
                "* Studio Tools - Recording, tempo, and time signature control\n"
                "* 32 Instrument Slots - MIDI-controllable instrument switching\n"
                "* System Tools - Built-in utility modules\n\n"
                "SYSTEM REQUIREMENTS\n"
                "---------------------------------------------------------------\n"
                "WINDOWS:\n"
                "* Windows 10 or later (64-bit)\n"
                "* ASIO-compatible audio interface (or ASIO4ALL)\n"
                "* Intel/AMD x64 processor (dual-core recommended)\n"
                "* 4GB RAM minimum (8GB recommended)\n"
                "* 100MB disk space (plus space for plugins)\n\n"
                "macOS:\n"
                "* macOS 10.13 High Sierra or later\n"
                "* Intel or Apple Silicon processor\n"
                "* 4GB RAM minimum (8GB recommended)\n"
                "* 100MB disk space (plus space for plugins)\n\n"
                "LINUX:\n"
                "* Ubuntu 20.04 or equivalent (64-bit)\n"
                "* JACK or ALSA audio system\n"
                "* Intel/AMD x64 processor (dual-core recommended)\n"
                "* 4GB RAM minimum (8GB recommended)\n"
                "* 100MB disk space (plus space for plugins)\n\n"
                "QUICK START\n"
                "---------------------------------------------------------------\n"
                "1. Go to Settings tab and select your audio device\n"
                "2. Go to Plugins tab and click 'Scan Plugins' to find your VST plugins\n"
                "3. Go to Rack tab and right-click to add instruments and effects\n"
                "4. Connect modules by dragging from output pins to input pins\n"
                "5. Use the virtual keyboard (Keys button) to play instruments\n\n"
                "INTERFACE OVERVIEW\n"
                "---------------------------------------------------------------\n"
                "* Top Bar - Logo, registration status, ASIO status indicator\n"
                "* Main Tabs - Rack, Mixer, Studio, Settings, Plugins, Manual, Register\n"
                "* Bottom Bar - Load/Save/Reset buttons, Keyboard toggle\n"
                "* Footer - 32-slot instrument selector with Multi-Mode toggle\n"
            );
            
        case RackView:
            return juce::String(
                "===============================================================\n"
                "                         RACK VIEW\n"
                "===============================================================\n\n"
                "The Rack is your main workspace for building audio chains.\n\n"
                "ADDING MODULES\n"
                "---------------------------------------------------------------\n"
                "* Right-click on empty space to open the Add Module menu\n"
                "* Choose from INSTRUMENTS, EFFECTS, or SYSTEM TOOLS\n"
                "* New modules appear at your click position\n\n"
                "MODULE TYPES\n"
                "---------------------------------------------------------------\n"
                "* Audio Input (pink header) - Hardware audio inputs\n"
                "* Audio Output (pink header) - Hardware audio outputs\n"
                "* MIDI Input (red header) - Hardware MIDI inputs\n"
                "* MIDI Output (red header) - Hardware MIDI outputs\n"
                "* Instruments (green header) - VST/AU instruments\n"
                "* Effects (blue header) - VST/AU effects\n"
                "* System Tools - Built-in utility modules (purple/dark)\n\n"
                "CONNECTIONS\n"
                "---------------------------------------------------------------\n"
                "* Drag from output pin (bottom) to input pin (top)\n"
                "* Blue pins = Audio\n"
                "* Red pins = MIDI\n"
                "* Orange pins = Sidechain\n"
                "* Double-click wire to delete connection\n"
                "* Right-click wire for options\n\n"
                "MODULE BUTTONS\n"
                "---------------------------------------------------------------\n"
                "* E - Open plugin editor window\n"
                "* CH - MIDI channel selector (instruments only)\n"
                "* M - Mute/Bypass toggle\n"
                "* P - Pass-through mode (effects only)\n"
                "* X - Delete module\n\n"
                "MOVING MODULES\n"
                "---------------------------------------------------------------\n"
                "* Click and drag any module to reposition\n"
                "* Modules stay within the rack boundaries\n"
            );
            
        case Mixer:
            return juce::String(
                "===============================================================\n"
                "                          MIXER\n"
                "===============================================================\n\n"
                "The Mixer provides per-channel level control.\n\n"
                "CHANNEL STRIPS\n"
                "---------------------------------------------------------------\n"
                "Each strip shows:\n"
                "* Channel name at top\n"
                "* Vertical fader for gain\n"
                "* dB value display\n"
                "* Level meter on the right\n\n"
                "FADER RANGE\n"
                "---------------------------------------------------------------\n"
                "* -infinity (silence) to +12dB boost\n"
                "* Unity (0dB) at default position\n"
                "* Drag fader up/down to adjust\n\n"
                "LEVEL METERS\n"
                "---------------------------------------------------------------\n"
                "* Green - Normal levels\n"
                "* Yellow - High levels (approaching 0dB)\n"
                "* Red - Clipping risk\n\n"
                "CHANNEL VISIBILITY\n"
                "---------------------------------------------------------------\n"
                "* Only active channels are shown\n"
                "* Channels appear when audio is routed\n"
                "* Master output channel always visible\n"
            );
            
        case Studio:
            return juce::String(
                "===============================================================\n"
                "                          STUDIO\n"
                "===============================================================\n\n"
                "Studio tools for tempo and timing control are now in the\n"
                "Audio/MIDI Settings tab.\n\n"
                "See the Audio/MIDI tab for:\n"
                "* Master Tempo control\n"
                "* Time Signature settings\n"
                "* Metronome controls\n\n"
                "RECORDING\n"
                "---------------------------------------------------------------\n"
                "Use the Recorder system tool (right-click > System Tools > Recorder)\n"
                "to record audio at any point in your signal chain.\n\n"
                "TEMPO SETTINGS\n"
                "---------------------------------------------------------------\n"
                "* BPM slider (20-300 BPM)\n"
                "* TAP button for tap tempo\n"
                "* Numerical display of current tempo\n\n"
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
                "* Control Panel - Open driver's native settings (Windows)\n"
                "* Status - Shows current connection state\n"
                "* OFF option - Disable audio device\n"
                "* Set Recording Folder - Choose default location for recordings\n\n"
                "ASIO REQUIREMENTS\n"
                "---------------------------------------------------------------\n"
                "Colosseum requires ASIO for low-latency audio:\n"
                "* Professional interfaces include native ASIO\n"
                "* Consumer devices can use ASIO4ALL driver\n"
                "* macOS uses CoreAudio by default\n"
                "* Linux uses JACK or ALSA\n\n"
                "SAMPLE RATE & BUFFER\n"
                "---------------------------------------------------------------\n"
                "Configure in your ASIO driver's control panel:\n"
                "* 44100 Hz - Standard CD quality\n"
                "* 48000 Hz - Standard video/broadcast\n"
                "* 88200/96000 Hz - High resolution\n"
                "* Buffer: Lower = less latency, higher CPU\n\n"
                "MIDI INPUTS (Red Frame)\n"
                "---------------------------------------------------------------\n"
                "* Lists all connected MIDI input devices\n"
                "* Dark red channel filter button for each device\n"
                "* Button states:\n"
                "  - OFF = Device disabled (gray)\n"
                "  - ALL = All 16 MIDI channels enabled (dark red)\n"
                "  - CH X = Single channel enabled (dark red)\n"
                "  - X CH = Multiple channels enabled (dark red)\n\n"
                "MIDI OUTPUTS (Gold Frame)\n"
                "---------------------------------------------------------------\n"
                "* Lists all connected MIDI output devices\n"
                "* Gold channel filter button for each device\n"
                "* Same channel filtering as inputs\n"
                "* Useful for sending MIDI to external hardware\n\n"
                "MIDI CHANNEL FILTERING\n"
                "---------------------------------------------------------------\n"
                "Click the channel button next to a MIDI device to:\n"
                "* Enable/disable specific MIDI channels (1-16)\n"
                "* ALL = receive/send all channels (pass-through)\n"
                "* Select specific channels for channel remapping\n"
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
                "* Choose 'Scan for New' or 'Rescan All'\n"
                "* Wait for scan to complete\n\n"
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
                "* AudioUnit paths (macOS only)\n"
                "* LADSPA paths (Linux only)\n"
                "* Use 'Add Defaults' for standard locations\n\n"
                "BLACKLIST\n"
                "---------------------------------------------------------------\n"
                "* Reset Blacklist - Clear all blacklisted plugins\n"
                "* Use if you've fixed a previously problematic plugin\n\n"
                "SUPPORTED FORMATS\n"
                "---------------------------------------------------------------\n"
                "* VST3 - Modern VST standard (recommended)\n"
                "* AudioUnit - macOS native format\n"
                "* LADSPA - Linux audio plugin standard\n\n"
                "FUTURE SUPPORT\n"
                "---------------------------------------------------------------\n"
                "Support for VST2, CLAP, and additional formats will be\n"
                "added when JUCE 9 is released with full support for these\n"
                "plugin standards.\n"
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
                "* Visual feedback during mixing\n\n"
                "MIDI MONITOR\n"
                "---------------------------------------------------------------\n"
                "Real-time MIDI activity display.\n\n"
                "Appearance: Tall dark module (3x height)\n\n"
                "Features:\n"
                "* Live MIDI Event Display - Shows incoming MIDI messages\n"
                "* Channel Number - Which MIDI channel (1-16)\n"
                "* Note Name - Musical note (e.g., C4, F#5)\n"
                "* Velocity - Note velocity value (1-127)\n"
                "* Color Coding:\n"
                "  - Green: Note On messages\n"
                "  - Orange: Note Off messages\n"
                "* Auto-scroll - Most recent events at top\n"
                "* 'Waiting for MIDI...' when no activity\n\n"
                "Controls:\n"
                "* X Button - Delete module\n\n"
                "Use Cases:\n"
                "* Debug MIDI routing issues\n"
                "* Verify MIDI input is reaching instruments\n"
                "* Monitor MIDI channel assignments\n"
                "* Check velocity response from controllers\n\n"
                "RECORDER\n"
                "---------------------------------------------------------------\n"
                "Audio recorder for capturing stereo signals to disk.\n\n"
                "Appearance: Wide dark module (2x width, 3x height)\n\n"
                "Features:\n"
                "* Records 24-bit WAV files\n"
                "* Real-time waveform visualization\n"
                "* Stereo level meters (L/R)\n"
                "* Time display (MM:SS.t or H:MM:SS)\n"
                "* Custom filename prefix\n"
                "* Sync mode for linked recording\n\n"
                "Controls:\n"
                "* Name Box - Click to edit recording name prefix\n"
                "* Record Button (red circle) - Start recording\n"
                "* Stop Button (blue square) - Stop and save recording\n"
                "* Folder Button - Open recording folder in file explorer\n"
                "* SYNC/INDEPENDENT Toggle - Link multiple recorders\n"
                "* X Button - Delete module\n\n"
                "Right-Click Tooltips:\n"
                "* Right-click any button for help tooltip\n\n"
                "Recording Output:\n"
                "* Files saved as: [Name]_YYYYMMDD_HHMMSS.wav\n"
                "* Default folder: Documents/Colosseum/recordings\n"
                "* Change default in Audio/MIDI Settings tab\n\n"
                "Sync Mode:\n"
                "* When SYNC is ON (cyan), all synced recorders\n"
                "  start and stop together\n"
                "* When INDEPENDENT (gray), recorder operates alone\n"
                "* Perfect for multi-track recording\n\n"
                "Use Cases:\n"
                "* Record final mix output\n"
                "* Capture individual instrument tracks\n"
                "* Record pre-effect and post-effect simultaneously\n"
                "* Create stems for mixing in other DAWs\n"
            );
            
        case ResourceMeters:
            return juce::String(
                "===============================================================\n"
                "                     RESOURCE METERS\n"
                "===============================================================\n\n"
                "Colosseum displays real-time CPU and RAM usage meters in the\n"
                "top-right corner of the main window, next to the REG LED.\n\n"
                "CPU METER\n"
                "---------------------------------------------------------------\n"
                "Displays: CPU: XX.X%\n\n"
                "What it measures:\n"
                "* Audio processing load (DSP usage)\n"
                "* Percentage of audio buffer time being used\n"
                "* NOT general system CPU usage\n\n"
                "The CPU meter shows how much of each audio buffer cycle is\n"
                "consumed by audio processing. This is critical for real-time\n"
                "audio performance.\n\n"
                "Understanding the values:\n"
                "* 0-50%  : Safe, plenty of headroom\n"
                "* 50-70% : Moderate usage, acceptable\n"
                "* 70-85% : High usage, approaching limits\n"
                "* 85%+   : Critical, risk of audio dropouts\n\n"
                "Technical details:\n"
                "* Provided by JUCE's AudioDeviceManager\n"
                "* Measures actual time spent in processBlock()\n"
                "* Updates in real-time during audio playback\n"
                "* Specific to audio processing, not UI or background tasks\n\n"
                "RAM METER\n"
                "---------------------------------------------------------------\n"
                "Displays: RAM: XXXMB\n\n"
                "What it measures:\n"
                "* Colosseum's memory usage (process-specific)\n"
                "* NOT total system RAM\n"
                "* Shows current allocated memory in megabytes\n\n"
                "The RAM meter displays how much memory Colosseum is currently\n"
                "using, including all loaded plugins, audio buffers, and data.\n\n"
                "Platform-specific measurement:\n"
                "* Windows: WorkingSetSize (physical RAM used by process)\n"
                "* macOS: resident_size (memory resident in RAM)\n"
                "* Linux: VmRSS (Resident Set Size from /proc/self/status)\n\n"
                "Understanding the values:\n"
                "* 50-200MB  : Light usage, minimal plugins\n"
                "* 200-500MB : Normal usage, several plugins loaded\n"
                "* 500MB-1GB : Heavy usage, many large sample libraries\n"
                "* 1GB+      : Very heavy, multiple large orchestral libraries\n\n"
                "Factors affecting RAM usage:\n"
                "* Number of loaded plugins\n"
                "* Sample library sizes (especially orchestral instruments)\n"
                "* Audio buffer settings\n"
                "* Plugin GUI windows (each window consumes memory)\n\n"
                "PERFORMANCE OPTIMIZATION\n"
                "---------------------------------------------------------------\n"
                "If CPU meter is high (>70%):\n"
                "* Increase ASIO buffer size in audio driver control panel\n"
                "* Bypass (M button) unused plugins\n"
                "* Close plugin editor windows you're not using\n"
                "* Reduce number of active plugins\n"
                "* Use freeze/render techniques for complex chains\n\n"
                "If RAM usage is high:\n"
                "* Unload unused instruments from the rack\n"
                "* Close plugin editor windows\n"
                "* Use smaller sample libraries or purge unused samples\n"
                "* Check for memory leaks in third-party plugins\n\n"
                "WHY THESE METERS MATTER\n"
                "---------------------------------------------------------------\n"
                "Real-time audio processing is extremely time-sensitive.\n"
                "Audio must be processed within strict buffer deadlines:\n\n"
                "* If CPU usage exceeds 100%, you get audio dropouts/clicks\n"
                "* If RAM is insufficient, system may swap to disk (very slow)\n"
                "* These meters help you stay within safe operating limits\n\n"
                "The CPU meter is especially critical - it directly indicates\n"
                "whether your system can handle the current processing load\n"
                "in real-time without audible artifacts.\n"
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
                "* Right-Click (button) - Show button tooltip/help\n"
                "* Double-Click (wire) - Delete connection\n"
                "* Drag (pin to pin) - Create connection\n"
                "* Drag (module) - Move module position\n"
                "* Click E button - Open plugin editor\n"
                "* Click M button - Toggle bypass/mute\n"
                "* Click X button - Delete module\n"
                "* Click P button - Toggle pass-through (effects)\n\n"
                "RECORDER MODULE\n"
                "---------------------------------------------------------------\n"
                "* Click Record (red) - Start recording\n"
                "* Click Stop (blue) - Stop and save\n"
                "* Click Folder - Open recordings folder\n"
                "* Click Name box - Edit filename prefix\n"
                "* Click SYNC/INDEPENDENT - Toggle sync mode\n"
                "* Right-click any button for tooltip\n\n"
                "INSTRUMENT SELECTOR (Bottom Footer)\n"
                "---------------------------------------------------------------\n"
                "* Click any slot - Activate/deactivate instrument\n"
                "* Right-Click slot - Show MIDI controller info\n"
                "* Multi-Mode toggle:\n"
                "  - OFF = Single instrument mode (one at a time)\n"
                "  - ON = Multiple instruments simultaneously\n\n"
                "MIDI NOTE CONTROL\n"
                "---------------------------------------------------------------\n"
                "* MIDI Note 1 = Select Instrument 1\n"
                "* MIDI Note 2 = Select Instrument 2\n"
                "* ... up to MIDI Note 32 = Instrument 32\n"
                "* Notes 1-32 are reserved for selection\n"
                "* Perfect for MIDI pad controllers\n"
                "* Works in both single and multi-mode\n\n"
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
                "2. Check Registration LED (top right) - should be green\n"
                "3. Go to Settings tab and verify audio device is selected\n"
                "4. Make sure audio device is not 'OFF'\n"
                "5. Click ON button on Audio Output if grayed out\n"
                "6. Verify connections from instruments to output\n"
                "7. Check mixer output levels (not muted)\n\n"
                "ASIO NOT WORKING\n"
                "---------------------------------------------------------------\n"
                "1. Ensure no other application is using the audio device\n"
                "2. Try opening the ASIO Control Panel and adjusting buffer size\n"
                "3. For USB interfaces, try different USB ports\n"
                "4. Consider using ASIO4ALL if native ASIO unavailable\n"
                "5. Check cables and power to audio interface\n\n"
                "PLUGIN CRASHES DURING SCAN\n"
                "---------------------------------------------------------------\n"
                "1. Reopen Colosseum after crash\n"
                "2. Accept the prompt to blacklist the problem plugin\n"
                "3. Run scan again\n"
                "4. If needed, manually remove plugin from folder\n"
                "5. Update plugin to latest version\n\n"
                "PLUGIN NOT APPEARING\n"
                "---------------------------------------------------------------\n"
                "1. Go to Plugins tab > Plugin Folders\n"
                "2. Verify the correct folders are added\n"
                "3. Click 'Add Defaults' for standard locations\n"
                "4. Run 'Scan Plugins' again\n"
                "5. Check if plugin is 64-bit (32-bit not supported)\n"
                "6. Check plugin format matches OS (VST3/VST2/AU/CLAP)\n\n"
                "HIGH CPU USAGE\n"
                "---------------------------------------------------------------\n"
                "1. Increase ASIO buffer size in Control Panel\n"
                "2. Reduce number of active plugins\n"
                "3. Use bypass (M) on unused plugins\n"
                "4. Close unnecessary plugin editor windows\n"
                "5. Disable unused mixer channels\n\n"
                "MIDI NOT WORKING\n"
                "---------------------------------------------------------------\n"
                "1. Go to Audio/MIDI tab\n"
                "2. Verify MIDI device appears in list\n"
                "3. Check channel filter settings (should show ALL or channels)\n"
                "4. Try the virtual keyboard (Keys button)\n"
                "5. Check instrument's MIDI channel setting (orange indicator)\n"
                "6. Verify MIDI cable is connected properly\n"
                "7. Check if MIDI device is powered on\n"
                "8. Use MIDI Monitor system tool to debug\n\n"
                "INSTRUMENT SELECTOR NOT WORKING\n"
                "---------------------------------------------------------------\n"
                "1. Check that instruments are loaded in Rack view\n"
                "2. Instrument selector is at bottom footer (not top)\n"
                "3. Multi-Mode toggle affects selection behavior:\n"
                "   - OFF = Only one instrument at a time\n"
                "   - ON = Multiple instruments can be active\n"
                "4. MIDI notes 1-32 reserved for instrument switching\n\n"
                "CONNECTIONS NOT WORKING\n"
                "---------------------------------------------------------------\n"
                "* Ensure you're connecting output to input (not same direction)\n"
                "* Audio pins (blue) only connect to audio pins\n"
                "* MIDI pins (red) only connect to MIDI pins\n"
                "* Check that modules aren't bypassed (M button)\n"
                "* Verify both nodes are not frozen or disabled\n\n"
                "RECORDING ISSUES\n"
                "---------------------------------------------------------------\n"
                "1. Check that audio is reaching the Recorder input\n"
                "2. Verify recording folder exists and is writable\n"
                "3. Use 'Set Recording Folder' in Settings if needed\n"
                "4. Check disk space availability\n"
                "5. Use Folder button to verify recordings are saved\n"
            );
            
        case DemoLimitations:
            return juce::String(
                "===============================================================\n"
                "                     DEMO LIMITATIONS\n"
                "===============================================================\n\n"
                "Colosseum operates in DEMO MODE until registered with a valid\n"
                "license. The demo version has the following limitation:\n\n"
                "AUDIO SILENCE INTERRUPTION\n"
                "---------------------------------------------------------------\n"
                "* 3 seconds of complete silence every 15 seconds\n"
                "* Affects both audio inputs AND outputs\n"
                "* Affects MIDI input processing\n"
                "* Cannot be bypassed or disabled\n\n"
                "CYCLE PATTERN\n"
                "---------------------------------------------------------------\n"
                "The demo mode follows this repeating pattern:\n\n"
                "1. Normal Operation - 15 seconds\n"
                "   - Full audio input/output\n"
                "   - Full MIDI processing\n"
                "   - All features available\n\n"
                "2. Silence Period - 3 seconds\n"
                "   - Audio inputs cleared (no sound in)\n"
                "   - Audio outputs cleared (no sound out)\n"
                "   - MIDI inputs blocked\n"
                "   - All-Notes-Off sent to prevent stuck notes\n\n"
                "3. Pattern repeats indefinitely...\n\n"
                "WHAT IS NOT LIMITED\n"
                "---------------------------------------------------------------\n"
                "* All features are fully functional\n"
                "* No restrictions on:\n"
                "  - Number of plugins\n"
                "  - Save/Load patches\n"
                "  - Recording (except during silence)\n"
                "  - Plugin scanning\n"
                "  - MIDI routing\n"
                "  - Audio routing\n"
                "  - Mixer controls\n"
                "  - Studio tools\n\n"
                "REGISTRATION LED INDICATOR\n"
                "---------------------------------------------------------------\n"
                "* Top right corner of main window\n"
                "* Demo Mode: Orange/Red 'REG' LED\n"
                "* Registered: Green 'REG' LED\n\n"
                "HOW TO REGISTER\n"
                "---------------------------------------------------------------\n"
                "1. Go to Register tab\n"
                "2. Note your Machine ID\n"
                "3. Click 'Copy ID' button\n"
                "4. Visit our website to purchase a license\n"
                "5. Provide your Machine ID\n"
                "6. Receive your serial number via email\n"
                "7. Enter serial number in Register tab\n"
                "8. Click 'Register' button\n\n"
                "Once registered, the audio silence limitation is\n"
                "permanently removed and you can use Colosseum\n"
                "professionally without any interruptions.\n\n"
                "DEMO MODE PURPOSE\n"
                "---------------------------------------------------------------\n"
                "The demo mode allows you to:\n"
                "* Test all features before purchasing\n"
                "* Evaluate plugin compatibility\n"
                "* Learn the workflow\n"
                "* Build and save patches\n\n"
                "The periodic silence ensures the demo cannot be used\n"
                "for professional production while still allowing full\n"
                "evaluation of the software's capabilities.\n"
            );
            
        default:
            return "Select a chapter from the navigation above.";
    }
}
