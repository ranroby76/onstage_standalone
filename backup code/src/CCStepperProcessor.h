// #D:\Workspace\Subterraneum_plugins_daw\src\CCStepperProcessor.h
// CC STEP SEQUENCER - 16-slot CC sequencer system tool
// Each slot has user-selectable MIDI channel (1-16)
// MIDI-only output (no audio buses)
// Features: variable step count, rate/type/order/speed/swing/smooth per slot
// Supports sync to master BPM and time signature via playhead

#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <array>

class CCStepperProcessor : public juce::AudioProcessor {
public:
    // =========================================================================
    // Enums
    // =========================================================================
    enum RateDiv {
        Rate_1_1 = 0, Rate_1_2, Rate_1_4, Rate_1_8,
        Rate_1_16, Rate_1_32, Rate_1_64, Rate_1_128,
        NumRates
    };

    enum RateType {
        TypeNormal = 0, TypeTriplet, TypeDotted,
        NumRateTypes
    };

    enum StepOrder {
        OrderForward = 0, OrderReverse, OrderPingPong, OrderRandom, OrderDrunk,
        NumOrders
    };

    enum SpeedMode {
        SpeedHalf = 0, SpeedNormal, SpeedDouble,
        NumSpeedModes
    };

    // =========================================================================
    // Constants
    // =========================================================================
    static constexpr int MaxSlots = 16;
    static constexpr int MaxSteps = 128;
    static constexpr int AllowedStepCounts[] = { 2, 4, 8, 16, 32, 64, 128 };
    static constexpr int NumAllowedStepCounts = 7;

    // =========================================================================
    // Slot - one CC sequence lane
    // =========================================================================
    struct Slot {
        bool enabled = false;
        int stepCount = 16;
        int midiChannel = 1;          // User-selectable 1-16
        int ccNumber = 1;             // 0-127
        RateDiv rate = Rate_1_8;
        RateType type = TypeNormal;
        StepOrder order = OrderForward;
        SpeedMode speed = SpeedNormal;
        bool smooth = false;
        float swing = 0.0f;           // 0-100%, 0 = no swing

        int steps[MaxSteps];

        // Runtime (not saved)
        double phase = 0.0;
        int currentStep = 0;
        int previousStep = 0;
        int lastSentCCValue = -1;
        bool pingPongForward = true;

        Slot() { for (int i = 0; i < MaxSteps; i++) steps[i] = 0; }

        void resetPlayback() {
            phase = 0.0;
            currentStep = 0;
            previousStep = 0;
            lastSentCCValue = -1;
            pingPongForward = true;
        }
    };

    // =========================================================================
    // Constructor / Destructor
    // =========================================================================
    CCStepperProcessor();
    ~CCStepperProcessor() override;

    // =========================================================================
    // AudioProcessor overrides
    // =========================================================================
    const juce::String getName() const override { return "Step Seq"; }

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    double getTailLengthSeconds() const override { return 0.0; }
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    // =========================================================================
    // Transport
    // =========================================================================
    void play();
    void stop();
    void togglePlayStop();
    bool isPlaying() const { return playing.load(); }

    void setBpm(double newBpm) { bpm.store(juce::jlimit(20.0, 400.0, newBpm)); }
    double getBpm() const { return bpm.load(); }
    double getEffectiveBpm() const { return effectiveBpm.load(); }

    // =========================================================================
    // Sync to Master
    // =========================================================================
    void setSyncToMasterBpm(bool s) { syncToMasterBpm.store(s); }
    bool isSyncToMasterBpm() const { return syncToMasterBpm.load(); }

    void setSyncToMasterTimeSig(bool s) { syncToMasterTimeSig.store(s); }
    bool isSyncToMasterTimeSig() const { return syncToMasterTimeSig.load(); }

    // =========================================================================
    // Time Signature (internal, used when not synced)
    // =========================================================================
    void setTimeSigNumerator(int n) { tsNumerator.store(juce::jlimit(1, 16, n)); }
    int getTimeSigNumerator() const { return tsNumerator.load(); }

    void setTimeSigDenominator(int d) { tsDenominator.store(juce::jlimit(1, 16, d)); }
    int getTimeSigDenominator() const { return tsDenominator.load(); }

    int getEffectiveTimeSigNum() const { return effectiveTsNum.load(); }
    int getEffectiveTimeSigDen() const { return effectiveTsDen.load(); }

    // =========================================================================
    // Slot access
    // =========================================================================
    Slot& getSlot(int index) { return slots[juce::jlimit(0, MaxSlots - 1, index)]; }
    const Slot& getSlot(int index) const { return slots[juce::jlimit(0, MaxSlots - 1, index)]; }

    int getSlotCurrentStep(int index) const {
        if (index >= 0 && index < MaxSlots) return slotCurrentStepAtomic[index].load();
        return 0;
    }

    bool slotHasData(int index) const;
    void clearSlot(int index);
    void copySlot(int srcIndex, int dstIndex);
    void copySlotToClipboard(int index);
    bool pasteSlotFromClipboard(int index);
    bool hasClipboardData() const { return clipboardValid; }

    // =========================================================================
    // Utility
    // =========================================================================
    static juce::String getRateName(RateDiv rate);
    static juce::String getTypeName(RateType type);
    static juce::String getOrderName(StepOrder order);
    static juce::String getSpeedName(SpeedMode speed);
    static double getRateMultiplier(RateDiv rate);
    static double getTypeMultiplier(RateType type);
    static double getSpeedMultiplier(SpeedMode speed);
    static int getNextAllowedStepCount(int current, bool increase);

    static constexpr const char* getIdentifier() { return "StepSeq"; }

private:
    void advanceStep(Slot& slot);
    int getNextStep(const Slot& slot) const;
    int computeCCValue(const Slot& slot) const;

    std::array<Slot, MaxSlots> slots;
    std::array<std::atomic<int>, MaxSlots> slotCurrentStepAtomic;

    Slot clipboard;
    bool clipboardValid = false;

    std::atomic<bool> playing { false };
    std::atomic<double> bpm { 120.0 };
    std::atomic<double> effectiveBpm { 120.0 };

    std::atomic<bool> syncToMasterBpm { false };
    std::atomic<bool> syncToMasterTimeSig { true };

    std::atomic<int> tsNumerator { 4 };
    std::atomic<int> tsDenominator { 4 };
    std::atomic<int> effectiveTsNum { 4 };
    std::atomic<int> effectiveTsDen { 4 };

    double currentSampleRate = 44100.0;
    juce::Random randomGen;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CCStepperProcessor)
};
