
// #D:\Workspace\Subterraneum_plugins_daw\src\StudioTab.h

#pragma once

#include <JuceHeader.h>
#include "Style.h"

class SubterraneumAudioProcessor;

// FIX #3: Custom LookAndFeel for gold sliders in Studio Tab
class GoldSliderLookAndFeel : public juce::LookAndFeel_V4
{
public:
    GoldSliderLookAndFeel()
    {
        // Gold colors
        goldColor = juce::Colour(0xffFFD700);        // Gold
        darkGoldColor = juce::Colour(0xff9B7A00);    // Dark gold
        blackColor = juce::Colours::black;
    }
    
    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                         float sliderPos, float /*minSliderPos*/, float /*maxSliderPos*/,
                         const juce::Slider::SliderStyle /*style*/, juce::Slider& /*slider*/) override
    {
        auto trackWidth = juce::jmin(6.0f, (float)height * 0.25f);
        
        auto trackBounds = juce::Rectangle<float>((float)x, (float)y + (float)height * 0.5f - trackWidth * 0.5f,
                                                   (float)width, trackWidth);
        
        // Draw "off" rail (dark gold)
        g.setColour(darkGoldColor);
        g.fillRoundedRectangle(trackBounds, trackWidth * 0.5f);
        
        // Draw "on" rail (gold) - from start to thumb position
        auto onTrack = trackBounds.withWidth(sliderPos - trackBounds.getX());
        g.setColour(goldColor);
        g.fillRoundedRectangle(onTrack, trackWidth * 0.5f);
        
        // Draw thumb (gold circle with black center)
        float thumbRadius = 12.0f;
        float thumbX = sliderPos;
        float thumbY = y + height * 0.5f;
        
        // Outer gold circle
        g.setColour(goldColor);
        g.fillEllipse(thumbX - thumbRadius, thumbY - thumbRadius, thumbRadius * 2, thumbRadius * 2);
        
        // Inner black circle
        float innerRadius = thumbRadius * 0.5f;
        g.setColour(blackColor);
        g.fillEllipse(thumbX - innerRadius, thumbY - innerRadius, innerRadius * 2, innerRadius * 2);
    }
    
private:
    juce::Colour goldColor;
    juce::Colour darkGoldColor;
    juce::Colour blackColor;
};

class StudioTab : public juce::Component,
                  public juce::Button::Listener,
                  public juce::Slider::Listener,
                  public juce::ComboBox::Listener,
                  private juce::Timer {
public:
    StudioTab(SubterraneumAudioProcessor& p);
    ~StudioTab() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    void buttonClicked(juce::Button* b) override;
    void sliderValueChanged(juce::Slider* s) override;
    void comboBoxChanged(juce::ComboBox* cb) override;
    void timerCallback() override;
    
    // Get current tempo/time signature for MIDI clock
    double getTempo() const { return currentTempo; }
    int getTimeSigNumerator() const { return timeSigNumerator; }
    int getTimeSigDenominator() const { return timeSigDenominator; }
    
private:
    SubterraneumAudioProcessor& processor;
    
    // ==========================================================================
    // Recording Section
    // ==========================================================================
    juce::GroupComponent recordingGroup { "recordingGroup", "Recording" };
    
    juce::TextButton recordBtn { "REC" };
    juce::TextButton stopBtn { "STOP" };
    juce::TextButton setFolderBtn { "Set Recording Folder" };  // FIX 2: New button
    juce::TextButton saveFileBtn { "Save File" };  // FIX 2: Renamed from "Show File"
    
    juce::Label recordingStatusLabel { "status", "Ready to record" };
    juce::Label recordingTimeLabel { "time", "00:00:00.000" };
    juce::Label recordingFormatLabel { "format", "24-bit / 44100 Hz / Stereo" };
    
    // Recording sample rate for display (fetched from processor)
    double recordingSampleRate = 44100.0;
    double recordingStartTime = 0.0;  // Milliseconds when recording started
    
    // FIX 2: Recording folder path storage
    juce::File recordingFolder;
    
    // Recording LED
    class RecordingLED : public juce::Component {
    public:
        void paint(juce::Graphics& g) override {
            auto bounds = getLocalBounds().toFloat();
            
            // Make it circular by using the smaller dimension
            float size = juce::jmin(bounds.getWidth(), bounds.getHeight()) - 4;
            auto ledBounds = juce::Rectangle<float>(
                bounds.getCentreX() - size / 2,
                bounds.getCentreY() - size / 2,
                size, size
            );
            
            if (isActive) {
                // Pulsing red when recording
                float pulse = 0.5f + 0.5f * std::sin(pulsePhase);
                g.setColour(juce::Colours::red.withAlpha(0.5f + 0.5f * pulse));
                g.fillEllipse(ledBounds.expanded(2));
            }
            
            g.setColour(isActive ? juce::Colours::red : juce::Colours::darkred.darker());
            g.fillEllipse(ledBounds);
            
            if (isActive) {
                g.setColour(juce::Colours::white.withAlpha(0.3f));
                g.fillEllipse(ledBounds.reduced(size * 0.3f).translated(-1, -1));
            }
        }
        
        void setActive(bool active) { 
            isActive = active; 
            repaint(); 
        }
        
        void pulse() {
            pulsePhase += 0.15f;
            if (pulsePhase > juce::MathConstants<float>::twoPi)
                pulsePhase -= juce::MathConstants<float>::twoPi;
            repaint();
        }
        
    private:
        bool isActive = false;
        float pulsePhase = 0.0f;
    };
    RecordingLED recordingLED;
    
    // ==========================================================================
    // Tempo Section
    // ==========================================================================
    juce::GroupComponent tempoGroup { "tempoGroup", "Master Tempo" };
    
    juce::Slider tempoSlider;
    juce::Label tempoLabel { "tempo", "BPM" };
    juce::Label tempoValueLabel { "tempoValue", "120.00" };
    
    juce::TextButton tapTempoBtn { "TAP" };
    std::vector<double> tapTimes;
    double lastTapTime = 0.0;
    
    double currentTempo = 120.0;
    
    // ==========================================================================
    // Time Signature Section
    // ==========================================================================
    juce::GroupComponent timeSignatureGroup { "timeSignatureGroup", "Time Signature" };
    
    juce::ComboBox numeratorCombo;
    juce::Label slashLabel { "slash", "/" };
    juce::ComboBox denominatorCombo;
    
    int timeSigNumerator = 4;
    int timeSigDenominator = 4;
    
    // ==========================================================================
    // Metronome Section (Optional)
    // ==========================================================================
    juce::GroupComponent metronomeGroup { "metronomeGroup", "Metronome" };
    juce::ToggleButton metronomeBtn { "Enable" };
    juce::Slider metronomeVolumeSlider;
    
    // ==========================================================================
    // Info Display
    // ==========================================================================
    juce::Label infoLabel { "info", "" };
    
    // Helper methods
    void startRecording();
    void stopRecording();
    void setRecordingFolder();  // FIX 2: New method
    void saveRecordingFile();   // FIX 2: New method (renamed from exportRecording)
    void updateRecordingTime();
    void updateTempoDisplay();
    void handleTapTempo();
    
    juce::String formatTime(double seconds);
    
    // FIX 2: Load/save recording folder preference
    void loadRecordingFolder();
    void saveRecordingFolderPreference();
    
    // FIX #3: Custom gold look and feel for sliders
    GoldSliderLookAndFeel goldSliderLookAndFeel;
};


