// #D:\Workspace\onstage_colosseum_upgrade\src\VideoSurfaceComponent.cpp
#include "VideoSurfaceComponent.h"
#include "PluginProcessor.h"

VideoSurfaceComponent::VideoSurfaceComponent(SubterraneumAudioProcessor& proc)
    : processor(proc)
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
    
    juce::Image frame = processor.getMediaPlayer().getCurrentVideoFrame();
    
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
