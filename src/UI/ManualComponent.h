#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class ManualComponent : public juce::Component
{
public:
    ManualComponent()
    {
        // Define Pages
        pageTitles = { 
            "1. What is OnStage?", "2. I/O Tab", "3. Vocals Tab", 
            "4. Media Tab", "5. Visual Meters", "6. MIDI Support", 
            "7. Tooltips", "8. Recording", "9. Registration" 
        };

        pageContents = {
            // 1. Intro
            "WHAT IS ONSTAGE?\n\n"
            "Welcome to OnStage! This application is a professional live performance environment designed specifically for singers, "
            "karaoke hosts, and live streamers. Unlike standard media players or DAWs, OnStage combines a low-latency audio engine "
            "with a dedicated media player and a powerful vocal effects chain in one seamless interface.\n\n"
            "Who is it for?\n"
            "- Performers who need real-time vocal effects without lag.\n"
            "- Karaoke hosts running complex routing setups.\n"
            "- Singers who want to record high-quality demos over backing tracks.\n\n"
            "What makes it special?\n"
            "OnStage runs exclusively on ASIO drivers, ensuring your voice is processed instantly. It integrates backing tracks, "
            "microphones, and effects (Reverb, Delay, Harmonizer, Exciter) into a single, stability-focused workspace.",

            // 2. I/O Tab
            "THE I/O TAB\n\n"
            "This is your control center for audio connections. OnStage is built for ASIO to ensure professional low-latency performance.\n\n"
            "1. ASIO Driver:\n"
            "   Select your audio interface's ASIO driver here. If your hardware is not connected, the system defaults to 'OFF' to prevent crashes. "
            "   Click 'Control Panel' to adjust buffer sizes directly in your driver's settings.\n\n"
            "2. Microphone Inputs:\n"
            "   Route your physical inputs to Mic 1 and Mic 2. You can Mute or Bypass FX for each mic instantly using the toggle buttons.\n\n"
            "3. Backing Tracks Routing:\n"
            "   This section lets you route audio from external software (like virtual cables) or inputs into the OnStage mixer. "
            "   The 'Internal Media Player' is always active on Input 1.\n\n"
            "4. Vocal Settings:\n"
            "   - Latency Correction: If your recordings sound out of sync with the music, adjust this slider to shift the vocals in time.\n"
            "   - Vocal Boost: Adds gain to your vocals specifically in the recording file, ensuring you are heard clearly over loud backing tracks.",

            // 3. Vocals Tab
            "THE VOCALS TAB\n\n"
            "This is the heart of your sound. Each microphone has its own dedicated chain:\n\n"
            "Per-Mic Effects:\n"
            "- Preamp: Controls the initial input gain.\n"
            "- Exciter (Air): Adds high-end sparkle and clarity to dull vocals.\n"
            "- Sculpt: Tube-style saturation processor with intelligent tone shaping. Clean Mud removes boxiness at 300Hz, "
            "Tame Harsh smooths harshness at 3.5kHz, and Air adds brilliance at 12kHz.\n"
            "- EQ: A 3-band equalizer (Low Shelf, Mid Bell, High Shelf) to shape your tone precisely.\n"
            "- Compressor: Evens out your volume, making quiet parts audible and loud parts controlled.\n\n"
            "Global Effects (Applied to both mics):\n"
            "- Harmonizer: Adds synthesized backing vocals (pitch-shifted copies of your voice). Supports 2 independent voices.\n"
            "- Dynamic EQ (Sidechain): Automatically ducks specific frequencies in the music when you sing, making your voice cut through the mix.\n"
            "- Reverb: Simulates room acoustics using convolution. Load custom impulse responses for any space.\n"
            "- Delay: Adds echoes with ping-pong stereo, feedback control, and filtering.\n\n"
            "Visual Meters:\n"
            "Each effect panel displays real-time animated graphs showing exactly what that processor is doing to your sound.",

            // 4. Media Tab
            "THE MEDIA TAB\n\n"
            "Your built-in DJ booth. This tab handles video and audio playback.\n\n"
            "The Playlist:\n"
            "- Add Files: Supports MP3, WAV, MP4, AVI, and more.\n"
            "- Track Banners: Each track in the list has its own controls. Click the '+' button on a track to reveal Volume, Speed, and Wait Time sliders.\n"
            "- Crossfade (F): Toggle the 'F' button on a track banner. When ON, the 'Wait' slider becomes a Crossfade duration, smoothly blending into the next track.\n\n"
            "Controls:\n"
            "- Play/Pause/Stop: Standard transport controls.\n"
            "- Progress Slider: Scrub through the track.\n"
            "- Video Screen: Displays video content for karaoke files.",

            // 5. Visual Meters
            "VISUAL METERS\n\n"
            "Every effect in OnStage includes a real-time animated graph showing exactly what's happening to your audio.\n\n"
            "EQ Panel:\n"
            "- Displays the combined frequency response curve showing how all 3 bands shape your sound together.\n\n"
            "Compressor Panel:\n"
            "- Shows the compression curve with a moving golden circle that tracks your current input/output levels in real-time.\n\n"
            "Sculpt Panel:\n"
            "- Visualizes the frequency response showing mud reduction, harsh taming, and air boost curves.\n\n"
            "Dynamic EQ Panel:\n"
            "- Displays an animated 'bowl' that pumps up and down at the sidechain frequency, showing how much the music is being ducked.\n\n"
            "Harmonizer Panel:\n"
            "- Shows a pitch ladder with glowing lines representing your original voice and the two harmony voices.\n\n"
            "Delay Panel:\n"
            "- Animated echoes travel across the screen, fading with each repeat, with ping-pong between left/right channels.\n\n"
            "Reverb Panel:\n"
            "- Particle cloud animation: particles explode from center, swirl, and fade based on frequency (high frequencies die faster).",

            // 6. MIDI Support
            "MIDI SUPPORT\n\n"
            "OnStage allows you to control almost every knob and button using an external MIDI controller.\n\n"
            "How it works:\n"
            "1. Connect your MIDI device.\n"
            "2. Go to the I/O Tab and select your device in the 'MIDI Input' dropdown.\n"
            "3. Click 'Refresh' if you connected the device after opening the app.\n\n"
            "Mappings are fixed to standard CC numbers to ensure stability. You can see exactly which CC number controls which knob by right-clicking any control (see 'Tooltips').",

            // 7. Tooltips
            "TOOLTIPS & HELP\n\n"
            "Don't remember the MIDI map? No problem!\n\n"
            "Simply RIGHT-CLICK on any knob, slider, or toggle button in the application.\n"
            "A popup bubble will appear telling you exactly what that control does and which MIDI CC number or Note is assigned to it.\n\n"
            "This feature is available everywhere—even on the playlist tracks!",

            // 8. Recording
            "RECORDING\n\n"
            "Capture your performance instantly.\n\n"
            "1. Click the red 'RECORD' button at the top right.\n"
            "2. Sing! The app records the full mix (Vocals + Music + Effects).\n"
            "3. Click 'Stop' when finished.\n"
            "4. A 'Download WAV' button will appear. Click it to save your masterpiece to your computer.\n\n"
            "Tech Note:\n"
            "Recordings are saved as high-quality WAV files. If you find your vocals are slightly delayed in the recording compared to the music (due to system processing time), "
            "go to the I/O Tab and increase the 'Latency Correction' slider until they line up perfectly.",

            // 9. Registration
            "REGISTRATION\n\n"
            "Unlock the full power of OnStage (Pro Mode).\n\n"
            "1. Click the 'REGISTER' button in the top header.\n"
            "2. Copy the 'User ID' shown in the window.\n"
            "3. Go to our website and paste this ID into the box above the 'Buy Now' link.\n"
            "4. Complete the purchase. You will receive a Serial Number on-screen and via email.\n"
            "5. Paste that Serial Number back into the OnStage registration window.\n"
            "6. Click 'Save License File'.\n\n"
            "That's it! You are now a Pro user.\n\n"
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
        contentView.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFF151515));
        contentView.setColour(juce::TextEditor::textColourId, juce::Colours::white);
        contentView.setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        contentView.setFont(juce::Font(16.0f));

        setSize(600, 500);
        setPage(0); // Load Page 1
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xFF202020)); // Background
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