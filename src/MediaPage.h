// #D:\Workspace\onstage_colosseum_upgrade\src\MediaPage.h
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "VideoSurfaceComponent.h"
#include "PlaylistComponent.h"
#include "StyledSlider.h" 

class SubterraneumAudioProcessor;

// Custom LookAndFeel for glassy gradient buttons
class GlassyButtonLookAndFeel : public juce::LookAndFeel_V4
{
public:
    GlassyButtonLookAndFeel(bool green) : isGreen(green) {}
    
    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& /*backgroundColour*/,
                              bool isMouseOverButton, bool isButtonDown) override
    {
        auto bounds = button.getLocalBounds().toFloat();
        
        // Base colors
        juce::Colour baseColor, lightColor, darkColor;
        if (isGreen)
        {
            baseColor = juce::Colour(0xFF228B22);   // Forest green
            lightColor = juce::Colour(0xFF32CD32); // Lime green
            darkColor = juce::Colour(0xFF006400);  // Dark green
        }
        else
        {
            baseColor = juce::Colour(0xFFB22222);   // Firebrick
            lightColor = juce::Colour(0xFFDC143C); // Crimson
            darkColor = juce::Colour(0xFF8B0000);  // Dark red
        }
        
        // Brighten on hover/press
        if (isButtonDown)
        {
            lightColor = lightColor.brighter(0.2f);
            baseColor = baseColor.brighter(0.1f);
        }
        else if (isMouseOverButton)
        {
            lightColor = lightColor.brighter(0.1f);
        }
        
        // Main gradient (top to bottom)
        juce::ColourGradient mainGradient(
            lightColor, bounds.getX(), bounds.getY(),
            darkColor, bounds.getX(), bounds.getBottom(),
            false);
        mainGradient.addColour(0.5, baseColor);
        
        g.setGradientFill(mainGradient);
        g.fillRoundedRectangle(bounds, 6.0f);
        
        // Glass highlight (top half)
        auto highlightRect = bounds.withHeight(bounds.getHeight() * 0.45f);
        juce::ColourGradient glassGradient(
            juce::Colours::white.withAlpha(0.4f), highlightRect.getX(), highlightRect.getY(),
            juce::Colours::white.withAlpha(0.05f), highlightRect.getX(), highlightRect.getBottom(),
            false);
        g.setGradientFill(glassGradient);
        g.fillRoundedRectangle(highlightRect.reduced(1.0f), 5.0f);
        
        // Border
        g.setColour(darkColor.darker(0.3f));
        g.drawRoundedRectangle(bounds.reduced(0.5f), 6.0f, 1.5f);
    }
    
    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                        bool /*isMouseOverButton*/, bool /*isButtonDown*/) override
    {
        auto bounds = button.getLocalBounds();
        
        // Black text with slight shadow for better visibility
        g.setColour(juce::Colours::black.withAlpha(0.3f));
        g.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
        g.drawText(button.getButtonText(), bounds.translated(1, 1), juce::Justification::centred);
        
        g.setColour(juce::Colours::black);
        g.drawText(button.getButtonText(), bounds, juce::Justification::centred);
    }
    
private:
    bool isGreen;
};

class MediaPage : public juce::Component, private juce::Timer
{
public:
    MediaPage(SubterraneumAudioProcessor& proc);
    ~MediaPage() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    juce::String formatTime(double seconds) const;

    SubterraneumAudioProcessor& processor;
    
    std::unique_ptr<VideoSurfaceComponent> videoSurface;
    std::unique_ptr<PlaylistComponent> playlistComponent;
    
    MidiTooltipTextButton playPauseBtn; 
    MidiTooltipTextButton stopBtn;
    
    // Glassy button LookAndFeels
    GlassyButtonLookAndFeel greenButtonLF { true };
    GlassyButtonLookAndFeel redButtonLF { false };
    
    StyledSlider progressSlider;
    juce::Label currentTimeLabel;
    juce::Label totalTimeLabel;
    
    bool isUserDraggingSlider = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MediaPage)
};
