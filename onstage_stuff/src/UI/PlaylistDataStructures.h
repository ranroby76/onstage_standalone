#pragma once
#include <juce_core/juce_core.h>

struct PlaylistItem
{
    juce::String filePath;
    juce::String title;
    float volume = 1.0f; // 0.0 - 1.0
    float playbackSpeed = 1.0f; // 0.5 - 2.0
    int transitionDelaySec = 0;
    bool isCrossfade = false; 
    
    // NEW: Pitch (-12 to +12 semitones)
    float pitch = 0.0f; 
    
    bool isExpanded = false;
    
    void ensureTitle()
    {
        if (title.isEmpty())
            title = juce::File(filePath).getFileNameWithoutExtension();
    }
};