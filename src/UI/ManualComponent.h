#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class ManualComponent : public juce::Component
{
public:
    ManualComponent()
    {
        // Define Pages
        pageTitles = { 
            "1. What is OnStage?", "2. App Layout", "3. I/O Tab", 
            "4. Studio Tab", "5. Effects Browser", "6. Studio Effects",
            "7. Guitar Effects", "8. Media Tab", "9. Presets",
            "10. MIDI & Tooltips", "11. Recording", "12. Registration"
        };

        pageContents = {
            // 1. Intro
            "WHAT IS ONSTAGE?\n\n"
            "Welcome to OnStage! This application is a professional live performance environment designed specifically for singers, "
            "musicians, karaoke hosts, and live streamers.\n\n"
            "OnStage combines a low-latency ASIO audio engine with a powerful modular effects studio and a built-in media player "
            "in one seamless interface.\n\n"
            "Who is it for?\n"
            "- Vocalists who need real-time effects processing without lag.\n"
            "- Guitar players looking for a full pedalboard of effects.\n"
            "- Karaoke hosts running complex routing setups.\n"
            "- Live streamers who want professional sound.\n"
            "- Anyone who wants to record high-quality demos over backing tracks.\n\n"
            "What makes it special?\n"
            "OnStage uses a fully modular graph-based audio engine. Instead of a fixed effects chain, you build your own signal "
            "path by dragging effects onto a visual canvas and wiring them together however you like. "
            "With over 30 built-in effects covering both studio vocal processing and guitar pedals, plus a built-in media player "
            "and recorder, OnStage is your complete live performance toolkit.",

            // 2. App Layout
            "APP LAYOUT\n\n"
            "OnStage is organized into three main areas:\n\n"
            "Header Bar (Top):\n"
            "Contains the Fanan and OnStage logos, Manual button, Save/Load Preset buttons, a preset name display, "
            "the Register button, and a PRO/DEMO mode indicator.\n\n"
            "Sidebar (Left):\n"
            "Three tab buttons switch between the main pages:\n"
            "- I/O: Audio device and MIDI configuration.\n"
            "- Studio: The modular effects canvas where you build your signal chain.\n"
            "- Media: Built-in media player and playlist manager.\n\n"
            "Below the tab buttons you will find:\n"
            "- ASIO LED: Green when an ASIO device is connected, red when disconnected.\n"
            "- REG LED: Green when registered (Pro Mode), red in Demo Mode.\n"
            "- CPU: Current audio processing CPU usage.\n"
            "- RAM: Application memory usage.\n"
            "- LAT: Total audio latency in milliseconds.\n\n"
            "Right Strip:\n"
            "A vertical strip on the far right displaying:\n"
            "- Master Meter: Stereo L/R level meters with green, yellow, and red zones plus peak hold indicators.\n"
            "- Master Volume: A vertical slider (MIDI CC 7) controlling the final output level.",

            // 3. I/O Tab
            "THE I/O TAB\n\n"
            "This is your control center for audio and MIDI connections.\n\n"
            "ASIO Driver:\n"
            "Select your audio interface's ASIO driver from the dropdown. If no hardware is connected, the system stays OFF "
            "to prevent crashes. Click 'Control Panel' to open your driver's native settings for buffer size and sample rate.\n\n"
            "Device Info:\n"
            "Once connected, the I/O Tab displays your current sample rate, buffer size, round-trip latency, "
            "and the number of active input and output channels.\n\n"
            "Channel Lists:\n"
            "Scrollable lists showing all available input and output channel names from your ASIO device.\n\n"
            "MIDI Input:\n"
            "Click 'Select MIDI Inputs...' to open a popup where you can enable multiple MIDI controllers simultaneously. "
            "Each device has a checkbox. Changes take effect immediately.\n\n"
            "Recording Folder:\n"
            "Click 'Set Default Recording Folder...' to choose where recordings are saved by default. "
            "The current path is displayed below the button.\n\n"
            "Note: Sample rate and buffer size are controlled externally through your ASIO driver's control panel, "
            "not within OnStage.",

            // 4. Studio Tab
            "THE STUDIO TAB\n\n"
            "This is the heart of OnStage - a fully modular audio routing canvas.\n\n"
            "The Wiring Canvas:\n"
            "The Studio tab displays a visual graph where audio flows from input nodes on the left to output nodes on the right. "
            "You build your signal chain by adding effect nodes and connecting them with virtual wires.\n\n"
            "Built-in Nodes (always present):\n"
            "- Audio Input: Represents your ASIO hardware inputs (microphones, instruments).\n"
            "- Audio Output: Represents your ASIO hardware outputs (speakers, headphones).\n"
            "- Playback: Audio from the built-in media player.\n\n"
            "Adding Effects:\n"
            "- Right-click on empty canvas space to open the Add Effect menu.\n"
            "- Or drag effects from the Effects Browser panel on the right.\n"
            "- Or double-click an effect in the browser to add it at a default position.\n\n"
            "Connecting Nodes:\n"
            "- Click and drag from an output pin (bottom of a node) to an input pin (top of another node) to create a wire.\n"
            "- Blue pins carry audio signals.\n"
            "- You can connect any output to any input to create custom routing.\n\n"
            "Node Controls:\n"
            "Each effect node has small buttons at the bottom:\n"
            "- B (Red): Bypass the effect (audio passes through unprocessed).\n"
            "- E (Gold): Open the effect's editor window with all its parameters.\n"
            "- X (Red): Delete the node from the canvas.\n\n"
            "Right-click a node for additional options like 'Disconnect All Wires' or 'Delete'.\n\n"
            "Pre-Amp nodes have an inline gain slider directly on the canvas for quick adjustment.",

            // 5. Effects Browser
            "EFFECTS BROWSER\n\n"
            "When you switch to the Studio tab, an Effects Browser panel appears on the right side of the canvas.\n\n"
            "The browser lists all 30+ built-in effects organized into two sections:\n"
            "- STUDIO EFFECTS (gold header): Professional vocal and mixing processors.\n"
            "- GUITAR EFFECTS (purple header): Classic guitar pedal emulations.\n\n"
            "Each effect shows a colored category dot:\n"
            "- Blue: Input (Pre-Amp, Gate)\n"
            "- Green: Dynamics (EQ, Compressor, De-Esser, Dynamic EQ, Master)\n"
            "- Red: Color (Exciter, Sculpt, Saturation, Doubler)\n"
            "- Purple: Time (Reverbs, Delay)\n"
            "- Orange: Pitch (Harmonizer)\n"
            "- Gray: Utility (Recorder)\n"
            "- Deep Purple: Guitar (all guitar effects)\n\n"
            "Search:\n"
            "Type in the search box at the top to filter effects by name or category. "
            "The count at the bottom shows how many effects match your search.\n\n"
            "Adding Effects:\n"
            "- Drag an effect from the browser onto the canvas to place it at a specific position.\n"
            "- Double-click an effect to add it at a default position.\n"
            "- The effect count updates as you filter.",

            // 6. Studio Effects
            "STUDIO EFFECTS\n\n"
            "Input:\n"
            "- Pre-Amp: Controls input gain with an inline slider on the canvas.\n"
            "- Gate: Noise gate to silence audio below a threshold.\n\n"
            "EQ & Dynamics:\n"
            "- EQ: 3-band parametric equalizer (Low Shelf, Mid Bell, High Shelf) with real-time frequency response display.\n"
            "- Compressor: Dynamics control with threshold, ratio, attack, release, and makeup gain. "
            "Shows a compression curve with a golden circle tracking input/output levels.\n"
            "- De-Esser: Targeted sibilance reduction with adjustable frequency and threshold.\n"
            "- Dynamic EQ: Sidechain-driven EQ that automatically ducks frequencies when audio is detected. "
            "Animated visualization shows the ducking action.\n"
            "- Master: Mastering processor combining ultrasonic filtering, glue compression, harmonic excitation, "
            "and stereo enhancement in one module. Controls: Sidepass, Glue, Scope, Skronk, Girth, Drive.\n\n"
            "Color & Character:\n"
            "- Exciter: Adds high-frequency sparkle and clarity.\n"
            "- Sculpt: Tube-style saturation with intelligent tone shaping. Clean Mud (300Hz), Tame Harsh (3.5kHz), Air (12kHz).\n"
            "- Saturation: Analog-style harmonic saturation.\n"
            "- Doubler: Creates a thicker sound by adding delayed copies of the signal with adjustable timing and level.\n\n"
            "Time-Based:\n"
            "- Convo. Reverb: Convolution reverb using impulse responses. Load custom IR files for any acoustic space. "
            "Features Hall, Plate, and Ambiance modes plus gated reverb.\n"
            "- Studio Reverb: Algorithmic reverb with four models - Room, Chamber, Space, and a fourth variant. "
            "Each model has its own dedicated parameters. Animated visualization.\n"
            "- Delay: Echo effect with multiple types, feedback, filtering, and stereo ping-pong.\n\n"
            "Pitch:\n"
            "- Harmonizer: Adds up to 4 pitch-shifted harmony voices with individual semitone, pan, gain, delay, "
            "and formant controls. Includes wet level and glide time.\n\n"
            "Utility:\n"
            "- Recorder: An inline recording node you can place anywhere in the signal chain. "
            "Records whatever audio passes through it to a WAV file. You can name each recorder and place multiple recorders "
            "to capture audio at different points in your chain.",

            // 7. Guitar Effects
            "GUITAR EFFECTS\n\n"
            "OnStage includes a full set of guitar effect emulations. These appear with a purple theme on the canvas.\n\n"
            "Drive:\n"
            "- Overdrive: Smooth tube-style overdrive with drive, tone, and level controls.\n"
            "- Distortion: Harder clipping distortion with gain, tone, and output.\n"
            "- Fuzz: Vintage fuzz pedal emulation.\n\n"
            "Modulation:\n"
            "- Chorus: Classic chorus with rate, depth, and mix controls.\n"
            "- Flanger: Jet-like flanging effect with rate, depth, feedback, and mix.\n"
            "- Phaser: Multi-stage phaser with adjustable center, rate, depth, feedback, stages, spread, "
            "sharpness, stereo width, waveform, tone, and mix.\n"
            "- Tremolo: Volume modulation with rate, depth, wave shape, stereo, and bias controls.\n"
            "- Vibrato: Pitch modulation with rate, depth, wave, stereo, delay, and mix.\n\n"
            "Tone:\n"
            "- Tone: Guitar-focused 3-band EQ with bass, mid (sweepable frequency), treble, and presence.\n"
            "- Tone Stack: Amp-style tone stack emulation with multiple amp models (bass, mid, treble, gain).\n\n"
            "Spatial:\n"
            "- Rotary Speaker: Leslie speaker emulation with horn rate, doppler, tremolo, rotor rate, "
            "drive, waveshape, width, and mix.\n"
            "- Wah: Wah pedal with pedal position, mode (manual/auto/LFO), model, Q, sensitivity, attack, "
            "LFO rate, and mix.\n"
            "- Reverb: Simple guitar reverb with size, damping, mix, and width.\n\n"
            "Utility:\n"
            "- Noise Gate: Guitar noise gate with threshold, attack, hold, and release.\n\n"
            "Cabinets:\n"
            "- Cab Sim: Cabinet simulator with selectable cabinet models, mic types, mic position, and level.\n"
            "- Cab IR (Convolution): Load custom cabinet impulse response files for realistic amp simulation.",

            // 8. Media Tab
            "THE MEDIA TAB\n\n"
            "Your built-in media player for backing tracks and karaoke.\n\n"
            "The Playlist:\n"
            "- Add audio and video files (MP3, WAV, MP4, AVI, and more).\n"
            "- Track Banners: Each track in the list has its own controls. Click '+' on a track to reveal Volume, Speed, "
            "and Wait Time sliders.\n"
            "- Crossfade (F): Toggle the 'F' button on a track banner. When ON, the 'Wait' slider becomes a Crossfade duration, "
            "smoothly blending into the next track.\n\n"
            "Transport Controls:\n"
            "- Play, Pause, and Stop buttons.\n"
            "- Progress slider to scrub through the current track.\n"
            "- Video display for karaoke and video files.\n\n"
            "The media player's audio appears as the 'Playback' node on the Studio canvas, "
            "allowing you to route backing track audio through effects or directly to the output.\n\n"
            "Media and playlist folder locations are saved in your settings and restored on next launch.",

            // 9. Presets
            "PRESETS\n\n"
            "OnStage saves your entire effects canvas (all nodes, wiring, and parameter values) as a preset.\n\n"
            "Saving:\n"
            "Click 'Save Preset' in the header bar. Choose a location and filename. "
            "Presets are saved as .onspreset files.\n\n"
            "Loading:\n"
            "Click 'Load Preset' in the header bar. Browse to a .onspreset file and select it. "
            "The canvas will be restored with all your effects, connections, and settings.\n\n"
            "The current preset name is displayed in the header bar between the Save/Load buttons and the Register button.",

            // 10. MIDI & Tooltips
            "MIDI SUPPORT & TOOLTIPS\n\n"
            "MIDI Control:\n"
            "OnStage allows you to control knobs and buttons using external MIDI controllers.\n\n"
            "1. Connect your MIDI device(s).\n"
            "2. Go to the I/O Tab and click 'Select MIDI Inputs...' to enable your device(s).\n"
            "3. Multiple MIDI devices can be enabled simultaneously.\n\n"
            "The Master Volume slider is mapped to MIDI CC 7.\n\n"
            "Tooltips:\n"
            "Right-click on any knob, slider, or toggle button in the application to see a tooltip popup showing:\n"
            "- What the control does.\n"
            "- Which MIDI CC number or Note is assigned to it.\n\n"
            "This works everywhere in the app, including effect panels and playlist tracks.",

            // 11. Recording
            "RECORDING\n\n"
            "OnStage provides flexible recording options through the Recorder node.\n\n"
            "Using the Recorder Node:\n"
            "1. Add a 'Recorder' node from the Studio Effects menu or the Effects Browser.\n"
            "2. Wire it into your signal chain at the point where you want to capture audio.\n"
            "3. Click the record button on the Recorder node to start recording.\n"
            "4. Click stop when finished. Your recording is saved as a high-quality WAV file.\n\n"
            "You can place multiple Recorder nodes at different points in your chain to capture "
            "audio at various stages (for example, one before effects for a dry recording and one after effects for a wet recording).\n\n"
            "Each Recorder node has its own name editor so you can label your recordings.\n\n"
            "Default Recording Folder:\n"
            "Set your preferred recording folder in the I/O Tab using the 'Set Default Recording Folder...' button. "
            "If no folder is set, recordings go to the default location (My Documents\\OnStage\\recordings).\n\n"
            "Tech Note:\n"
            "Recordings are saved as high-quality WAV files at whatever sample rate your ASIO device is running.",

            // 12. Registration
            "REGISTRATION\n\n"
            "Unlock the full power of OnStage (Pro Mode).\n\n"
            "1. Click the 'REGISTER' button in the header bar.\n"
            "2. Copy the 'User ID' shown in the registration window.\n"
            "3. Go to our website and paste this ID into the box above the 'Buy Now' link.\n"
            "4. Complete the purchase. You will receive a Serial Number on-screen and via email.\n"
            "5. Paste that Serial Number back into the OnStage registration window.\n"
            "6. Click 'Save License File'.\n\n"
            "That's it! The REG LED in the sidebar turns green and the header shows 'PRO'.\n\n"
            "Developed by Subcore - Professional Audio Software."
        };

        // Create Navigation Buttons
        for (int i = 0; i < pageTitles.size(); ++i)
        {
            auto* btn = new juce::TextButton();
            btn->setButtonText(juce::String(i + 1));
            btn->setTooltip(pageTitles[i]); // Hover to see page name
            btn->setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF2A2A2A));
            btn->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
            btn->onClick = [this, i] { setPage(i); };
            addAndMakeVisible(btn);
            navButtons.add(btn);
        }

        // Title Header
        addAndMakeVisible(pageHeaderLabel);
        pageHeaderLabel.setFont(juce::Font(22.0f, juce::Font::bold));
        pageHeaderLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFD4AF37)); // Gold
        pageHeaderLabel.setJustificationType(juce::Justification::centred);

        // Content Area
        addAndMakeVisible(contentView);
        contentView.setMultiLine(true);
        contentView.setReadOnly(true);
        contentView.setCaretVisible(false);
        contentView.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFF1A1A1A));
        contentView.setColour(juce::TextEditor::textColourId, juce::Colours::lightgrey);
        contentView.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xFF333333));
        contentView.setFont(juce::Font(15.0f));

        setPage(0);
        setSize(750, 550);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(15);
        
        // Navigation Bar
        auto navArea = area.removeFromTop(30);
        int btnWidth = navArea.getWidth() / navButtons.size();
        for (auto* btn : navButtons)
        {
            btn->setBounds(navArea.removeFromLeft(btnWidth).reduced(2, 0));
        }

        area.removeFromTop(10);
        
        // Page Title
        pageHeaderLabel.setBounds(area.removeFromTop(30));
        
        // Content
        area.removeFromTop(10);
        contentView.setBounds(area);
    }

private:
    void setPage(int index)
    {
        if (index < 0 || index >= pageTitles.size()) return;

        // Update Buttons Visuals
        for (int i = 0; i < navButtons.size(); ++i)
        {
            if (i == index) {
                navButtons[i]->setColour(juce::TextButton::buttonColourId, juce::Colour(0xFFD4AF37)); // Gold
                navButtons[i]->setColour(juce::TextButton::textColourOffId, juce::Colours::black);
            } else {
                navButtons[i]->setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF2A2A2A)); // Dark
                navButtons[i]->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
            }
        }

        // Update Text
        pageHeaderLabel.setText(pageTitles[index], juce::dontSendNotification);
        contentView.setText(pageContents[index]);
        
        // FIX: Pass 'false' to indicate "do not select text" when moving caret
        contentView.moveCaretToTop(false);
    }

    juce::StringArray pageTitles;
    juce::StringArray pageContents;
    juce::OwnedArray<juce::TextButton> navButtons;
    juce::Label pageHeaderLabel;
    juce::TextEditor contentView;
};