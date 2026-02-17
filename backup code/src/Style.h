#pragma once

#include <JuceHeader.h>

namespace Style {
    const auto colBackground = juce::Colour(0xff1e1e1e);
    const auto colNodeBg = juce::Colour(0xff2d2d2d);
    const auto colNodeBypassed = juce::Colour(0xff3a3a3a);
    const auto colNodeBodyBypassed = juce::Colour(0xff3a3a3a);  // Body color when bypassed
    const auto colIONode = juce::Colour(0xff404040);
    const auto colNodeBody   = juce::Colour(0xff2d2d2d);
    const auto colNodeHeader = juce::Colour(0xff3a3a3a);
    const auto colNodeBorder = juce::Colour(0xff555555);
    const auto colNodeBorderActive = juce::Colour(0xff00aaff);
    const auto colNodeTitle = juce::Colour(0xff505050);
    const auto colNodeTitleBypassed = juce::Colour(0xff404040);  // Title color when bypassed
    const auto colText       = juce::Colours::grey;
    const auto colPinAudio     = juce::Colour(0xff00aaff);  // Blue
    const auto colPinSidechain = juce::Colour(0xff00ff00);  // Pure Green
    const auto colPinMidi      = juce::Colour(0xffff0000);  // Pure Red
    const auto colBypass       = juce::Colour(0xffd63031);
    
    const float nodeHeight = 60.0f;
    const float nodeWidth = 152.0f;     // FIX: wider to fit up to 6 buttons (VST2: E+M+P+T+L+X)
    const float nodeCornerSize = 5.0f;
    const float nodeRounding = 5.0f;  // Rounding radius for node corners
    const float nodeTitleHeight = 24.0f;
    const float pinSize = 8.0f;
    const float hookLength = 10.0f; 
    const float btnSize = 14.0f;
    const float pinSpacing = 14.0f;
    const float minNodeWidth = 156.0f;  // FIX: minimum with margin for 6 buttons
    const float minPinSpacing = 18.0f;
    
    const float bottomBtnWidth = 20.0f;
    const float bottomBtnHeight = 20.0f;
    const float bottomBtnSpacing = 4.0f;
    const float bottomBtnMargin = 4.0f;
    
    const int mainHeaderHeight = 60;  // Doubled from 30 for logo display
    const int instrHeaderHeight = 140;  // FIX: Doubled from 70 for taller instrument buttons
    const int rightMenuWidth = 96;
    const int leftMenuWidth = 96;  // FIX: Left green tab menu
    const auto colLeftMenu = juce::Colour(0xff2E7D32);  // FIX: Dark green (darker than before)
}
