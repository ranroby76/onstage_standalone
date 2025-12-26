// **Fix:** Updated the constructor to accept `AudioEngine&` instead of the old player reference, matching the header file we updated earlier.

#include "VideoSurfaceComponent.h"
#include "../AudioEngine.h" // Include full definition here

VideoSurfaceComponent::VideoSurfaceComponent(AudioEngine& engine)
    : audioEngine(engine)
{
    setOpaque(true);
    startTimerHz(60);
}

VideoSurfaceComponent::~VideoSurfaceComponent()
{
    stopTimer();
}

void VideoSurfaceComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
    
    // Safely access the ACTIVE player via the engine
    juce::Image frame = audioEngine.getMediaPlayer().getCurrentVideoFrame();
    
    if (frame.isValid())
    {
        g.drawImage(frame, getLocalBounds().toFloat(), 
                    juce::RectanglePlacement::centred);
    }
}

void VideoSurfaceComponent::resized()
{
}

void VideoSurfaceComponent::timerCallback()
{
    repaint();
}
