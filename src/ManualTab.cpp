// #D:\Workspace\onstage_colosseum_upgrade\src\ManualTab.cpp
// OnStage User Manual - Mini Karaoke DAW / Live Performance FX Host

#include "ManualTab.h"
#include "PluginProcessor.h"

ManualTab::ManualTab(SubterraneumAudioProcessor& p) : processor(p) {
    chapterButtons = { &btnWelcome, &btnRack, &btnMedia, &btnSettings,
                       &btnPlugins, &btnSystemTools, &btnShortcuts,
                       &btnTroubleshooting, &btnDemo };
    
    for (auto* btn : chapterButtons) {
        addAndMakeVisible(btn);
        btn->addListener(this);
    }
    
    addAndMakeVisible(contentView);
    contentView.setMultiLine(true);
    contentView.setReadOnly(true);
    contentView.setScrollbarsShown(true);
    contentView.setCaretVisible(false);
    contentView.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff252525));
    contentView.setColour(juce::TextEditor::textColourId, juce::Colours::lightgrey);
    contentView.setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    contentView.setFont(juce::Font(14.0f));
    
    showChapter(Welcome);
}

void ManualTab::paint(juce::Graphics& g) {
    g.fillAll(Style::colBackground);
    
    g.setColour(juce::Colour(0xff333333));
    g.fillRect(0, 0, getWidth(), 50);
    
    g.setColour(juce::Colours::orange);
    g.setFont(juce::Font(16.0f, juce::Font::bold));
    g.drawText("ONSTAGE USER MANUAL", 10, 10, 250, 30, juce::Justification::centredLeft);
}

void ManualTab::resized() {
    auto area = getLocalBounds();
    
    auto buttonArea = area.removeFromTop(50).reduced(5);
    buttonArea.removeFromLeft(220);
    
    int buttonWidth = 95;
    int gap = 4;
    
    for (auto* btn : chapterButtons) {
        btn->setBounds(buttonArea.removeFromLeft(buttonWidth));
        buttonArea.removeFromLeft(gap);
    }
    
    area.removeFromTop(5);
    contentView.setBounds(area.reduced(10));
}

void ManualTab::buttonClicked(juce::Button* b) {
    if (b == &btnWelcome) showChapter(Welcome);
    else if (b == &btnRack) showChapter(RackView);
    else if (b == &btnMedia) showChapter(Media);
    else if (b == &btnSettings) showChapter(Settings);
    else if (b == &btnPlugins) showChapter(Plugins);
    else if (b == &btnSystemTools) showChapter(SystemTools);
    else if (b == &btnShortcuts) showChapter(Shortcuts);
    else if (b == &btnTroubleshooting) showChapter(Troubleshooting);
    else if (b == &btnDemo) showChapter(DemoLimitations);
}

void ManualTab::showChapter(Chapter chapter) {
    currentChapter = chapter;
    contentView.setText(getChapterContent(chapter));
    contentView.setCaretPosition(0);
    highlightButton(chapter);
}

void ManualTab::highlightButton(Chapter chapter) {
    for (auto* btn : chapterButtons)
        btn->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff444444));
    
    juce::TextButton* activeBtn = nullptr;
    switch (chapter) {
        case Welcome:         activeBtn = &btnWelcome; break;
        case RackView:        activeBtn = &btnRack; break;
        case Media:           activeBtn = &btnMedia; break;
        case Settings:        activeBtn = &btnSettings; break;
        case Plugins:         activeBtn = &btnPlugins; break;
        case SystemTools:     activeBtn = &btnSystemTools; break;
        case Shortcuts:       activeBtn = &btnShortcuts; break;
        case Troubleshooting: activeBtn = &btnTroubleshooting; break;
        case DemoLimitations: activeBtn = &btnDemo; break;
    }
    
    if (activeBtn)
        activeBtn->setColour(juce::TextButton::buttonColourId, juce::Colours::orange.darker());
}

juce::String ManualTab::getChapterContent(Chapter chapter) {
    switch (chapter) {

        // =================================================================
        case Welcome:
        // =================================================================
            return juce::String(
                "===============================================================\n"
                "                    WELCOME TO ONSTAGE\n"
                "===============================================================\n\n"
                "OnStage is a mini karaoke DAW and live performance effects\n"
                "host by Fanan Team. It combines video/audio playback with\n"
                "a powerful effects rack, perfect for singers, performers,\n"
                "and live entertainers.\n\n"
                "HIGHLIGHTS\n"
                "---------------------------------------------------------------\n"
                "* Media Player - VLC-powered video and audio playback\n"
                "* Playlist Management - organize your karaoke tracks\n"
                "* Effects Rack - node-based signal chain for your voice\n"
                "* VST3/VST2/AU Effects - use your favorite effect plugins\n"
                "* Built-in System Tools - connectors, meters, recorder\n"
                "* Container Nodes - save and reuse effect chains\n"
                "* Low-latency ASIO/CoreAudio/JACK engine\n"
                "* Save/Load patches (.ons files)\n"
                "* Favorites browser for quick patch access\n"
                "* CPU and RAM monitors for performance tracking\n\n"
                "DESIGNED FOR LIVE PERFORMANCE\n"
                "---------------------------------------------------------------\n"
                "OnStage focuses on what performers need:\n"
                "* Quick media playback with visual feedback\n"
                "* Real-time effects processing for vocals\n"
                "* Simple, clean interface that works on stage\n"
                "* Reliable audio engine with crash protection\n\n"
                "SYSTEM REQUIREMENTS\n"
                "---------------------------------------------------------------\n"
                "Windows:  10+ (64-bit), ASIO interface recommended\n"
                "macOS:    10.13+, Intel or Apple Silicon\n"
                "Linux:    Ubuntu 20.04+ (64-bit), JACK or ALSA\n"
                "All:      Dual-core CPU, 4 GB RAM (8 GB recommended)\n\n"
                "QUICK START\n"
                "---------------------------------------------------------------\n"
                "1. Settings tab   > select your audio device\n"
                "2. Plugins tab    > scan for your effect plugins\n"
                "3. Media tab      > load your karaoke tracks\n"
                "4. Rack tab       > add effects between Audio In and Out\n"
                "5. Perform!       > play media and sing with effects\n\n"
                "INTERFACE LAYOUT\n"
                "---------------------------------------------------------------\n"
                "Top bar       Logo, CPU/RAM meters, ASIO & REG status LEDs\n"
                "Left panel    Load / Save / Reset / MIDI Panic buttons\n"
                "Main tabs     Rack, Media, Settings, Plugins, Manual, Register\n"
                "Right panel   Plugin Browser (visible on Rack tab)\n"
            );

        // =================================================================
        case RackView:
        // =================================================================
            return juce::String(
                "===============================================================\n"
                "                         RACK VIEW\n"
                "===============================================================\n\n"
                "The Rack is your effects canvas. Every module is a node with\n"
                "input pins on top and output pins on the bottom. Drag wires\n"
                "between them to build your signal chain.\n\n"
                "TYPICAL SIGNAL FLOW\n"
                "---------------------------------------------------------------\n"
                "Audio Input (microphone) -> Effects -> Audio Output (speakers)\n\n"
                "Your voice comes in through the Audio Input node, passes\n"
                "through any effects you add, and goes out through the\n"
                "Audio Output node to your speakers or PA system.\n\n"
                "ADDING EFFECTS\n"
                "---------------------------------------------------------------\n"
                "* Right-click empty space > choose from the Add Node menu\n"
                "  - System Tools submenu: built-in utilities\n"
                "  - Your scanned plugins appear below, grouped by vendor\n"
                "* Or drag from the Plugin Browser panel on the right\n"
                "* Double-click a plugin in the browser to add it\n\n"
                "NODE COLOURS (header bar)\n"
                "---------------------------------------------------------------\n"
                "Pink     Audio Input / Audio Output (hardware I/O)\n"
                "Blue     Effect plugins (reverb, EQ, compressor, etc.)\n"
                "Purple   Connector/Amp (routing and gain)\n"
                "Cyan     Container (nested effect chains)\n"
                "Dark     Other system tools (meters, recorder, etc.)\n\n"
                "CONNECTIONS\n"
                "---------------------------------------------------------------\n"
                "* Drag from an output pin (bottom) to an input pin (top)\n"
                "* Pin colours:\n"
                "  Blue   = Audio\n"
                "  Red    = MIDI\n"
                "* Delete connection: double-click the wire\n"
                "* Right-click wire for info or delete option\n\n"
                "NODE BUTTONS\n"
                "---------------------------------------------------------------\n"
                "E       Open editor (plugin GUI or system tool popup)\n"
                "M       Mute / bypass the effect\n"
                "P       Pass-through (bypass processing)\n"
                "X       Delete node\n\n"
                "CANVAS NAVIGATION\n"
                "---------------------------------------------------------------\n"
                "Left-click + drag on empty space   Pan the canvas\n"
                "Ctrl + Mouse Wheel                 Zoom in/out\n"
                "Shift + Mouse Wheel                Horizontal scroll\n"
                "Mouse Wheel                        Vertical scroll\n"
                "Zoom Slider (bottom left)          Adjust zoom level\n"
                "Double-click zoom slider           Reset to 100%\n\n"
                "MINIMAP\n"
                "---------------------------------------------------------------\n"
                "The minimap in the corner shows a thumbnail overview.\n"
                "Click or drag on the minimap to navigate the canvas.\n\n"
                "PLUGIN BROWSER (Right Panel)\n"
                "---------------------------------------------------------------\n"
                "The Plugin Browser shows three tabs:\n\n"
                "Add Plugins    Browse your scanned effect plugins\n"
                "               Filter by vendor, folder, or format\n"
                "               Search by name\n\n"
                "Favorites      Quick access to saved .ons patches\n"
                "               Set your favorites folder with 'Set Folder'\n"
                "               Double-click to load a patch\n\n"
                "Containers     Browse saved container presets (.onsc)\n"
                "               Drag onto canvas to add a container\n"
            );

        // =================================================================
        case Media:
        // =================================================================
            return juce::String(
                "===============================================================\n"
                "                          MEDIA TAB\n"
                "===============================================================\n\n"
                "The Media tab is your karaoke and playback control center.\n"
                "It features a video display and playlist management.\n\n"
                "VIDEO DISPLAY\n"
                "---------------------------------------------------------------\n"
                "The large area on the left shows video playback.\n"
                "Maintains 16:9 aspect ratio automatically.\n"
                "For audio-only files, shows a dark background.\n\n"
                "TRANSPORT CONTROLS\n"
                "---------------------------------------------------------------\n"
                "PLAY / PAUSE    Start or pause playback (green button)\n"
                "                MIDI Note 15 triggers this remotely\n\n"
                "STOP            Stop playback and reset position (red button)\n"
                "                MIDI Note 16 triggers this remotely\n\n"
                "Progress Bar    Shows playback position\n"
                "                Click or drag to seek\n\n"
                "Time Display    Current time / Total duration\n\n"
                "PLAYLIST (Right Side)\n"
                "---------------------------------------------------------------\n"
                "The playlist panel on the right manages your tracks.\n\n"
                "Adding Tracks:\n"
                "* Click 'Add Files' to add individual media files\n"
                "* Click 'Add Folder' to add all media from a folder\n"
                "* Supported formats: MP4, AVI, MKV, MP3, WAV, FLAC, etc.\n\n"
                "Managing Tracks:\n"
                "* Click a track to select it\n"
                "* Double-click to play immediately\n"
                "* Drag tracks to reorder\n"
                "* Right-click for context menu (remove, move up/down)\n\n"
                "MIDI REMOTE CONTROL\n"
                "---------------------------------------------------------------\n"
                "Control media playback from your MIDI controller:\n\n"
                "  MIDI Note 15    Play / Pause toggle\n"
                "  MIDI Note 16    Stop\n\n"
                "This allows hands-free control during performance.\n\n"
                "AUDIO ROUTING\n"
                "---------------------------------------------------------------\n"
                "Media playback audio is routed through the Rack.\n"
                "You can add effects to your backing tracks by placing\n"
                "them in the signal chain.\n\n"
                "TIPS FOR PERFORMERS\n"
                "---------------------------------------------------------------\n"
                "* Pre-load your setlist in order\n"
                "* Use MIDI notes to control playback without touching the computer\n"
                "* Keep video files at reasonable resolution for smooth playback\n"
                "* Test your setup before going on stage\n"
            );

        // =================================================================
        case Settings:
        // =================================================================
            return juce::String(
                "===============================================================\n"
                "                         SETTINGS\n"
                "===============================================================\n\n"
                "All audio and MIDI configuration lives here.\n\n"
                "AUDIO DEVICE\n"
                "---------------------------------------------------------------\n"
                "Select your audio interface from the dropdown.\n\n"
                "Windows:   ASIO drivers recommended for low latency\n"
                "macOS:     CoreAudio (built-in or external interface)\n"
                "Linux:     JACK or ALSA\n\n"
                "Control Panel (Windows ASIO only):\n"
                "Opens your ASIO driver's control panel to adjust\n"
                "sample rate and buffer size.\n\n"
                "STATUS LINE\n"
                "---------------------------------------------------------------\n"
                "Shows the active driver, sample rate, buffer size, and\n"
                "calculated latency (input + output in milliseconds).\n\n"
                "MIDI DEVICES\n"
                "---------------------------------------------------------------\n"
                "MIDI Inputs (Red Frame):\n"
                "Lists all connected MIDI input devices.\n"
                "Each device has a channel filter:\n"
                "  OFF     Device disabled\n"
                "  ALL     All 16 channels pass through\n"
                "  CH X    Single channel enabled\n\n"
                "Click the filter button to open the channel selector.\n\n"
                "MIDI Outputs (Gold Frame):\n"
                "Same layout as inputs. Use for external MIDI devices.\n\n"
                "Reconnect MIDI Devices:\n"
                "Re-enumerates all MIDI devices without restarting.\n"
                "Use when you plug in or power-cycle a MIDI controller.\n\n"
                "FOLDER SETTINGS\n"
                "---------------------------------------------------------------\n"
                "Set Recording Folder:\n"
                "Default folder where Recorder modules save files.\n\n"
                "DEFAULT PATCH (Autoload)\n"
                "---------------------------------------------------------------\n"
                "Save as Default (green button):\n"
                "Saves the current state as the startup patch.\n"
                "This patch loads automatically when OnStage starts.\n\n"
                "Clear Default (red button):\n"
                "Removes the saved default so OnStage starts empty.\n\n"
                "The default patch is stored as OnStage_Default.ons in\n"
                "your OS application data folder.\n\n"
                "ASIO LED (Top Right)\n"
                "---------------------------------------------------------------\n"
                "Green    Audio engine running normally\n"
                "Red      No audio device or error\n\n"
                "If the LED is red, check your audio device settings.\n"
            );

        // =================================================================
        case Plugins:
        // =================================================================
            return juce::String(
                "===============================================================\n"
                "                          PLUGINS\n"
                "===============================================================\n\n"
                "OnStage hosts effect plugins in VST3, VST2, and AudioUnit\n"
                "formats. The Plugins tab manages scanning and organization.\n\n"
                "IMPORTANT: OnStage is an effects-only host. Instrument\n"
                "plugins (synths, samplers) are filtered out automatically.\n\n"
                "SCANNING PLUGINS\n"
                "---------------------------------------------------------------\n"
                "Scan All Plugins:\n"
                "Performs a full scan of all plugin folders.\n"
                "Uses safe out-of-process scanning - if a plugin crashes\n"
                "during scan, OnStage continues without freezing.\n\n"
                "Rescan Existing:\n"
                "Re-scans only plugins already in the list.\n"
                "Faster than a full scan, useful for plugin updates.\n\n"
                "PLUGIN FOLDERS\n"
                "---------------------------------------------------------------\n"
                "Click 'Plugin Folders...' to manage search paths.\n\n"
                "Default locations are scanned automatically:\n"
                "  Windows VST3:  C:\\Program Files\\Common Files\\VST3\n"
                "  macOS VST3:    /Library/Audio/Plug-Ins/VST3\n"
                "  macOS AU:      /Library/Audio/Plug-Ins/Components\n\n"
                "Add custom folders if your plugins are elsewhere.\n\n"
                "PLUGIN LIST\n"
                "---------------------------------------------------------------\n"
                "The tree view shows all scanned effect plugins.\n\n"
                "Sort options:\n"
                "  Type     Group by effect type\n"
                "  Vendor   Group by manufacturer\n\n"
                "Eye Icon (per plugin):\n"
                "Toggle visibility in the Add Node menus.\n"
                "Hidden plugins won't appear in the Rack context menu.\n\n"
                "X Button (per plugin):\n"
                "Remove the plugin from the list.\n"
                "The plugin file is NOT deleted, just unregistered.\n\n"
                "MANUAL PLUGIN LOADING\n"
                "---------------------------------------------------------------\n"
                "If a plugin isn't found by scanning, you can load it\n"
                "manually from the System Tools menu:\n\n"
                "  VST2 Plugin...   Browse for a .dll (Win) or .vst (Mac)\n"
                "  VST3 Plugin...   Browse for a .vst3 bundle\n\n"
                "BLACKLIST\n"
                "---------------------------------------------------------------\n"
                "Plugins that crash during scanning are blacklisted.\n"
                "View the blacklist with 'Blacklist...' button.\n"
                "Reset the blacklist to retry scanning problem plugins.\n"
            );

        // =================================================================
        case SystemTools:
        // =================================================================
            return juce::String(
                "===============================================================\n"
                "                       SYSTEM TOOLS\n"
                "===============================================================\n\n"
                "System tools are built-in utility modules for routing,\n"
                "metering, and recording. Add them from the right-click\n"
                "menu > System Tools, or from the Plugin Browser.\n\n\n"
                "CONNECTOR / AMP\n"
                "---------------------------------------------------------------\n"
                "A simple audio routing node with volume control.\n"
                "Purple colored module.\n\n"
                "  Volume Slider    Drag to adjust gain\n"
                "                   Center (50%) = unity gain (0 dB)\n"
                "                   Max = +35 dB boost\n"
                "                   Min = silence\n\n"
                "  M button         Mute toggle\n"
                "  X button         Delete node\n\n"
                "Use for: signal routing, gain staging, creating\n"
                "parallel paths, summing multiple sources.\n\n\n"
                "STEREO METER\n"
                "---------------------------------------------------------------\n"
                "Visual level meter showing stereo audio levels.\n"
                "Displays peak and RMS values.\n\n"
                "  Green            Safe levels\n"
                "  Yellow           Getting hot\n"
                "  Red              Clipping risk\n\n"
                "Place anywhere in your chain to monitor levels.\n"
                "The meter is pass-through - audio is not affected.\n\n\n"
                "RECORDER\n"
                "---------------------------------------------------------------\n"
                "Records audio to disk in real-time.\n"
                "Saves WAV files (24-bit, current sample rate).\n\n"
                "  Red Circle       Start recording\n"
                "  Blue Square      Stop and save\n"
                "  Folder Icon      Open recordings folder\n"
                "  Name Box         Edit filename prefix\n"
                "  SYNC / IND       Toggle linked recording mode\n\n"
                "SYNC Mode:\n"
                "When multiple recorders are in SYNC mode, starting\n"
                "one starts all of them. Same for stopping.\n\n"
                "Files are saved to the recording folder set in Settings.\n"
                "Default: Documents/OnStage/recordings\n\n\n"
                "TRANSIENT SPLITTER\n"
                "---------------------------------------------------------------\n"
                "Splits audio into transient (attack) and sustain (body)\n"
                "components on separate outputs.\n\n"
                "  Outputs 1-2      Transient (attack) L/R\n"
                "  Outputs 3-4      Sustain (body) L/R\n\n"
                "  E button         Open editor with full controls\n"
                "  X button         Delete node\n\n"
                "Editor Controls:\n"
                "  Sensitivity      How easily transients are detected\n"
                "  Decay            Gate close speed (1-500 ms)\n"
                "  Hold Time        Minimum gate open time (0-100 ms)\n"
                "  Smoothing        Prevents clicks\n"
                "  Transient Gain   Level for transient outputs\n"
                "  Sustain Gain     Level for sustain outputs\n\n"
                "Use for: parallel compression, drum shaping, creative\n"
                "sound design, adding effects to attack or sustain only.\n\n\n"
                "CONTAINER\n"
                "---------------------------------------------------------------\n"
                "A nested effect chain inside a single node.\n"
                "Cyan colored module.\n\n"
                "Containers let you:\n"
                "* Group multiple effects into a single reusable unit\n"
                "* Save effect chains as presets (.onsc files)\n"
                "* Load container presets from the Plugin Browser\n"
                "* Create complex routing inside the container\n\n"
                "  E button         Open container editor\n"
                "                   (shows the inner graph)\n"
                "  +/- buttons      Add/remove I/O buses\n"
                "  Volume Slider    Container output level\n"
                "  M button         Mute\n"
                "  X button         Delete\n\n"
                "Container Editor:\n"
                "Double-click a container to edit its contents.\n"
                "The editor shows Input and Output nodes.\n"
                "Wire effects between them just like the main Rack.\n\n"
                "Saving Presets:\n"
                "Right-click the container > Save Preset\n"
                "Presets are saved to Documents/OnStage/containers\n\n\n"
                "VST2 PLUGIN... / VST3 PLUGIN...\n"
                "---------------------------------------------------------------\n"
                "Manually load a plugin from a file.\n"
                "Use when a plugin isn't found by automatic scanning.\n\n"
                "  VST2 Plugin...   Browse for .dll (Win) or .vst (Mac)\n"
                "  VST3 Plugin...   Browse for .vst3 bundle\n"
            );

        // =================================================================
        case Shortcuts:
        // =================================================================
            return juce::String(
                "===============================================================\n"
                "                        SHORTCUTS\n"
                "===============================================================\n\n"
                "RACK VIEW\n"
                "---------------------------------------------------------------\n"
                "Right-click empty space     Add Node menu\n"
                "Right-click a pin           Pin info tooltip\n"
                "Right-click a wire          Wire info / delete option\n"
                "Right-click a button        Button tooltip\n"
                "Double-click a wire         Delete connection\n"
                "Double-click effect node    Open plugin editor\n"
                "Double-click container      Open container editor\n"
                "Drag pin to pin             Create connection\n"
                "Drag a node                 Move it on the canvas\n\n"
                "NODE BUTTONS\n"
                "---------------------------------------------------------------\n"
                "E       Open editor (plugin GUI or tool popup)\n"
                "M       Mute / bypass\n"
                "P       Pass-through (effects only)\n"
                "X       Delete node\n\n"
                "CANVAS NAVIGATION\n"
                "---------------------------------------------------------------\n"
                "Left-click + drag empty     Pan the canvas\n"
                "Ctrl + Mouse Wheel          Zoom in/out (at cursor)\n"
                "Shift + Mouse Wheel         Horizontal scroll\n"
                "Mouse Wheel                 Vertical scroll\n"
                "Click minimap               Jump to position\n"
                "Drag minimap                Pan view\n"
                "Double-click zoom slider    Reset to 100%\n\n"
                "MEDIA TAB\n"
                "---------------------------------------------------------------\n"
                "Click track                 Select track\n"
                "Double-click track          Play immediately\n"
                "Drag track                  Reorder in playlist\n"
                "Right-click track           Context menu\n\n"
                "MIDI NOTES (Media Control)\n"
                "---------------------------------------------------------------\n"
                "Note 15                     Play / Pause toggle\n"
                "Note 16                     Stop\n\n"
                "LEFT PANEL\n"
                "---------------------------------------------------------------\n"
                "Load         Open a saved .ons patch\n"
                "Save         Save current session\n"
                "Reset        Clear everything and start fresh\n"
                "MIDI Panic   Send all-notes-off to all effects\n\n"
                "PLUGIN BROWSER\n"
                "---------------------------------------------------------------\n"
                "Add Plugins      Standard plugin list\n"
                "Favorites        Browse saved .ons patches\n"
                "Containers       Browse container presets\n"
                "Set Folder...    Choose favorites folder\n"
                "Search box       Filter by name\n"
                "Double-click     Add to rack / load patch\n"
                "Drag to canvas   Add at specific position\n\n"
                "SETTINGS TAB\n"
                "---------------------------------------------------------------\n"
                "Save as Default  Save startup patch\n"
                "Clear Default    Remove startup patch\n"
            );

        // =================================================================
        case Troubleshooting:
        // =================================================================
            return juce::String(
                "===============================================================\n"
                "                     TROUBLESHOOTING\n"
                "===============================================================\n\n"
                "NO SOUND\n"
                "---------------------------------------------------------------\n"
                "1. Check ASIO LED (top right) - should be green\n"
                "2. Settings tab: verify audio device is selected\n"
                "3. Check that effects are connected to Audio Output\n"
                "4. Make sure nodes aren't muted (M button)\n"
                "5. Check volume on Connector/Amp nodes\n"
                "6. Verify your audio interface is working\n\n"
                "NO MIDI RESPONSE\n"
                "---------------------------------------------------------------\n"
                "1. Settings tab: verify MIDI device is listed\n"
                "2. Check channel filter (should show ALL or your channel)\n"
                "3. If device disconnected: hit 'Reconnect MIDI Devices'\n"
                "4. Check your MIDI controller is sending on the right channel\n\n"
                "VIDEO NOT PLAYING\n"
                "---------------------------------------------------------------\n"
                "1. Check the file format is supported\n"
                "2. Try a different video file to rule out corruption\n"
                "3. On Windows: ensure VLC DLLs are present\n"
                "4. On macOS: media playback uses AVFoundation\n\n"
                "PLUGIN CRASHES DURING SCAN\n"
                "---------------------------------------------------------------\n"
                "OnStage uses out-of-process scanning for safety.\n"
                "If a plugin crashes, it's automatically skipped.\n\n"
                "1. Check the blacklist (Plugins tab > Blacklist...)\n"
                "2. Remove the problem plugin from your system\n"
                "3. Or reset the blacklist to retry scanning\n\n"
                "PLUGIN WON'T LOAD\n"
                "---------------------------------------------------------------\n"
                "1. Make sure it's a 64-bit plugin (32-bit not supported)\n"
                "2. Make sure it's an EFFECT, not an instrument\n"
                "3. Try manual loading: System Tools > VST3 Plugin...\n"
                "4. Check the plugin works in another host\n\n"
                "HIGH CPU USAGE\n"
                "---------------------------------------------------------------\n"
                "1. Check the CPU meter in the top bar\n"
                "2. Remove or bypass heavy plugins\n"
                "3. Increase buffer size in your ASIO driver\n"
                "4. Close other applications\n"
                "5. Check for plugins with known CPU issues\n\n"
                "AUDIO GLITCHES / POPS\n"
                "---------------------------------------------------------------\n"
                "1. Increase ASIO buffer size\n"
                "2. Close background applications\n"
                "3. Disable WiFi and Bluetooth if not needed\n"
                "4. Check your audio interface drivers are up to date\n"
                "5. On Windows: disable power saving for USB\n\n"
                "CRASH ON STARTUP\n"
                "---------------------------------------------------------------\n"
                "OnStage has crash protection. If it crashed before,\n"
                "it will start in Safe Mode.\n\n"
                "If crashes persist:\n"
                "1. Delete the settings file to reset\n"
                "   Windows: %APPDATA%/Fanan/OnStage\n"
                "   macOS:   ~/Library/Application Support/Fanan/OnStage\n"
                "2. Reinstall OnStage\n"
                "3. Contact Fanan Team support\n"
            );

        // =================================================================
        case DemoLimitations:
        // =================================================================
            return juce::String(
                "===============================================================\n"
                "                    DEMO / REGISTRATION\n"
                "===============================================================\n\n"
                "DEMO MODE\n"
                "---------------------------------------------------------------\n"
                "OnStage runs in demo mode until you register.\n\n"
                "Demo limitations:\n"
                "* Audio output mutes for 3 seconds every 5 minutes\n"
                "* A reminder dialog appears periodically\n"
                "* All features are otherwise fully functional\n\n"
                "The demo lets you evaluate OnStage completely before\n"
                "purchasing. There's no time limit on the demo.\n\n"
                "REGISTRATION\n"
                "---------------------------------------------------------------\n"
                "To register OnStage:\n\n"
                "1. Purchase a license from Fanan Team\n"
                "2. You'll receive a serial number via email\n"
                "3. Go to the Register tab in OnStage\n"
                "4. Enter your serial number\n"
                "5. Click 'Register'\n\n"
                "Once registered:\n"
                "* No more audio muting\n"
                "* No more reminder dialogs\n"
                "* License is tied to your computer\n\n"
                "REGISTRATION LED\n"
                "---------------------------------------------------------------\n"
                "The REG LED in the top-right corner shows status:\n\n"
                "  Green    Registered - full version active\n"
                "  Red      Demo mode - limitations apply\n\n"
                "MOVING YOUR LICENSE\n"
                "---------------------------------------------------------------\n"
                "Your license is tied to your computer's hardware ID.\n"
                "If you need to move it to a new computer:\n\n"
                "1. Contact Fanan Team support\n"
                "2. Provide your original serial number\n"
                "3. We'll issue a new key for your new hardware\n\n"
                "SUPPORT\n"
                "---------------------------------------------------------------\n"
                "For technical support, licensing questions, or feedback:\n\n"
                "Email:    support@fanan.team\n"
                "Website:  www.fanan.team\n\n"
                "Thank you for using OnStage!\n"
            );

        default:
            return "Chapter not found.";
    }
}
