#include "StereoMeterProcessor.h"

StereoMeterProcessor::StereoMeterProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true))
{
    // FIX: Removed output bus - meter is input-only
}

void StereoMeterProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    // Calculate decay based on sample rate
    // We want ~300ms fall time
    float blocksPerSecond = (float)sampleRate / (float)samplesPerBlock;
    levelDecay = std::pow(0.01f, 1.0f / (0.3f * blocksPerSecond));
    
    // Peak hold: ~1.5 seconds
    peakHoldSamples = (int)(1.5f * blocksPerSecond);
    
    leftPeakHoldCounter = 0;
    rightPeakHoldCounter = 0;
}

void StereoMeterProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midiMessages*/) {
    const int numSamples = buffer.getNumSamples();
    
    // Calculate peak levels for this block
    float leftBlockPeak = 0.0f;
    float rightBlockPeak = 0.0f;
    
    if (buffer.getNumChannels() >= 1) {
        leftBlockPeak = buffer.getMagnitude(0, 0, numSamples);
    }
    if (buffer.getNumChannels() >= 2) {
        rightBlockPeak = buffer.getMagnitude(1, 0, numSamples);
    }
    
    // FIX #3: Reset threshold - if audio drops below this, snap to 0
    // This prevents meters from slowly decaying and appearing stuck
    const float resetThreshold = 0.0001f; // -80dB
    
    // Update levels with decay
    float currentLeft = leftLevel.load();
    float currentRight = rightLevel.load();
    
    if (leftBlockPeak > currentLeft) {
        leftLevel.store(leftBlockPeak);
    } else {
        float newLeft = currentLeft * levelDecay;
        // Snap to 0 if below threshold
        leftLevel.store((newLeft < resetThreshold) ? 0.0f : newLeft);
    }
    
    if (rightBlockPeak > currentRight) {
        rightLevel.store(rightBlockPeak);
    } else {
        float newRight = currentRight * levelDecay;
        // Snap to 0 if below threshold
        rightLevel.store((newRight < resetThreshold) ? 0.0f : newRight);
    }
    
    // Update peak hold
    float currentLeftPeak = leftPeak.load();
    float currentRightPeak = rightPeak.load();
    
    if (leftBlockPeak >= currentLeftPeak) {
        leftPeak.store(leftBlockPeak);
        leftPeakHoldCounter = peakHoldSamples;
    } else if (leftPeakHoldCounter > 0) {
        leftPeakHoldCounter--;
    } else {
        float newLeftPeak = currentLeftPeak * 0.995f;
        // Snap peak to 0 if below threshold
        leftPeak.store((newLeftPeak < resetThreshold) ? 0.0f : newLeftPeak);
    }
    
    if (rightBlockPeak >= currentRightPeak) {
        rightPeak.store(rightBlockPeak);
        rightPeakHoldCounter = peakHoldSamples;
    } else if (rightPeakHoldCounter > 0) {
        rightPeakHoldCounter--;
    } else {
        float newRightPeak = currentRightPeak * 0.995f;
        // Snap peak to 0 if below threshold
        rightPeak.store((newRightPeak < resetThreshold) ? 0.0f : newRightPeak);
    }
    
    // Clipping detection (>= 1.0 or 0dB)
    if (leftBlockPeak >= 0.99f) {
        leftClipping.store(true);
    }
    if (rightBlockPeak >= 0.99f) {
        rightClipping.store(true);
    }
    
    // FIX: Meter is input-only - audio is consumed here (not passed through)
    // Clear the buffer since there's no output
    buffer.clear();
}

bool StereoMeterProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    // FIX: Only check input layout since there's no output
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return true;
}

void StereoMeterProcessor::getStateInformation(juce::MemoryBlock& /*destData*/) {
    // No state to save
}

void StereoMeterProcessor::setStateInformation(const void* /*data*/, int /*sizeInBytes*/) {
    // No state to restore
}
