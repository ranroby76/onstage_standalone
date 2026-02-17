// #D:\Workspace\Subterraneum_plugins_daw\src\CCStepperProcessor.cpp
// CC STEP SEQUENCER - MIDI-only, 16-slot, phase-accumulator based

#include "CCStepperProcessor.h"

CCStepperProcessor::CCStepperProcessor()
    : AudioProcessor(BusesProperties())  // No audio buses - MIDI only
{
    for (int i = 0; i < MaxSlots; i++)
    {
        slots[i] = Slot();
        slots[i].midiChannel = i + 1;  // Default: slot N -> channel N+1
        slotCurrentStepAtomic[i].store(0);
    }
}

CCStepperProcessor::~CCStepperProcessor() {}

void CCStepperProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate;
}

void CCStepperProcessor::releaseResources() {}

bool CCStepperProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // MIDI-only: accept disabled/empty audio layouts
    return layouts.getMainInputChannelSet().isDisabled()
        && layouts.getMainOutputChannelSet().isDisabled();
}

// =============================================================================
// processBlock
// =============================================================================

void CCStepperProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    buffer.clear();

    if (!playing.load())
        return;

    // --- Sync to master BPM/TimeSig from playhead ---
    double useBpm = bpm.load();
    int useTsNum = tsNumerator.load();
    int useTsDen = tsDenominator.load();

    if (auto* playHead = getPlayHead())
    {
        auto posInfo = playHead->getPosition();
        if (posInfo.hasValue())
        {
            if (syncToMasterBpm.load())
            {
                auto bpmOpt = posInfo->getBpm();
                if (bpmOpt.hasValue() && *bpmOpt > 0.0)
                    useBpm = *bpmOpt;
            }

            if (syncToMasterTimeSig.load())
            {
                auto tsOpt = posInfo->getTimeSignature();
                if (tsOpt.hasValue())
                {
                    useTsNum = tsOpt->numerator;
                    useTsDen = tsOpt->denominator;
                }
            }
        }
    }

    effectiveBpm.store(useBpm);
    effectiveTsNum.store(useTsNum);
    effectiveTsDen.store(useTsDen);

    const int numSamples = buffer.getNumSamples();
    const double beatsPerSecond = useBpm / 60.0;
    const double beatsPerSample = beatsPerSecond / currentSampleRate;

    for (int slotIdx = 0; slotIdx < MaxSlots; slotIdx++)
    {
        auto& slot = slots[slotIdx];
        if (!slot.enabled || slot.stepCount < 2)
            continue;

        // Step duration in beats (quarter notes)
        double stepBeats = getRateMultiplier(slot.rate) * 4.0;
        stepBeats *= getTypeMultiplier(slot.type);
        stepBeats *= getSpeedMultiplier(slot.speed);

        // Swing: 0% = no swing, 100% = max swing
        // Odd steps get shortened, even steps get lengthened
        double swingAmount = (double)slot.swing / 100.0;
        double effectiveStepBeats = stepBeats;
        if (slot.currentStep % 2 == 0 && swingAmount > 0.0)
            effectiveStepBeats = stepBeats * (1.0 + swingAmount * 0.5);
        else if (slot.currentStep % 2 == 1 && swingAmount > 0.0)
            effectiveStepBeats = stepBeats * (1.0 - swingAmount * 0.5);

        if (effectiveStepBeats < 0.001) effectiveStepBeats = 0.001;

        double phaseInc = beatsPerSample / effectiveStepBeats;

        for (int sample = 0; sample < numSamples; sample++)
        {
            slot.phase += phaseInc;

            if (slot.phase >= 1.0)
            {
                slot.phase -= 1.0;
                slot.previousStep = slot.currentStep;
                advanceStep(slot);

                if (!slot.smooth)
                {
                    int ccVal = juce::jlimit(0, 127, slot.steps[slot.currentStep]);
                    if (ccVal != slot.lastSentCCValue)
                    {
                        midiMessages.addEvent(
                            juce::MidiMessage::controllerEvent(slot.midiChannel, slot.ccNumber, ccVal),
                            sample);
                        slot.lastSentCCValue = ccVal;
                    }
                }
            }

            if (slot.smooth)
            {
                int ccVal = computeCCValue(slot);
                if (ccVal != slot.lastSentCCValue)
                {
                    midiMessages.addEvent(
                        juce::MidiMessage::controllerEvent(slot.midiChannel, slot.ccNumber, ccVal),
                        sample);
                    slot.lastSentCCValue = ccVal;
                }
            }
        }

        slotCurrentStepAtomic[slotIdx].store(slot.currentStep);
    }
}

// =============================================================================
// Step advancement
// =============================================================================

void CCStepperProcessor::advanceStep(Slot& slot)
{
    slot.currentStep = getNextStep(slot);
}

int CCStepperProcessor::getNextStep(const Slot& slot) const
{
    const int count = slot.stepCount;
    int cur = slot.currentStep;

    switch (slot.order)
    {
        case OrderForward:  return (cur + 1) % count;
        case OrderReverse:  return (cur - 1 + count) % count;
        case OrderPingPong:
        {
            if (slot.pingPongForward)
            {
                if (cur + 1 >= count)
                {
                    const_cast<Slot&>(slot).pingPongForward = false;
                    return juce::jmax(0, cur - 1);
                }
                return cur + 1;
            }
            else
            {
                if (cur - 1 < 0)
                {
                    const_cast<Slot&>(slot).pingPongForward = true;
                    return juce::jmin(count - 1, cur + 1);
                }
                return cur - 1;
            }
        }
        case OrderRandom:
            return const_cast<juce::Random&>(randomGen).nextInt(count);
        case OrderDrunk:
        {
            int delta = const_cast<juce::Random&>(randomGen).nextInt(3) - 1;
            int next = cur + delta;
            if (next < 0) next = count - 1;
            if (next >= count) next = 0;
            return next;
        }
        default: return (cur + 1) % count;
    }
}

int CCStepperProcessor::computeCCValue(const Slot& slot) const
{
    if (!slot.smooth)
        return juce::jlimit(0, 127, slot.steps[slot.currentStep]);

    int prevVal = slot.steps[slot.previousStep];
    int curVal = slot.steps[slot.currentStep];
    double interpolated = prevVal + (curVal - prevVal) * slot.phase;
    return juce::jlimit(0, 127, (int)std::round(interpolated));
}

// =============================================================================
// Transport
// =============================================================================

void CCStepperProcessor::play() { playing.store(true); }

void CCStepperProcessor::stop()
{
    playing.store(false);
    for (int i = 0; i < MaxSlots; i++)
    {
        slots[i].resetPlayback();
        slotCurrentStepAtomic[i].store(0);
    }
}

void CCStepperProcessor::togglePlayStop()
{
    if (playing.load()) stop(); else play();
}

// =============================================================================
// Slot operations
// =============================================================================

bool CCStepperProcessor::slotHasData(int index) const
{
    if (index < 0 || index >= MaxSlots) return false;
    const auto& slot = slots[index];
    for (int i = 0; i < slot.stepCount; i++)
        if (slot.steps[i] != 0) return true;
    return false;
}

void CCStepperProcessor::clearSlot(int index)
{
    if (index < 0 || index >= MaxSlots) return;
    auto& slot = slots[index];
    for (int i = 0; i < MaxSteps; i++) slot.steps[i] = 0;
    slot.resetPlayback();
}

void CCStepperProcessor::copySlot(int srcIndex, int dstIndex)
{
    if (srcIndex < 0 || srcIndex >= MaxSlots) return;
    if (dstIndex < 0 || dstIndex >= MaxSlots) return;
    if (srcIndex == dstIndex) return;

    auto& src = slots[srcIndex];
    auto& dst = slots[dstIndex];
    dst.enabled = src.enabled;
    dst.stepCount = src.stepCount;
    dst.midiChannel = src.midiChannel;
    dst.ccNumber = src.ccNumber;
    dst.rate = src.rate;
    dst.type = src.type;
    dst.order = src.order;
    dst.speed = src.speed;
    dst.smooth = src.smooth;
    dst.swing = src.swing;
    std::memcpy(dst.steps, src.steps, sizeof(dst.steps));
    dst.resetPlayback();
}

void CCStepperProcessor::copySlotToClipboard(int index)
{
    if (index < 0 || index >= MaxSlots) return;
    auto& src = slots[index];
    clipboard.enabled = src.enabled;
    clipboard.stepCount = src.stepCount;
    clipboard.midiChannel = src.midiChannel;
    clipboard.ccNumber = src.ccNumber;
    clipboard.rate = src.rate;
    clipboard.type = src.type;
    clipboard.order = src.order;
    clipboard.speed = src.speed;
    clipboard.smooth = src.smooth;
    clipboard.swing = src.swing;
    std::memcpy(clipboard.steps, src.steps, sizeof(clipboard.steps));
    clipboardValid = true;
}

bool CCStepperProcessor::pasteSlotFromClipboard(int index)
{
    if (!clipboardValid || index < 0 || index >= MaxSlots) return false;
    auto& dst = slots[index];
    dst.enabled = clipboard.enabled;
    dst.stepCount = clipboard.stepCount;
    dst.midiChannel = clipboard.midiChannel;
    dst.ccNumber = clipboard.ccNumber;
    dst.rate = clipboard.rate;
    dst.type = clipboard.type;
    dst.order = clipboard.order;
    dst.speed = clipboard.speed;
    dst.smooth = clipboard.smooth;
    dst.swing = clipboard.swing;
    std::memcpy(dst.steps, clipboard.steps, sizeof(dst.steps));
    dst.resetPlayback();
    return true;
}

// =============================================================================
// Utility - static
// =============================================================================

juce::String CCStepperProcessor::getRateName(RateDiv rate)
{
    switch (rate)
    {
        case Rate_1_1:   return "1/1";
        case Rate_1_2:   return "1/2";
        case Rate_1_4:   return "1/4";
        case Rate_1_8:   return "1/8";
        case Rate_1_16:  return "1/16";
        case Rate_1_32:  return "1/32";
        case Rate_1_64:  return "1/64";
        case Rate_1_128: return "1/128";
        default:         return "1/4";
    }
}

juce::String CCStepperProcessor::getTypeName(RateType type)
{
    switch (type)
    {
        case TypeNormal:  return "Normal";
        case TypeTriplet: return "Triplet";
        case TypeDotted:  return "Dotted";
        default:          return "Normal";
    }
}

juce::String CCStepperProcessor::getOrderName(StepOrder order)
{
    switch (order)
    {
        case OrderForward:  return "Forward";
        case OrderReverse:  return "Reverse";
        case OrderPingPong: return "PingPong";
        case OrderRandom:   return "Random";
        case OrderDrunk:    return "Drunk";
        default:            return "Forward";
    }
}

juce::String CCStepperProcessor::getSpeedName(SpeedMode speed)
{
    switch (speed)
    {
        case SpeedHalf:   return "Half";
        case SpeedNormal: return "Normal";
        case SpeedDouble: return "Double";
        default:          return "Normal";
    }
}

double CCStepperProcessor::getRateMultiplier(RateDiv rate)
{
    switch (rate)
    {
        case Rate_1_1:   return 1.0;
        case Rate_1_2:   return 0.5;
        case Rate_1_4:   return 0.25;
        case Rate_1_8:   return 0.125;
        case Rate_1_16:  return 0.0625;
        case Rate_1_32:  return 0.03125;
        case Rate_1_64:  return 0.015625;
        case Rate_1_128: return 0.0078125;
        default:         return 0.25;
    }
}

double CCStepperProcessor::getTypeMultiplier(RateType type)
{
    switch (type)
    {
        case TypeNormal:  return 1.0;
        case TypeTriplet: return 2.0 / 3.0;
        case TypeDotted:  return 1.5;
        default:          return 1.0;
    }
}

double CCStepperProcessor::getSpeedMultiplier(SpeedMode speed)
{
    switch (speed)
    {
        case SpeedHalf:   return 2.0;   // Double step duration = half speed
        case SpeedNormal: return 1.0;
        case SpeedDouble: return 0.5;   // Half step duration = double speed
        default:          return 1.0;
    }
}

int CCStepperProcessor::getNextAllowedStepCount(int current, bool increase)
{
    for (int i = 0; i < NumAllowedStepCounts; i++)
    {
        if (AllowedStepCounts[i] == current)
        {
            if (increase && i + 1 < NumAllowedStepCounts) return AllowedStepCounts[i + 1];
            if (!increase && i - 1 >= 0)                   return AllowedStepCounts[i - 1];
            return current;
        }
    }
    return 16;
}

// =============================================================================
// State save/restore
// =============================================================================

void CCStepperProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::ValueTree vt("CCStepperState");
    vt.setProperty("bpm", bpm.load(), nullptr);
    vt.setProperty("syncBpm", syncToMasterBpm.load(), nullptr);
    vt.setProperty("syncTs", syncToMasterTimeSig.load(), nullptr);
    vt.setProperty("tsNum", tsNumerator.load(), nullptr);
    vt.setProperty("tsDen", tsDenominator.load(), nullptr);

    for (int i = 0; i < MaxSlots; i++)
    {
        const auto& slot = slots[i];
        juce::ValueTree slotVT("Slot");
        slotVT.setProperty("index", i, nullptr);
        slotVT.setProperty("enabled", slot.enabled, nullptr);
        slotVT.setProperty("stepCount", slot.stepCount, nullptr);
        slotVT.setProperty("midiChannel", slot.midiChannel, nullptr);
        slotVT.setProperty("ccNumber", slot.ccNumber, nullptr);
        slotVT.setProperty("rate", (int)slot.rate, nullptr);
        slotVT.setProperty("type", (int)slot.type, nullptr);
        slotVT.setProperty("order", (int)slot.order, nullptr);
        slotVT.setProperty("speed", (int)slot.speed, nullptr);
        slotVT.setProperty("smooth", slot.smooth, nullptr);
        slotVT.setProperty("swing", slot.swing, nullptr);

        juce::String stepStr;
        for (int s = 0; s < slot.stepCount; s++)
        {
            if (s > 0) stepStr += ",";
            stepStr += juce::String(slot.steps[s]);
        }
        slotVT.setProperty("steps", stepStr, nullptr);
        vt.addChild(slotVT, -1, nullptr);
    }

    juce::MemoryOutputStream mos(destData, false);
    vt.writeToStream(mos);
}

void CCStepperProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    auto vt = juce::ValueTree::readFromData(data, (size_t)sizeInBytes);
    if (!vt.isValid() || vt.getType().toString() != "CCStepperState")
        return;

    bpm.store((double)vt.getProperty("bpm", 120.0));
    syncToMasterBpm.store((bool)vt.getProperty("syncBpm", false));
    syncToMasterTimeSig.store((bool)vt.getProperty("syncTs", true));
    tsNumerator.store((int)vt.getProperty("tsNum", 4));
    tsDenominator.store((int)vt.getProperty("tsDen", 4));

    for (int c = 0; c < vt.getNumChildren(); c++)
    {
        auto slotVT = vt.getChild(c);
        if (slotVT.getType().toString() != "Slot") continue;
        int idx = (int)slotVT.getProperty("index", -1);
        if (idx < 0 || idx >= MaxSlots) continue;

        auto& slot = slots[idx];
        slot.enabled = (bool)slotVT.getProperty("enabled", false);
        slot.stepCount = (int)slotVT.getProperty("stepCount", 16);
        slot.midiChannel = (int)slotVT.getProperty("midiChannel", idx + 1);
        slot.ccNumber = (int)slotVT.getProperty("ccNumber", 1);
        slot.rate = (RateDiv)(int)slotVT.getProperty("rate", (int)Rate_1_8);
        slot.type = (RateType)(int)slotVT.getProperty("type", (int)TypeNormal);
        slot.order = (StepOrder)(int)slotVT.getProperty("order", (int)OrderForward);
        slot.speed = (SpeedMode)(int)slotVT.getProperty("speed", (int)SpeedNormal);
        slot.smooth = (bool)slotVT.getProperty("smooth", false);
        slot.swing = (float)(double)slotVT.getProperty("swing", 0.0);

        juce::String stepStr = slotVT.getProperty("steps", "").toString();
        if (stepStr.isNotEmpty())
        {
            juce::StringArray tokens;
            tokens.addTokens(stepStr, ",", "");
            for (int s = 0; s < juce::jmin(tokens.size(), slot.stepCount); s++)
                slot.steps[s] = juce::jlimit(0, 127, tokens[s].getIntValue());
        }
        slot.resetPlayback();
    }
}
