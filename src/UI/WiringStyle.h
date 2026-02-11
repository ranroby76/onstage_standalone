// ==============================================================================
//  WiringStyle.h
//  OnStage — Colors, dimensions, and constants for the WiringCanvas
//
//  Adapted from Colosseum's Style.h with OnStage golden accent colors.
// ==============================================================================

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace WiringStyle
{
    // --- Canvas background ---------------------------------------------------
    const auto colBackground        = juce::Colour (0xFF1E1E1E);

    // --- Node body -----------------------------------------------------------
    const auto colNodeBody          = juce::Colour (0xFF2D2D2D);
    const auto colNodeBodyBypassed  = juce::Colour (0xFF3A3A3A);
    const auto colNodeBorder        = juce::Colour (0xFF555555);
    const auto colNodeBorderActive  = juce::Colour (0xFFD4AF37);  // Gold (OnStage accent)
    const auto colNodeTitle         = juce::Colour (0xFF505050);
    const auto colNodeTitleBypassed = juce::Colour (0xFF404040);
    const auto colIONodeBody        = juce::Colour (0xFF404040);

    // --- Guitar node colours (deep purple theme) -----------------------------
    const auto colGuitarNodeTitle   = juce::Colour (0xFF663399);
    const auto colGuitarNodeBody    = juce::Colour (0xFF2A1A3D);
    const auto colGuitarNodeBorder  = juce::Colour (0xFF9B59B6);

    // --- Pins ----------------------------------------------------------------
    const auto colPinAudio          = juce::Colour (0xFF00AAFF);  // Blue
    const auto colPinSidechain      = juce::Colour (0xFF00FF00);  // Green
    const auto colPinHover          = juce::Colour (0xFFCC88FF);  // Light purple on hover
    const auto colPinValidTarget    = juce::Colours::yellow;

    // --- Wires ---------------------------------------------------------------
    const auto colWireIdle          = juce::Colours::darkgrey;
    const auto colWireHover         = juce::Colour (0xFFCC88FF);
    const auto colWireSignal        = juce::Colour (0xFF00AAFF);  // Bright blue with signal

    // --- Buttons (B / E / X) -------------------------------------------------
    const auto colBypass            = juce::Colour (0xFFD63031);  // Red
    const auto colEditor            = juce::Colour (0xFFD4AF37);  // Gold (OnStage)
    const auto colDelete            = juce::Colour (0xFFE74C3C);  // Red
    const auto colBtnBackground     = juce::Colour (0xFF3A3A3A);

    // --- Text ----------------------------------------------------------------
    const auto colText              = juce::Colours::grey;
    const auto colTextBright        = juce::Colours::white;

    // --- Dimensions ----------------------------------------------------------
    constexpr float nodeWidth        = 140.0f;
    constexpr float nodeHeight       = 60.0f;
    constexpr float nodeRounding     = 5.0f;
    constexpr float nodeTitleHeight  = 24.0f;
    constexpr float minNodeWidth     = 120.0f;

    // Pins
    constexpr float pinSize          = 8.0f;
    constexpr float pinSpacing       = 14.0f;
    constexpr float minPinSpacing    = 18.0f;

    // Bottom buttons (B / E / X)
    constexpr float btnWidth         = 20.0f;
    constexpr float btnHeight        = 20.0f;
    constexpr float btnSpacing       = 4.0f;
    constexpr float btnMargin        = 4.0f;

    // I/O nodes get special sizing
    constexpr float ioNodeMinWidth   = 160.0f;

    // Grid
    constexpr int   gridSize         = 40;

    // --- Recorder node dimensions --------------------------------------------
    constexpr float recorderNodeWidth  = 360.0f;
    constexpr float recorderNodeHeight = 160.0f;
}
