// #D:\Workspace\onstage_colosseum_upgrade\src\PlaylistDataStructures.h
#pragma once
#include <juce_core/juce_core.h>

struct PlaylistItem
{
    juce::String filePath;
    juce::String title;
    float volume = 1.0f;           // 0.0 - 2.0
    float playbackSpeed = 1.0f;    // 0.5 - 1.5
    int transitionDelaySec = 0;
    bool isCrossfade = false; 
    float pitch = 0.0f;            // -12 to +12 semitones (future use)
    bool isExpanded = false;
    
    void ensureTitle()
    {
        if (title.isEmpty())
            title = juce::File(filePath).getFileNameWithoutExtension();
    }
};
