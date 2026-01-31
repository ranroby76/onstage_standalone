#pragma once

#include <JuceHeader.h>

namespace Style {
    const auto colBackground = juce::Colour(0xff1e1e1e);
    const auto colNodeBody   = juce::Colour(0xff2d2d2d);
    const auto colNodeHeader = juce::Colour(0xff3a3a3a);
    const auto colText       = juce::Colours::grey;
    const auto colPinAudio     = juce::Colour(0xff00aaff);
    const auto colPinSidechain = juce::Colour(0xff00ff66);
    const auto colPinMidi      = juce::Colour(0xffff4444);
    const auto colBypass       = juce::Colour(0xffd63031);
    
    const float nodeHeight = 60.0f;
    const float pinSize = 8.0f;
    const float hookLength = 10.0f; 
    const float btnSize = 14.0f;
    const float pinSpacing = 14.0f;
    const float minNodeWidth = 120.0f;
    
    const int mainHeaderHeight = 60;  // Doubled from 30 for logo display
    const int instrHeaderHeight = 70; 
    const int rightMenuWidth = 96;
}
