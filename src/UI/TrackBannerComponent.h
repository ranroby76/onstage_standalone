#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "PlaylistDataStructures.h"
#include "StyledSlider.h"
#include "LongPressDetector.h"

class TrackBannerComponent : public juce::Component, public LongPressDetector
{
public:
    TrackBannerComponent(int index, PlaylistItem& item, 
                         std::function<void()> onRemove,
                         std::function<void()> onExpandToggle,
                         std::function<void()> onBannerClick,
                         std::function<void()> onPlayButton,
                         std::function<void(float)> onVolChange,
                         std::function<void(float)> onSpeedChange);

    void paint(juce::Graphics& g) override;
    void resized() override;
    
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void onLongPress() override;

    void setPlaybackState(bool isCurrent, bool isAudioActive);
    bool isExpanded() const { return itemData.isExpanded; }

private:
    int trackIndex;
    PlaylistItem& itemData;
    
    bool isCurrentTrack = false;
    bool isAudioPlaying = false;

    std::function<void()> onRemoveCallback;
    std::function<void()> onExpandToggleCallback;
    std::function<void()> onBannerClickCallback;
    std::function<void()> onPlayButtonCallback;
    std::function<void(float)> onVolChangeCallback;
    std::function<void(float)> onSpeedChangeCallback;

    juce::Label indexLabel;
    MidiTooltipTextButton removeButton;
    MidiTooltipTextButton expandButton;
    MidiTooltipTextButton playButton;

    juce::Label volLabel, speedLabel, delayLabel;
    std::unique_ptr<StyledSlider> volSlider;
    std::unique_ptr<StyledSlider> speedSlider;
    std::unique_ptr<StyledSlider> delaySlider;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrackBannerComponent)
};
