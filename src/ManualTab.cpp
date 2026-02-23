#include "ManualTab.h"
#include "PluginProcessor.h"

ManualTab::ManualTab(SubterraneumAudioProcessor& p) : processor(p) {
    chapterButtons = { &btnIntro, &btnRack, &btnWorkspaces, &btnMixer,
                       &btnSettings, &btnPlugins, &btnSystemTools, 
                       &btnSampling, &btnShortcuts, &btnTroubleshooting, &btnDemo };
    
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
    
    showChapter(Introduction);
}

void ManualTab::paint(juce::Graphics& g) {
    g.fillAll(Style::colBackground);
    
    g.setColour(juce::Colour(0xff333333));
    g.fillRect(0, 0, getWidth(), 50);
    
    g.setColour(juce::Colours::gold);
    g.setFont(juce::Font(16.0f, juce::Font::bold));
    g.drawText("COLOSSEUM USER MANUAL", 10, 10, 250, 30, juce::Justification::centredLeft);
}

void ManualTab::resized() {
    auto area = getLocalBounds();
    
    auto buttonArea = area.removeFromTop(50).reduced(5);
    buttonArea.removeFromLeft(260);
    
    int buttonWidth = 85;
    int gap = 4;
    
    for (auto* btn : chapterButtons) {
        btn->setBounds(buttonArea.removeFromLeft(buttonWidth));
        buttonArea.removeFromLeft(gap);
    }
    
    area.removeFromTop(5);
    contentView.setBounds(area.reduced(10));
}

void ManualTab::buttonClicked(juce::Button* b) {
    if (b == &btnIntro) showChapter(Introduction);
    else if (b == &btnRack) showChapter(RackView);
    else if (b == &btnWorkspaces) showChapter(Workspaces);
    else if (b == &btnMixer) showChapter(Mixer);
    else if (b == &btnSettings) showChapter(Settings);
    else if (b == &btnPlugins) showChapter(Plugins);
    else if (b == &btnSystemTools) showChapter(SystemTools);
    else if (b == &btnSampling) showChapter(Sampling);
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
        case Introduction:    activeBtn = &btnIntro; break;
        case RackView:        activeBtn = &btnRack; break;
        case Workspaces:      activeBtn = &btnWorkspaces; break;
        case Mixer:           activeBtn = &btnMixer; break;
        case Settings:        activeBtn = &btnSettings; break;
        case Plugins:         activeBtn = &btnPlugins; break;
        case SystemTools:     activeBtn = &btnSystemTools; break;
        case Sampling:        activeBtn = &btnSampling; break;
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
        case Introduction:
        // =================================================================
            return juce::String(
                "===============================================================\n"
                "                    WELCOME TO COLOSSEUM\n"
                "===============================================================\n\n"
                "Colosseum is a modular mini-DAW for hosting audio plugins,\n"
                "building signal chains, and performing live. Think of it as\n"
                "a virtual pedalboard on steroids: you wire instruments,\n"
                "effects, recorders, samplers, and tools together any way\n"
                "you like, then switch between 16 independent workspaces\n"
                "on the fly.\n\n"
                "HIGHLIGHTS\n"
                "---------------------------------------------------------------\n"
                "* Node-based Rack - drag, drop, and connect anything\n"
                "* 16 Workspaces - instant preset switching, each a full session\n"
                "* Multi-format plugins - VST3, VST2, AudioUnit, LADSPA\n"
                "* Built-in system tools - meters, recorder, MIDI player,\n"
                "  samplers, CC step sequencer, transient splitter, Latcher\n"
                "* 32 instrument slots with MIDI-controllable switching\n"
                "* Low-latency ASIO / CoreAudio / JACK engine\n"
                "* Mixer view with per-channel gain and metering\n"
                "* Floating Mixer - detach the mixer into its own window\n"
                "* Magnetic Node Snap - connected nodes align automatically\n"
                "* Favorite Patches - quick access to your .subt presets\n"
                "* Default Settings - auto-load your preferred startup patch\n"
                "* CPU and RAM monitors for real-time performance tracking\n\n"
                "SYSTEM REQUIREMENTS\n"
                "---------------------------------------------------------------\n"
                "Windows:  10+ (64-bit), ASIO interface (or ASIO4ALL)\n"
                "macOS:    10.13+, Intel or Apple Silicon\n"
                "Linux:    Ubuntu 20.04+ (64-bit), JACK or ALSA\n"
                "All:      Dual-core CPU, 4 GB RAM (8 GB recommended)\n\n"
                "QUICK START (5 minutes to sound)\n"
                "---------------------------------------------------------------\n"
                "1. Settings tab   > select your audio device\n"
                "2. Plugins tab    > click Scan Plugins\n"
                "3. Rack tab       > right-click > System Tools or plugins to add\n"
                "4. Wire things up > drag from output pins to input pins\n"
                "5. Play!          > use the virtual keyboard (Keys button)\n"
                "                    or connect a MIDI controller\n\n"
                "INTERFACE LAYOUT\n"
                "---------------------------------------------------------------\n"
                "Top bar       Logo, CPU/RAM meters, ASIO & REG status LEDs\n"
                "Workspaces    16 numbered slots just below the header\n"
                "Main tabs     Rack, Mixer, Settings, Plugins, Manual, Register\n"
                "Left panel    Load / Save / Reset / Keys / MIDI Panic /\n"
                "              Floating Mixer\n"
                "Right panel   Plugin Browser (visible on Rack tab)\n"
                "Bottom bar    32-slot Instrument Selector with Multi-Mode\n"
            );

        // =================================================================
        case RackView:
        // =================================================================
            return juce::String(
                "===============================================================\n"
                "                         RACK VIEW\n"
                "===============================================================\n\n"
                "The Rack is your main canvas. Every module is a node with\n"
                "input pins on top and output pins on the bottom. Drag wires\n"
                "between them to build your signal chain.\n\n"
                "ADDING MODULES\n"
                "---------------------------------------------------------------\n"
                "* Right-click empty space > choose from the Add Node menu\n"
                "  - System Tools submenu: all built-in utilities\n"
                "  - Your scanned plugins appear below, grouped by vendor\n"
                "* Or drag from the Plugin Browser panel on the right\n\n"
                "NODE COLOURS (header bar)\n"
                "---------------------------------------------------------------\n"
                "Pink     Audio Input / Audio Output (hardware I/O)\n"
                "Red      MIDI Input / MIDI Output\n"
                "Green    Instruments (synths, samplers)\n"
                "Blue     Effects (reverb, EQ, compressor, etc.)\n"
                "Dark     System tools (connectors, meters, etc.)\n\n"
                "CONNECTIONS\n"
                "---------------------------------------------------------------\n"
                "* Drag from an output pin (bottom) to an input pin (top)\n"
                "* Pin colours:\n"
                "  Blue   = Audio\n"
                "  Red    = MIDI\n"
                "  Orange = Sidechain\n"
                "* Double-click a wire to delete it\n"
                "* Right-click a wire for info or delete option\n\n"
                "NODE BUTTONS\n"
                "---------------------------------------------------------------\n"
                "E   Open the plugin/tool editor\n"
                "CH  MIDI channel selector (instruments only)\n"
                "M   Mute / Bypass toggle\n"
                "P   Pass-through mode (effects only - audio passes even\n"
                "    when bypassed, useful for sidechain routing)\n"
                "X   Delete module\n\n"
                "Right-click any button for a tooltip describing its function.\n\n"
                "I/O NODES\n"
                "---------------------------------------------------------------\n"
                "The four I/O nodes (Audio In, Audio Out, MIDI In, MIDI Out)\n"
                "are always present. Each has an ON/OFF toggle:\n"
                "* Click the toggle on Audio Input to enable live audio in\n"
                "* Click the toggle on Audio Output to enable playback\n"
                "* MIDI In/Out route hardware MIDI to/from your graph\n\n"
                "PLUGIN BROWSER (right panel)\n"
                "---------------------------------------------------------------\n"
                "The browser shows all your scanned plugins and system tools.\n"
                "* Type in the search box to filter by name\n"
                "* Filter by: All, Instruments, Effects, or Tools\n"
                "* Drag any item onto the rack to add it\n"
                "* Double-click to add at default position\n"
                "* Plugins hidden via the eye toggle in Plugin Manager\n"
                "  are filtered out of both the browser and right-click menu\n\n"
                "FAVORITES MODE\n"
                "---------------------------------------------------------------\n"
                "The plugin browser has two mode buttons at the top:\n"
                "  Add Plugins    Standard plugin browser (cyan highlight)\n"
                "  Favorites      Browse your favorite .subt patches (gold)\n\n"
                "In Favorites mode:\n"
                "* Click 'Set Folder...' to choose a folder of .subt patches\n"
                "* All .subt files in the folder (including subfolders) are\n"
                "  listed with a gold star icon\n"
                "* Use the search box to filter by filename\n"
                "* Double-click a patch to load it. A popup lets you choose\n"
                "  which workspace (1-16) to load it into, or 'Current'\n"
                "  to load into the active workspace\n"
                "* The favorites folder is remembered between sessions\n\n"
                "MAGNETIC NODE SNAP\n"
                "---------------------------------------------------------------\n"
                "When dragging a node that is connected to other nodes,\n"
                "it will magnetically snap into alignment:\n"
                "  Horizontal   Top or center aligns with connected nodes\n"
                "  Vertical     Right edge snaps to left edge (chain layout)\n"
                "  Stacking     Left edges align for vertical stacking\n\n"
                "Snap threshold is 20 pixels. Only connected nodes participate.\n"
                "Hold Shift while dragging to disable snap temporarily.\n"
            );

        // =================================================================
        case Workspaces:
        // =================================================================
            return juce::String(
                "===============================================================\n"
                "                        WORKSPACES\n"
                "===============================================================\n\n"
                "Colosseum has 16 independent workspaces. Each workspace is\n"
                "a complete session: its own nodes, connections, and plugin\n"
                "states. Think of them as 16 pedalboard presets you can\n"
                "switch between instantly.\n\n"
                "The workspace bar sits just below the main header.\n"
                "The label 'Workspaces' is on the left, followed by 16\n"
                "numbered buttons.\n\n"
                "BUTTON COLOURS\n"
                "---------------------------------------------------------------\n"
                "Bright (active)    Currently loaded workspace\n"
                "Dim (occupied)     Has content but not loaded\n"
                "Dark (empty)       No content saved\n"
                "Gray (disabled)    Workspace is disabled\n\n"
                "SWITCHING\n"
                "---------------------------------------------------------------\n"
                "Click any enabled workspace button to switch to it.\n"
                "Colosseum saves the current workspace automatically before\n"
                "loading the target, so you never lose your work.\n\n"
                "RIGHT-CLICK MENU\n"
                "---------------------------------------------------------------\n"
                "Right-click any workspace button for options:\n\n"
                "Enable / Disable\n"
                "  Disabled workspaces are grayed out and can't be selected.\n"
                "  The active workspace cannot be disabled.\n\n"
                "Rename\n"
                "  Give the workspace a custom name (up to 15 characters).\n"
                "  The name appears as the button label.\n\n"
                "Clear\n"
                "  Wipes all nodes and connections from that workspace.\n"
                "  If it's the currently active workspace, the rack resets.\n\n"
                "Duplicate to...\n"
                "  Copies the entire workspace (nodes, connections, plugin\n"
                "  states) to another workspace slot. Great for creating\n"
                "  variations of a setup.\n\n"
                "PRESETS & WORKSPACES\n"
                "---------------------------------------------------------------\n"
                "When you Save a patch, all 16 workspaces are saved together.\n"
                "Loading a patch restores all workspaces and switches to the\n"
                "one that was active when saved.\n\n"
                "The Reset button in the left panel clears all 16 workspaces.\n\n"
                "TIP: Use workspaces for song sections (verse / chorus),\n"
                "different instrument combinations, or A/B comparisons.\n"
            );

        // =================================================================
        case Mixer:
        // =================================================================
            return juce::String(
                "===============================================================\n"
                "                          MIXER\n"
                "===============================================================\n\n"
                "The Mixer tab gives you per-channel level control for every\n"
                "plugin in your signal chain.\n\n"
                "CHANNEL STRIPS\n"
                "---------------------------------------------------------------\n"
                "Each strip shows:\n"
                "* Plugin name at top\n"
                "* Vertical fader (drag to adjust)\n"
                "* dB value readout\n"
                "* Stereo level meter on the right side\n\n"
                "FADER RANGE\n"
                "---------------------------------------------------------------\n"
                "-inf dB (silence) to +12 dB (boost)\n"
                "Default position is 0 dB (unity gain).\n\n"
                "LEVEL METERS\n"
                "---------------------------------------------------------------\n"
                "Green     Safe levels\n"
                "Yellow    Getting hot (approaching 0 dB)\n"
                "Red       Clipping risk - turn it down!\n\n"
                "Only channels with active audio routing are displayed.\n"
                "The master output channel is always visible.\n\n"
                "FLOATING MIXER\n"
                "---------------------------------------------------------------\n"
                "The 'Floating Mixer' button in the left panel (dark yellow,\n"
                "two-line label) detaches the mixer into a separate resizable\n"
                "window. This lets you view the mixer alongside the rack.\n\n"
                "  Floating Mixer   Detach mixer to a floating window\n"
                "  Dock Mixer       Return the mixer to its normal tab\n\n"
                "The floating window defaults to 900x500 pixels and can be\n"
                "resized from 400x200 up to your screen size. Closing the\n"
                "window automatically docks the mixer back.\n"
            );

        // =================================================================
        case Settings:
        // =================================================================
            return juce::String(
                "===============================================================\n"
                "                         SETTINGS\n"
                "===============================================================\n\n"
                "All audio, MIDI, and timing configuration lives here.\n\n"
                "DRIVER SETTINGS\n"
                "---------------------------------------------------------------\n"
                "Audio Device       Select your audio interface\n"
                "                   (ASIO on Windows, CoreAudio on macOS,\n"
                "                    JACK/ALSA on Linux)\n"
                "Control Panel      Opens the ASIO driver panel (Windows)\n"
                "Reconnect MIDI     Re-enumerates all MIDI devices without\n"
                "                   restarting the app. Use when you plug in\n"
                "                   or power-cycle a MIDI controller.\n"
                "Set Recording      Default folder for Recorder modules\n"
                "  Folder\n"
                "Set Sampler        Default folder for sampling tools\n"
                "  Folder\n\n"
                "STATUS LINE\n"
                "---------------------------------------------------------------\n"
                "Shows the active driver, sample rate, buffer size, and\n"
                "calculated latency (input + output + total in ms).\n\n"
                "MASTER TEMPO\n"
                "---------------------------------------------------------------\n"
                "* BPM slider: 20 - 300 BPM\n"
                "* TAP button: tap repeatedly to set tempo\n"
                "* Tempo value display (large orange text)\n"
                "* The master tempo is available to sync-aware tools\n"
                "  like the CC Step Sequencer\n\n"
                "TIME SIGNATURE\n"
                "---------------------------------------------------------------\n"
                "Set numerator (1-16) and denominator (2, 4, 8, 16).\n"
                "Displays the current signature in large text.\n\n"
                "METRONOME\n"
                "---------------------------------------------------------------\n"
                "Enable toggle and volume slider.\n\n"
                "MIDI INPUTS (Red Frame)\n"
                "---------------------------------------------------------------\n"
                "Lists all connected MIDI input devices.\n"
                "Each has a channel filter button:\n"
                "  OFF     Device disabled\n"
                "  ALL     All 16 channels pass through\n"
                "  CH X    Single channel enabled\n"
                "  X CH    Multiple specific channels enabled\n\n"
                "Click the button to open the channel selector popup.\n\n"
                "MIDI OUTPUTS (Gold Frame)\n"
                "---------------------------------------------------------------\n"
                "Same layout as inputs. Useful for sending MIDI to external\n"
                "synths, drum machines, or lighting controllers.\n\n"
                "TIP: If a MIDI device disconnects and reconnects (USB\n"
                "cable bump, power cycle), hit 'Reconnect MIDI Devices'\n"
                "instead of restarting Colosseum.\n\n"
                "DEFAULT SETTINGS (Autoload)\n"
                "---------------------------------------------------------------\n"
                "Two buttons at the bottom of the Settings tab let you set\n"
                "a default startup patch:\n\n"
                "  Save as Default (green)\n"
                "    Saves the entire current state — all 16 workspaces,\n"
                "    the graph, audio settings — as the default patch.\n"
                "    This patch loads automatically every time Colosseum\n"
                "    starts. Right-click this button for a description.\n\n"
                "  Clear Default (red)\n"
                "    Removes the saved default so Colosseum starts empty.\n\n"
                "The default patch is stored as Colosseum_Default.subt in\n"
                "your OS application data folder.\n"
            );

        // =================================================================
        case Plugins:
        // =================================================================
            return juce::String(
                "===============================================================\n"
                "                          PLUGINS\n"
                "===============================================================\n\n"
                "Manage your plugin library here.\n\n"
                "SCANNING\n"
                "---------------------------------------------------------------\n"
                "Click 'Scan Plugins' and choose:\n"
                "  Scan for New   Only scans plugins not already known\n"
                "  Rescan All     Clears the list and rescans everything\n\n"
                "Scanning runs in a separate process for crash protection.\n"
                "If a plugin crashes the scanner, you'll be asked to\n"
                "blacklist it on next launch.\n\n"
                "PLUGIN LIST\n"
                "---------------------------------------------------------------\n"
                "* Sort by Type or Vendor\n"
                "* Filter: All, Instruments, Effects\n"
                "* +/- buttons expand or collapse all groups\n"
                "* Search box at top for quick filtering\n\n"
                "EYE TOGGLE (hide/show)\n"
                "---------------------------------------------------------------\n"
                "Each plugin has an eye icon in the Plugin Manager.\n"
                "Click it to hide a plugin from the Rack's right-click menu\n"
                "and the Plugin Browser panel. The plugin stays scanned, but\n"
                "won't clutter your workflow. Toggle it back to restore.\n\n"
                "PLUGIN FOLDERS\n"
                "---------------------------------------------------------------\n"
                "Click 'Plugin Folders...' to manage search paths.\n"
                "Use 'Add Defaults' to add standard OS locations.\n\n"
                "BLACKLIST\n"
                "---------------------------------------------------------------\n"
                "'Reset Blacklist' clears all blocked plugins so they can\n"
                "be scanned again. Use after updating a problematic plugin.\n\n"
                "SUPPORTED FORMATS\n"
                "---------------------------------------------------------------\n"
                "VST3           All platforms (recommended)\n"
                "VST2           Windows (requires manual .dll load)\n"
                "AudioUnit      macOS only\n"
                "LADSPA         Linux only\n\n"
                "VST2 NOTE: VST2 plugins can be loaded via the System Tools\n"
                "submenu > 'VST2 Plugin...' which opens a file browser to\n"
                "select the .dll directly.\n"
            );

        // =================================================================
        case SystemTools:
        // =================================================================
            return juce::String(
                "===============================================================\n"
                "                       SYSTEM TOOLS\n"
                "===============================================================\n\n"
                "Built-in utility modules. Add via right-click > System Tools,\n"
                "or drag from the Plugin Browser's Tools section.\n\n"
                "All system tools are saved/restored with workspaces and\n"
                "presets, including their full state.\n\n\n"
                "CONNECTOR (Bus)\n"
                "---------------------------------------------------------------\n"
                "A stereo summing point with gain control.\n\n"
                "  Volume Knob   Drag up/down: -inf to +25 dB\n"
                "                Green dot at center = unity (0 dB)\n"
                "  M button      Mute toggle (yellow when muted)\n\n"
                "Use for: submixes, gain staging, summing parallel chains.\n\n\n"
                "STEREO METER\n"
                "---------------------------------------------------------------\n"
                "Visual L/R level meter (tall node, 3x height).\n\n"
                "  Bar meters    Green > Yellow > Orange > Red\n"
                "  Peak hold     White line shows recent peaks\n"
                "  CLIP LED      Red when clipping (click to reset)\n\n"
                "Use for: monitoring levels anywhere in the chain.\n\n\n"
                "MIDI MONITOR\n"
                "---------------------------------------------------------------\n"
                "Live MIDI event display (tall node, 3x height).\n\n"
                "  Shows channel, note name, velocity for each event\n"
                "  Green = Note On, Orange = Note Off\n"
                "  Auto-scrolls, newest events at top\n\n"
                "Use for: debugging MIDI routing, verifying input.\n\n\n"
                "RECORDER\n"
                "---------------------------------------------------------------\n"
                "24-bit WAV audio recorder (wide + tall node).\n\n"
                "  Name box      Click to set filename prefix\n"
                "  Record (red)  Start recording\n"
                "  Stop (blue)   Stop and save file\n"
                "  Folder icon   Open recordings folder\n"
                "  SYNC toggle   Link multiple recorders together\n"
                "  Waveform      Real-time visualization while recording\n"
                "  Time display  Elapsed recording time\n\n"
                "Files saved as: [Name]_YYYYMMDD_HHMMSS.wav\n"
                "Default folder: Documents/Colosseum/recordings\n"
                "(configurable in Settings > Set Recording Folder)\n\n"
                "Sync mode: when SYNC is on (cyan), all synced recorders\n"
                "start and stop together. Perfect for multi-track recording.\n\n\n"
                "MIDI PLAYER\n"
                "---------------------------------------------------------------\n"
                "Plays .mid files with full transport control.\n\n"
                "  Load          Click to browse for a .mid file\n"
                "  Play/Pause    Toggle playback\n"
                "  Stop          Return to beginning\n"
                "  Loop          Toggle loop mode\n"
                "  Position      Drag slider to seek\n"
                "  Tempo         Override the file's tempo with a custom BPM\n"
                "  Markers       Jump between section markers in the file\n\n"
                "Outputs all 16 MIDI channels simultaneously.\n"
                "Wire its MIDI output to instruments in your chain.\n\n\n"
                "CC STEP SEQUENCER\n"
                "---------------------------------------------------------------\n"
                "16-slot MIDI CC sequencer for parameter automation.\n\n"
                "  16 independent sequence slots, each with:\n"
                "  - Target CC number (0-127)\n"
                "  - MIDI channel (1-16)\n"
                "  - 2 to 128 steps\n"
                "  - Rate division (1/1 to 1/128)\n"
                "  - Rate type: Normal, Triplet, or Dotted\n"
                "  - Step order: Forward, Reverse, Ping-Pong, Random, Drunk\n"
                "  - Speed mode: Half, Normal, Double\n"
                "  - Swing amount (0-100%)\n"
                "  - Smooth interpolation toggle\n\n"
                "  Play/Stop     Start or stop all sequences\n"
                "  BPM           Drag to set manual tempo\n"
                "  SYNC          Lock tempo to master clock\n"
                "  E button      Open the full step editor\n\n"
                "Wire the MIDI output to any plugin that responds to CC.\n"
                "Great for: filter sweeps, rhythmic volume gates,\n"
                "LFO-style automation, and live parameter control.\n\n\n"
                "TRANSIENT SPLITTER\n"
                "---------------------------------------------------------------\n"
                "Zero-latency transient/sustain audio splitter.\n"
                "Splits audio into attack (transient) and body (sustain)\n"
                "components on 4 separate outputs.\n\n"
                "  Outputs 1-2   Transient (attack) L/R\n"
                "  Outputs 3-4   Sustain (body) L/R\n\n"
                "  E button opens the editor with full controls:\n\n"
                "  DETECTION\n"
                "  Sensitivity    How easily transients are detected\n"
                "  Decay          Gate close speed (1-500 ms)\n"
                "  Hold Time      Minimum transient gate open time (0-100 ms)\n"
                "  Smoothing      Gate signal smoothing to prevent clicks\n\n"
                "  FREQUENCY FOCUS (detection sidechain only)\n"
                "  High-Pass      Ignore low-frequency content in detection\n"
                "  Low-Pass       Ignore high-frequency content in detection\n"
                "  (These filters affect detection only, NOT the audio!)\n\n"
                "  OUTPUT\n"
                "  Transient Gain   Independent level for transient outputs\n"
                "  Sustain Gain     Independent level for sustain outputs\n"
                "  Balance          Redistribute energy between outputs\n\n"
                "  MODES\n"
                "  Stereo Link      Mono detection (max L/R) vs independent\n"
                "  Gate Mode        Hard 0/1 split vs proportional blend\n"
                "  Invert           Swap transient and sustain classification\n\n"
                "Use for: parallel processing of attack vs body, drum\n"
                "shaping, adding compression to sustain only, creative\n"
                "sound design.\n\n\n"
                "LATCHER MIDI CONTROLLER\n"
                "---------------------------------------------------------------\n"
                "A 4x4 MIDI toggle pad controller. Each pad acts as a\n"
                "latching switch: press once to send Note On, press again\n"
                "to send Note Off.\n\n"
                "  16 pads displayed on the node face (4 rows x 4 columns)\n"
                "  Light gray = OFF, Golden = ON\n\n"
                "  ALL OFF button (upper right corner of the node)\n"
                "    Releases all latched pads at once. Red when any pad\n"
                "    is active, gray when all pads are off.\n\n"
                "  E button    Open the pad editor popup\n"
                "  X button    Delete the Latcher node\n\n"
                "PAD EDITOR (E button)\n"
                "  Each pad has four settings:\n"
                "  Trigger Note   The incoming MIDI note that toggles this pad\n"
                "  Output Note    The note number sent when the pad is toggled\n"
                "  Velocity       The velocity value sent (1-127)\n"
                "  MIDI Channel   The output channel (1-16)\n\n"
                "  A visual keyboard at the bottom shows the current note.\n\n"
                "TOGGLE BEHAVIOR\n"
                "  Click a pad on the node (or via external MIDI trigger):\n"
                "    OFF to ON:  Sends NoteOn  (144, channel, pitch, velocity)\n"
                "    ON to OFF:  Sends NoteOff (128, channel, pitch, velocity)\n"
                "  All values come from the pad's settings in the E editor.\n\n"
                "EXTERNAL MIDI TRIGGER\n"
                "  Connect a MIDI source (MIDI In, keyboard, etc.) to the\n"
                "  Latcher's MIDI input pin. When you press a key on your\n"
                "  controller:\n"
                "    1. Latcher checks if the note is mapped to any pad\n"
                "    2. If mapped: toggles that pad (OFF to ON or ON to OFF)\n"
                "    3. If not mapped: the note passes through unchanged\n"
                "  Key release (Note Off) is always ignored by the Latcher.\n"
                "  The toggle happens only on key press (Note On, status 144).\n\n"
                "MIDI OUTPUT\n"
                "  The Latcher has a MIDI output pin. Wire it to instruments\n"
                "  or other MIDI processors. The output carries:\n"
                "  * Note On/Off messages generated by pad toggles\n"
                "  * Any non-mapped MIDI that passes through\n\n"
                "Use for: latching sustain notes, building MIDI toggle switches,\n"
                "creating drone layers, holding chord tones, triggering samples\n"
                "with toggle-on/toggle-off behavior instead of press-and-hold.\n"
            );

        // =================================================================
        case Sampling:
        // =================================================================
            return juce::String(
                "===============================================================\n"
                "                         SAMPLING\n"
                "===============================================================\n\n"
                "Colosseum includes two specialized sampling tools for\n"
                "capturing audio from your plugin chain.\n\n\n"
                "MANUAL SAMPLING\n"
                "===============================================================\n\n"
                "Record individual notes triggered by live MIDI input.\n"
                "Perfect for sampling hardware synths or VST instruments\n"
                "one note at a time.\n\n"
                "HOW IT WORKS\n"
                "---------------------------------------------------------------\n"
                "1. Add Manual Sampling to the rack\n"
                "2. Wire audio IN from your instrument's output\n"
                "3. Wire MIDI IN from MIDI Input or a controller\n"
                "4. Set the family name (base filename) in the editor\n"
                "5. Click ARM\n"
                "6. Play a note on your MIDI controller\n"
                "7. Recording starts on note-on, stops on silence detection\n"
                "8. File saved automatically as: Family_NoteOctave_Vvelocity.wav\n"
                "   Example: Piano_C4_V127.wav\n\n"
                "Set the output folder in Settings > Set Sampler Folder.\n\n\n"
                "AUTO SAMPLING\n"
                "===============================================================\n\n"
                "Automated multi-note sampling. Sends MIDI notes one by one\n"
                "to your instrument, records each response, and saves them\n"
                "as individual files. You can sample an entire keyboard\n"
                "range in one go.\n\n"
                "HOW IT WORKS\n"
                "---------------------------------------------------------------\n"
                "1. Add Auto Sampling to the rack\n"
                "2. Wire its MIDI OUT to your instrument's MIDI IN\n"
                "3. The tool follows the chain to tap audio from the\n"
                "   instrument's output (no explicit audio wiring needed)\n"
                "4. Configure notes, velocities, and durations:\n\n"
                "TEXT SYNTAX\n"
                "---------------------------------------------------------------\n"
                "Each line defines a sampling pass:\n"
                "  [notes], [velocities], [durations_sec]\n\n"
                "Notes:      Single (60), list (60,64,67), range (36-72)\n"
                "Velocities: Single (127), list (64,96,127), range (32-127)\n"
                "Durations:  Optional, in seconds (0.5, 1.0, 2.0)\n"
                "            Omit to use global hold time\n\n"
                "EXAMPLES\n"
                "---------------------------------------------------------------\n"
                "[36-72], [127]             Every note C2-C5, max velocity\n"
                "[60], [32,64,96,127]       Middle C at 4 velocity layers\n"
                "[36-84], [64,127], [1.5]   Range at 2 velocities, 1.5s each\n\n"
                "Press Start to begin the automated capture sequence.\n"
                "Files are named with note and velocity info automatically.\n"
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
                "Right-click empty space     Add Node menu (System Tools submenu\n"
                "                            + your plugin list)\n"
                "Right-click a pin           Pin info tooltip\n"
                "Right-click a wire          Wire info / delete connection\n"
                "Right-click a button        Tooltip describing the button\n"
                "Double-click a wire         Delete connection\n"
                "Double-click a plugin node  Open plugin editor window\n"
                "Drag pin to pin             Create connection\n"
                "Drag a node                 Move it around the rack\n\n"
                "NODE BUTTONS\n"
                "---------------------------------------------------------------\n"
                "E       Open editor (plugin GUI or system tool popup)\n"
                "CH      MIDI channel selector (instruments)\n"
                "M       Mute / bypass\n"
                "P       Pass-through (effects)\n"
                "X       Delete node\n\n"
                "WORKSPACE BAR\n"
                "---------------------------------------------------------------\n"
                "Click workspace button      Switch to that workspace\n"
                "Right-click workspace       Context menu: Enable/Disable,\n"
                "                            Rename, Clear, Duplicate to...\n\n"
                "INSTRUMENT SELECTOR (bottom)\n"
                "---------------------------------------------------------------\n"
                "Click slot                  Activate/deactivate instrument\n"
                "Right-click slot            MIDI controller info\n"
                "Multi-Mode toggle:\n"
                "  OFF = One instrument at a time (radio buttons)\n"
                "  ON  = Multiple instruments simultaneously\n\n"
                "MIDI NOTES 1-32 are reserved for instrument selection.\n"
                "Perfect for MIDI pad controllers.\n\n"
                "LEFT PANEL BUTTONS\n"
                "---------------------------------------------------------------\n"
                "Load         Open a saved .xml patch\n"
                "Save         Save current session (all 16 workspaces)\n"
                "Reset        Clear everything and start fresh\n"
                "Keys         Toggle virtual MIDI keyboard\n"
                "MIDI Panic   Send all-notes-off on all channels\n"
                "Floating     Detach/dock the mixer in a floating window\n"
                "  Mixer\n\n"
                "LATCHER NODE\n"
                "---------------------------------------------------------------\n"
                "Click pad               Toggle pad ON/OFF\n"
                "ALL OFF (top right)      Release all latched pads\n"
                "E button                 Open pad editor\n"
                "X button                 Delete Latcher\n\n"
                "MAGNETIC SNAP\n"
                "---------------------------------------------------------------\n"
                "Drag connected node      Auto-snaps to alignment\n"
                "Shift + drag             Disable snap temporarily\n\n"
                "PLUGIN BROWSER\n"
                "---------------------------------------------------------------\n"
                "Add Plugins button       Standard plugin list\n"
                "Favorites button         Browse favorite .subt patches\n"
                "Set Folder...            Choose favorites folder\n"
                "Double-click favorite    Load into workspace (choose which)\n\n"
                "SETTINGS\n"
                "---------------------------------------------------------------\n"
                "Save as Default          Save startup patch (right-click for info)\n"
                "Clear Default            Remove startup patch\n\n"
                "RECORDER MODULE\n"
                "---------------------------------------------------------------\n"
                "Red circle      Start recording\n"
                "Blue square     Stop and save\n"
                "Folder icon     Open recordings folder\n"
                "Name box        Edit filename prefix\n"
                "SYNC/IND        Toggle linked recording\n"
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
                "2. Settings tab: verify audio device is selected (not OFF)\n"
                "3. Click ON on Audio Output node if it's grayed out\n"
                "4. Check that instruments are connected to Audio Output\n"
                "5. Check Mixer - make sure nothing is muted\n"
                "6. Verify the correct workspace is active\n\n"
                "MIDI NOT RESPONDING\n"
                "---------------------------------------------------------------\n"
                "1. Settings tab: verify MIDI device is listed\n"
                "2. Check channel filter (should show ALL or your channels)\n"
                "3. Try the virtual keyboard (Keys button) to rule out\n"
                "   hardware issues\n"
                "4. Use a MIDI Monitor node to verify MIDI is flowing\n"
                "5. Check the instrument's CH button for channel mismatch\n"
                "6. If device disconnected: hit 'Reconnect MIDI Devices'\n"
                "   in Settings instead of restarting\n\n"
                "PLUGIN CRASHES DURING SCAN\n"
                "---------------------------------------------------------------\n"
                "1. Reopen Colosseum\n"
                "2. Accept the prompt to blacklist the problematic plugin\n"
                "3. Scan again - the bad plugin will be skipped\n"
                "4. Update the plugin and use 'Reset Blacklist' to retry\n\n"
                "PLUGIN NOT APPEARING\n"
                "---------------------------------------------------------------\n"
                "1. Plugins tab > Plugin Folders > verify paths\n"
                "2. Click 'Add Defaults' for standard OS locations\n"
                "3. Rescan plugins\n"
                "4. Check: must be 64-bit, correct format for your OS\n"
                "5. Check eye toggle - plugin may be hidden, not missing\n\n"
                "HIGH CPU\n"
                "---------------------------------------------------------------\n"
                "1. Increase buffer size in the ASIO Control Panel\n"
                "2. Bypass unused plugins (M button)\n"
                "3. Close plugin editor windows you're not using\n"
                "4. Reduce active plugin count\n"
                "5. Check CPU meter - over 70% means you're pushing it\n\n"
                "CONNECTIONS NOT WORKING\n"
                "---------------------------------------------------------------\n"
                "* Audio pins (blue) only connect to audio pins\n"
                "* MIDI pins (red) only connect to MIDI pins\n"
                "* Must go from output (bottom) to input (top)\n"
                "* Check that modules aren't bypassed (M button)\n\n"
                "WORKSPACE ISSUES\n"
                "---------------------------------------------------------------\n"
                "* Disabled workspaces (gray) can't be selected - right-click\n"
                "  to enable them\n"
                "* If a workspace seems empty, it may have been cleared\n"
                "* System tools are now saved with workspaces - if upgrading\n"
                "  from an older version, system tools in old patches may need\n"
                "  to be re-added\n\n"
                "RECORDING ISSUES\n"
                "---------------------------------------------------------------\n"
                "1. Verify audio reaches the Recorder (connect it inline)\n"
                "2. Check recording folder exists and is writable\n"
                "3. Check disk space\n"
                "4. Click the folder icon to verify files are being saved\n"
            );

        // =================================================================
        case DemoLimitations:
        // =================================================================
            return juce::String(
                "===============================================================\n"
                "                    DEMO & REGISTRATION\n"
                "===============================================================\n\n"
                "Colosseum runs in demo mode until registered.\n\n"
                "DEMO LIMITATION\n"
                "---------------------------------------------------------------\n"
                "A repeating cycle of:\n"
                "  15 seconds   Normal operation (full audio + MIDI)\n"
                "  3 seconds    Complete silence (audio + MIDI blocked)\n\n"
                "During silence periods:\n"
                "* Audio inputs and outputs are muted\n"
                "* MIDI input is blocked\n"
                "* All-Notes-Off is sent to prevent stuck notes\n\n"
                "WHAT IS NOT LIMITED\n"
                "---------------------------------------------------------------\n"
                "Everything else works fully in demo mode:\n"
                "* All features, all plugins, all system tools\n"
                "* Save/Load patches, 16 workspaces\n"
                "* Recording, sampling, MIDI routing\n"
                "* No plugin count limits\n\n"
                "STATUS INDICATOR\n"
                "---------------------------------------------------------------\n"
                "Top right corner:\n"
                "  Orange/Red REG LED = Demo mode\n"
                "  Green REG LED      = Registered\n\n"
                "HOW TO REGISTER\n"
                "---------------------------------------------------------------\n"
                "1. Go to the Register tab\n"
                "2. Note your unique Machine ID\n"
                "3. Click 'Copy ID' to copy it to clipboard\n"
                "4. Visit our website to purchase a license\n"
                "5. Provide your Machine ID during purchase\n"
                "6. You'll receive a serial number by email\n"
                "7. Enter the serial in the Register tab\n"
                "8. Click 'Register'\n\n"
                "Once registered, the silence limitation is permanently\n"
                "removed. Your license is tied to your machine.\n\n"
                "DEMO PURPOSE\n"
                "---------------------------------------------------------------\n"
                "The demo lets you fully evaluate Colosseum before buying:\n"
                "test plugins, build patches, learn the workflow. The periodic\n"
                "silence ensures it can't be used for production without a\n"
                "license, while giving you complete access to every feature.\n"
            );

        default:
            return "Select a chapter from the navigation above.";
    }
}