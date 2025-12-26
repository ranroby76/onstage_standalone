// This ensures a clean forward declaration to prevent circular include loops.

#ifndef ONSTAGE_ENGINE_VIDEO_SURFACE_COMPONENT_H
#define ONSTAGE_ENGINE_VIDEO_SURFACE_COMPONENT_H

#include <juce_gui_basics/juce_gui_basics.h>

// Forward Declaration only - do NOT include AudioEngine.h here
class AudioEngine; 

class VideoSurfaceComponent : public juce::Component, private juce::Timer
{
public:
    VideoSurfaceComponent(AudioEngine& engine);
    ~VideoSurfaceComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    
    AudioEngine& audioEngine;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VideoSurfaceComponent)
};
#endif
