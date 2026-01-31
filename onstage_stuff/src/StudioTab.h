#pragma once

#include <JuceHeader.h>
#include "Style.h"

// Forward declaration
class SubterraneumAudioProcessor;

// =============================================================================
// StudioTab - Recording sessions, tempo, and time signature management
// =============================================================================
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
    juce::TextButton exportBtn { "Show File" };
    
    juce::Label recordingStatusLabel { "status", "Ready to record" };
    juce::Label recordingTimeLabel { "time", "00:00:00.000" };
    juce::Label recordingFormatLabel { "format", "24-bit / 44100 Hz / Stereo" };
    
    // Recording sample rate for display (fetched from processor)
    double recordingSampleRate = 44100.0;
    double recordingStartTime = 0.0;  // Milliseconds when recording started
    
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
    void exportRecording();
    void updateRecordingTime();
    void updateTempoDisplay();
    void handleTapTempo();
    juce::String formatTime(double seconds);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StudioTab)
};
