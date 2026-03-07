// #D:\Workspace\onstage_colosseum_upgrade\src\VideoSurfaceComponent.h
#ifndef ONSTAGE_VIDEO_SURFACE_COMPONENT_H
#define ONSTAGE_VIDEO_SURFACE_COMPONENT_H

#include <juce_gui_basics/juce_gui_basics.h>

// Forward Declaration only — do NOT include PluginProcessor.h here
class SubterraneumAudioProcessor;

class VideoSurfaceComponent : public juce::Component, private juce::Timer
{
public:
    VideoSurfaceComponent(SubterraneumAudioProcessor& proc);
    ~VideoSurfaceComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    
    SubterraneumAudioProcessor& processor;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VideoSurfaceComponent)
};
#endif
