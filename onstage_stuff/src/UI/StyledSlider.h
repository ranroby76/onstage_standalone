// **Changes:** 1.  Included `LongPressDetector.h`. 2.  Updated `StyledSlider`, `MidiTooltipToggleButton`, `MidiTooltipTextButton`, and `MidiTooltipLabel` to inherit `LongPressDetector`. 3.  Implemented `onLongPress()` to show the tooltip. 4.  Updated mouse handlers to call `handleMouseDown`, `handleMouseDrag`, etc. <!-- end list -->

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "LongPressDetector.h"

class GoldenSliderLookAndFeel : public juce::LookAndFeel_V4
{
public:
    GoldenSliderLookAndFeel()
    {
        setColour(juce::Slider::thumbColourId, juce::Colour(0xffd4af37));
        setColour(juce::Slider::trackColourId, juce::Colour(0xff202020)); 
        setColour(juce::Slider::backgroundColourId, juce::Colour(0xff202020));
        setColour(juce::ScrollBar::thumbColourId, juce::Colour(0xffd4af37)); 
        setColour(juce::ScrollBar::trackColourId, juce::Colour(0xff1a1a1a));
        setColour(juce::ScrollBar::backgroundColourId, juce::Colour(0xff1a1a1a));
        setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xFFD4AF37)); 
        setColour(juce::ComboBox::textColourId, juce::Colours::black); 
        setColour(juce::ComboBox::arrowColourId, juce::Colours::black);
        setColour(juce::ComboBox::outlineColourId, juce::Colours::black);
        setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xFFD4AF37));
        setColour(juce::PopupMenu::textColourId, juce::Colours::black);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colours::black);
        setColour(juce::PopupMenu::highlightedTextColourId, juce::Colour(0xFFD4AF37));
        setColour(juce::TabbedButtonBar::frontOutlineColourId, juce::Colours::transparentBlack);
    }

    int getTabButtonOverlap (int) override { return 0; }
    int getTabButtonSpaceAroundImage () override { return 0; }

    void drawTabButton (juce::TabBarButton& button, juce::Graphics& g, bool isMouseOver, bool isMouseDown) override
    {
        auto activeArea = button.getActiveArea();
        const auto isFrontTab = button.isFrontTab();
        
        juce::Colour bgColour = isFrontTab ? juce::Colour(0xFF202020) : juce::Colour(0xFFD4AF37);
        juce::Colour textColour = isFrontTab ? juce::Colour(0xFFD4AF37) : juce::Colours::black;

        g.setColour(bgColour);
        g.fillRect(activeArea);
        
        if (!isFrontTab)
        {
            g.setColour(juce::Colour(0xFF202020)); 
            g.fillRect(activeArea.removeFromRight(1));
        }
        
        g.setColour(textColour);
        g.setFont(juce::Font(16.0f, juce::Font::bold));
        g.drawText(button.getButtonText(), activeArea, juce::Justification::centred, true);
    }

    void drawTabbedButtonBarBackground (juce::TabbedButtonBar& bar, juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xFF202020));
    }
    
    void drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box) override {
        auto cornerSize = box.findParentComponentOfClass<juce::GroupComponent>() != nullptr ? 0.0f : 3.0f;
        juce::Rectangle<int> boxBounds (0, 0, width, height);
        g.setColour (findColour(juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle (boxBounds.toFloat(), cornerSize);
        g.setColour (findColour(juce::ComboBox::outlineColourId));
        g.drawRoundedRectangle (boxBounds.toFloat().reduced (0.5f, 0.5f), cornerSize, 1.0f);
        juce::Path triangle;
        float arrowSize = 10.0f;
        float arrowX = width - 15.0f;
        float arrowY = height * 0.5f;
        triangle.addTriangle(arrowX - arrowSize * 0.5f, arrowY - arrowSize * 0.25f,
                             arrowX + arrowSize * 0.5f, arrowY - arrowSize * 0.25f,
                             arrowX, arrowY + arrowSize * 0.25f);
        g.setColour(findColour(juce::ComboBox::arrowColourId)); 
        g.fillPath(triangle);
    }

    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
        float sliderPos, float minSliderPos, float maxSliderPos,
        const juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        bool isVertical = (style == juce::Slider::LinearVertical);
        float trackWidth = isVertical ? (float)width * 0.2f : (float)height * 0.25f;
        trackWidth = juce::jlimit(4.0f, 8.0f, trackWidth);
        juce::Rectangle<float> trackBounds;
        if (isVertical) {
            float centerX = x + width * 0.5f;
            trackBounds = juce::Rectangle<float>(centerX - trackWidth * 0.5f, (float)y + 5, trackWidth, (float)height - 10);
        } else {
            float centerY = y + height * 0.5f;
            trackBounds = juce::Rectangle<float>((float)x + 5, centerY - trackWidth * 0.5f, (float)width - 10, trackWidth);
        }
        g.setColour(juce::Colour(0xFF151515));
        g.fillRoundedRectangle(trackBounds, trackWidth * 0.5f);
        g.setColour(juce::Colour(0xFF333333));
        g.drawRoundedRectangle(trackBounds, trackWidth * 0.5f, 1.0f);
        juce::Rectangle<float> fillBounds = trackBounds;
        if (isVertical) {
            fillBounds.setTop(sliderPos);
            fillBounds.setBottom(trackBounds.getBottom());
        } else {
            fillBounds.setRight(sliderPos);
            fillBounds.setWidth(sliderPos - trackBounds.getX());
        }
        if (!fillBounds.isEmpty()) {
            g.setColour(juce::Colour(0xFFD4AF37));
            g.fillRoundedRectangle(fillBounds, trackWidth * 0.5f);
        }
        float thumbSize = isVertical ? (float)width * 0.7f : (float)height * 0.7f;
        thumbSize = juce::jlimit(14.0f, 20.0f, thumbSize);
        float thumbX = isVertical ? x + width * 0.5f - thumbSize * 0.5f : sliderPos - thumbSize * 0.5f;
        float thumbY = isVertical ? sliderPos - thumbSize * 0.5f : y + height * 0.5f - thumbSize * 0.5f;
        juce::Rectangle<float> thumbBounds(thumbX, thumbY, thumbSize, thumbSize);
        g.setColour(juce::Colour(0xFFD4AF37));
        g.fillEllipse(thumbBounds);
        g.setColour(juce::Colours::black);
        g.fillEllipse(thumbBounds.reduced(3.0f)); 
    }

    void drawScrollbar (juce::Graphics& g, juce::ScrollBar& scrollbar, int x, int y, int width, int height,
                        bool isScrollbarVertical, int thumbStartPosition, int thumbSize,
                        bool, bool) override
    {
        g.fillAll (findColour (juce::ScrollBar::backgroundColourId));
        juce::Rectangle<int> thumbBounds;
        if (isScrollbarVertical) thumbBounds = { x + 2, thumbStartPosition, width - 4, thumbSize };
        else thumbBounds = { thumbStartPosition, y + 2, thumbSize, height - 4 };
        g.setColour(findColour(juce::ScrollBar::thumbColourId)); 
        g.fillRoundedRectangle(thumbBounds.toFloat(), 4.0f);
    }
};

// --- TOOLTIP HELPERS ---
class TooltipAutoHideTimer : public juce::Timer {
public: TooltipAutoHideTimer(juce::Component* t) : tooltip(t) {}
    void timerCallback() override { stopTimer(); if (tooltip) tooltip->setVisible(false); }
private: juce::Component* tooltip;
};
class MidiTooltipHelper : public juce::BubbleMessageComponent {
public: MidiTooltipHelper() : hideTimer(this) { setAlwaysOnTop(true); addToDesktop(0); }
    void show(juce::Component* owner, const juce::String& text) {
        juce::AttributedString attString;
        attString.append(text, juce::Font(15.0f, juce::Font::bold), juce::Colours::white);
        showAt(owner->getScreenBounds(), attString, 2000, true, false);
        hideTimer.startTimer(2500);
    }
private: TooltipAutoHideTimer hideTimer;
};
inline void showMidiTooltip(juce::Component* c, const juce::String& m) {
    if (m.isEmpty()) return; static std::unique_ptr<MidiTooltipHelper> t;
    if (!t) t = std::make_unique<MidiTooltipHelper>(); t->show(c, m);
}

// --- WIDGET WRAPPERS ---

class StyledSlider : public juce::Slider, public LongPressDetector {
public:
    StyledSlider(juce::Slider::SliderStyle style = juce::Slider::RotaryVerticalDrag, juce::Slider::TextEntryBoxPosition textBox = juce::Slider::TextBoxBelow)
        : juce::Slider(style, textBox) { setLookAndFeel(&goldenLookAndFeel); setTextBoxStyle(textBox, false, 60, 18); }
    ~StyledSlider() override { setLookAndFeel(nullptr); }
    void setMidiInfo(const juce::String& info) { midiInfo = info; }
    
    // Long Press: Show Tooltip
    void onLongPress() override {
        if (midiInfo.isNotEmpty()) showMidiTooltip(this, midiInfo);
    }

    void mouseDown(const juce::MouseEvent& e) override {
        if (e.mods.isRightButtonDown()) { if (midiInfo.isNotEmpty()) showMidiTooltip(this, midiInfo); return; }
        handleMouseDown(e); // Touch Support
        juce::Slider::mouseDown(e);
    }
    void mouseUp(const juce::MouseEvent& e) override { 
        handleMouseUp(e); // Stop timer
        if (e.mods.isRightButtonDown() || isLongPressTriggered) return; // Block if long press occurred
        juce::Slider::mouseUp(e); 
    }
    void mouseDrag(const juce::MouseEvent& e) override { 
        handleMouseDrag(e); // Stop timer if moved
        if (e.mods.isRightButtonDown() || isLongPressTriggered) return; 
        juce::Slider::mouseDrag(e); 
    }
private:
    GoldenSliderLookAndFeel goldenLookAndFeel;
    juce::String midiInfo;
};

class VerticalSlider : public juce::Component {
public:
    VerticalSlider() {
        slider.setSliderStyle(juce::Slider::LinearVertical);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 20);
        addAndMakeVisible(slider);
        label.setJustificationType(juce::Justification::centred);
        label.setColour(juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible(label);
    }
    StyledSlider& getSlider() { return slider; }
    void setLabelText(const juce::String& text) { label.setText(text, juce::dontSendNotification); }
    void setRange(double min, double max, double interval = 0) { slider.setRange(min, max, interval); }
    void setValue(double value, juce::NotificationType notification = juce::dontSendNotification) { slider.setValue(value, notification); }
    double getValue() const { return slider.getValue(); }
    void setTextValueSuffix(const juce::String& suffix) { slider.setTextValueSuffix(suffix); }
    void setNumDecimalPlacesToDisplay(int places) { slider.setNumDecimalPlacesToDisplay(places); }
    void setSkewFactor(double factor) { slider.setSkewFactorFromMidPoint(factor); }
    void setMidiInfo(const juce::String& info) { midiInfo = info; slider.setMidiInfo(info); label.setMidiInfo(info); }
    void resized() override { auto area = getLocalBounds(); label.setBounds(area.removeFromTop(20)); slider.setBounds(area); }
private:
    StyledSlider slider;
    // Label also needs long press support for tooltip
    class InternalLabel : public juce::Label, public LongPressDetector {
    public: 
        void setMidiInfo(const juce::String& info) { midiInfo = info; }
        void onLongPress() override { if (midiInfo.isNotEmpty()) showMidiTooltip(this, midiInfo); }
        void mouseDown(const juce::MouseEvent& e) override { 
            if (e.mods.isRightButtonDown()) { if (midiInfo.isNotEmpty()) showMidiTooltip(this, midiInfo); return; } 
            handleMouseDown(e);
            juce::Label::mouseDown(e); 
        }
        void mouseUp(const juce::MouseEvent& e) override { 
            handleMouseUp(e); 
            if (e.mods.isRightButtonDown() || isLongPressTriggered) return;
            juce::Label::mouseUp(e); 
        }
        void mouseDrag(const juce::MouseEvent& e) override {
            handleMouseDrag(e);
            if (!isLongPressTriggered) juce::Label::mouseDrag(e);
        }
        juce::String midiInfo;
    };
    InternalLabel label;
    juce::String midiInfo;
};

class MidiTooltipToggleButton : public juce::ToggleButton, public LongPressDetector {
public: MidiTooltipToggleButton(const juce::String& text = "") : juce::ToggleButton(text) {}
    void setMidiInfo(const juce::String& info) { midiInfo = info; }
    void onLongPress() override { if (midiInfo.isNotEmpty()) showMidiTooltip(this, midiInfo); }
    void mouseDown(const juce::MouseEvent& e) override { 
        if (e.mods.isRightButtonDown()) { if (midiInfo.isNotEmpty()) showMidiTooltip(this, midiInfo); return; } 
        handleMouseDown(e);
        juce::ToggleButton::mouseDown(e);
    }
    void mouseUp(const juce::MouseEvent& e) override { 
        handleMouseUp(e);
        if (e.mods.isRightButtonDown() || isLongPressTriggered) return; 
        juce::ToggleButton::mouseUp(e); 
    }
    void mouseDrag(const juce::MouseEvent& e) override { handleMouseDrag(e); if (!isLongPressTriggered) juce::ToggleButton::mouseDrag(e); }
private: juce::String midiInfo;
};

class MidiTooltipTextButton : public juce::TextButton, public LongPressDetector {
public: MidiTooltipTextButton(const juce::String& text = "") : juce::TextButton(text) {}
    void setMidiInfo(const juce::String& info) { midiInfo = info; }
    void onLongPress() override { if (midiInfo.isNotEmpty()) showMidiTooltip(this, midiInfo); }
    void mouseDown(const juce::MouseEvent& e) override { 
        if (e.mods.isRightButtonDown()) { if (midiInfo.isNotEmpty()) showMidiTooltip(this, midiInfo); return; } 
        handleMouseDown(e);
        juce::TextButton::mouseDown(e);
    }
    void mouseUp(const juce::MouseEvent& e) override { 
        handleMouseUp(e);
        if (e.mods.isRightButtonDown() || isLongPressTriggered) return; 
        juce::TextButton::mouseUp(e); 
    }
    void mouseDrag(const juce::MouseEvent& e) override { handleMouseDrag(e); if (!isLongPressTriggered) juce::TextButton::mouseDrag(e); }
private: juce::String midiInfo;
};

class MidiTooltipLabel : public juce::Label, public LongPressDetector {
public: MidiTooltipLabel(const juce::String& name = "", const juce::String& text = "") : juce::Label(name, text) {}
    void setMidiInfo(const juce::String& info) { midiInfo = info; }
    void onLongPress() override { if (midiInfo.isNotEmpty()) showMidiTooltip(this, midiInfo); }
    void mouseDown(const juce::MouseEvent& e) override { 
        if (e.mods.isRightButtonDown()) { if (midiInfo.isNotEmpty()) showMidiTooltip(this, midiInfo); return; } 
        handleMouseDown(e);
        juce::Label::mouseDown(e);
    }
    void mouseUp(const juce::MouseEvent& e) override { handleMouseUp(e); if (e.mods.isRightButtonDown() || isLongPressTriggered) return; juce::Label::mouseUp(e); }
    void mouseDrag(const juce::MouseEvent& e) override { handleMouseDrag(e); if (!isLongPressTriggered) juce::Label::mouseDrag(e); }
private: juce::String midiInfo;
};