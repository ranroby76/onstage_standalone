#include "StereoMeterProcessor.h"

StereoMeterProcessor::StereoMeterProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
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
    
    // Update levels with decay
    float currentLeft = leftLevel.load();
    float currentRight = rightLevel.load();
    
    if (leftBlockPeak > currentLeft) {
        leftLevel.store(leftBlockPeak);
    } else {
        leftLevel.store(currentLeft * levelDecay);
    }
    
    if (rightBlockPeak > currentRight) {
        rightLevel.store(rightBlockPeak);
    } else {
        rightLevel.store(currentRight * levelDecay);
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
        leftPeak.store(currentLeftPeak * 0.995f);
    }
    
    if (rightBlockPeak >= currentRightPeak) {
        rightPeak.store(rightBlockPeak);
        rightPeakHoldCounter = peakHoldSamples;
    } else if (rightPeakHoldCounter > 0) {
        rightPeakHoldCounter--;
    } else {
        rightPeak.store(currentRightPeak * 0.995f);
    }
    
    // Clipping detection (>= 1.0 or 0dB)
    if (leftBlockPeak >= 0.99f) {
        leftClipping.store(true);
    }
    if (rightBlockPeak >= 0.99f) {
        rightClipping.store(true);
    }
    
    // Pass through audio unchanged
}

bool StereoMeterProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return true;
}

void StereoMeterProcessor::getStateInformation(juce::MemoryBlock& /*destData*/) {
    // No state to save
}

void StereoMeterProcessor::setStateInformation(const void* /*data*/, int /*sizeInBytes*/) {
    // No state to restore
}
