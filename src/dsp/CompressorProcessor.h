#pragma once
#include <juce_dsp/juce_dsp.h>

// ==============================================================================
//  CompressorProcessor with 5 vocal-oriented compressor types
// ==============================================================================
class CompressorProcessor
{
public:
    // Compressor type enumeration
    enum class Type
    {
        Opto = 0,    // LA-2A style - smooth, slow, program-dependent
        FET,         // 1176 style - fast, aggressive, punchy
        VCA,         // SSL style - clean, precise, transparent
        Vintage,     // Fairchild style - warm, tube saturation, glue
        Peak         // Peak detector - tight transient control
    };

    struct Params
    {
        Type type { Type::VCA };
        float thresholdDb { -18.0f };
        float ratio { 3.0f };
        float attackMs { 8.0f };
        float releaseMs { 120.0f };
        float makeupDb { 0.0f };
        float knee { 0.0f };      // Soft knee in dB (0 = hard knee)
        float mix { 1.0f };       // Dry/wet for parallel compression
        
        bool operator==(const Params& other) const
        {
            return type == other.type &&
                   thresholdDb == other.thresholdDb &&
                   ratio == other.ratio &&
                   attackMs == other.attackMs &&
                   releaseMs == other.releaseMs &&
                   makeupDb == other.makeupDb &&
                   knee == other.knee &&
                   mix == other.mix;
        }
        
        bool operator!=(const Params& other) const { return !(*this == other); }
    };

    CompressorProcessor() = default;

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        numChannels = spec.numChannels;
        
        // Reset envelope followers
        envelope = 0.0f;
        peakEnvelope = 0.0f;
        optoGainReduction = 1.0f;
        
        // Prepare makeup gain
        makeup.reset();
        makeup.prepare(spec);
        
        applyParams();
        isPrepared = true;
        currentInputLevel = 0.0f;
        currentGainReduction = 0.0f;
    }

    void reset()
    {
        envelope = 0.0f;
        peakEnvelope = 0.0f;
        optoGainReduction = 1.0f;
        makeup.reset();
        currentInputLevel = 0.0f;
        currentGainReduction = 0.0f;
    }

    void setParams(const Params& p)
    {
        params = p;
        if (isPrepared)
            applyParams();
    }

    Params getParams() const { return params; }

    template <typename Context>
    void process(Context&& ctx)
    {
        if (bypassed || !isPrepared)
            return;

        auto& block = ctx.getOutputBlock();
        const auto numSamples = block.getNumSamples();
        const auto channels = block.getNumChannels();

        // Store dry signal for parallel compression
        juce::AudioBuffer<float> dryBuffer;
        if (params.mix < 1.0f)
        {
            dryBuffer.setSize((int)channels, (int)numSamples, false, false, true);
            for (size_t ch = 0; ch < channels; ++ch)
            {
                juce::FloatVectorOperations::copy(
                    dryBuffer.getWritePointer((int)ch),
                    block.getChannelPointer(ch),
                    (int)numSamples);
            }
        }

        // Calculate input level for metering
        float sumSquares = 0.0f;
        int totalSamples = 0;
        for (size_t ch = 0; ch < channels; ++ch)
        {
            auto* data = block.getChannelPointer(ch);
            for (size_t i = 0; i < numSamples; ++i)
            {
                sumSquares += data[i] * data[i];
                totalSamples++;
            }
        }
        if (totalSamples > 0)
        {
            float rms = std::sqrt(sumSquares / totalSamples);
            currentInputLevel = juce::Decibels::gainToDecibels(rms + 1e-6f);
        }

        // Process based on compressor type
        switch (params.type)
        {
            case Type::Opto:    processOpto(block);    break;
            case Type::FET:     processFET(block);     break;
            case Type::VCA:     processVCA(block);     break;
            case Type::Vintage: processVintage(block); break;
            case Type::Peak:    processPeak(block);    break;
        }

        // Apply makeup gain
        makeup.process(ctx);

        // Apply dry/wet mix for parallel compression
        if (params.mix < 1.0f)
        {
            float wet = params.mix;
            float dry = 1.0f - wet;
            
            for (size_t ch = 0; ch < channels; ++ch)
            {
                auto* wetData = block.getChannelPointer(ch);
                const auto* dryData = dryBuffer.getReadPointer((int)ch);
                
                for (size_t i = 0; i < numSamples; ++i)
                {
                    wetData[i] = (wetData[i] * wet) + (dryData[i] * dry);
                }
            }
        }
    }

    void setBypassed(bool shouldBypass) { bypassed = shouldBypass; }
    bool isBypassed() const { return bypassed; }
    
    float getCurrentInputLevelDb() const { return currentInputLevel; }
    float getCurrentGainReductionDb() const { return currentGainReduction; }
    
    float getGainReductionDb(float inputDb) const
    {
        if (inputDb <= params.thresholdDb - params.knee / 2.0f)
            return 0.0f;
        
        float overThreshold;
        
        // Soft knee calculation
        if (params.knee > 0.0f && inputDb < params.thresholdDb + params.knee / 2.0f)
        {
            float kneeRange = inputDb - (params.thresholdDb - params.knee / 2.0f);
            float kneeRatio = kneeRange / params.knee;
            overThreshold = kneeRange * kneeRatio * 0.5f;
        }
        else
        {
            overThreshold = inputDb - params.thresholdDb;
        }
        
        return overThreshold * (1.0f - (1.0f / params.ratio));
    }

    static juce::String getTypeName(Type type)
    {
        switch (type)
        {
            case Type::Opto:    return "Opto";
            case Type::FET:     return "FET";
            case Type::VCA:     return "VCA";
            case Type::Vintage: return "Vintage";
            case Type::Peak:    return "Peak";
            default:            return "Unknown";
        }
    }

    static juce::String getTypeDescription(Type type)
    {
        switch (type)
        {
            case Type::Opto:    return "Smooth, musical (LA-2A)";
            case Type::FET:     return "Fast, aggressive (1176)";
            case Type::VCA:     return "Clean, precise (SSL)";
            case Type::Vintage: return "Warm, glue (Fairchild)";
            case Type::Peak:    return "Tight transient control";
            default:            return "";
        }
    }

private:
    void applyParams()
    {
        makeup.setGainDecibels(params.makeupDb);
        
        // Calculate attack/release coefficients
        float attackSec = params.attackMs / 1000.0f;
        float releaseSec = params.releaseMs / 1000.0f;
        
        // Adjust based on compressor type
        switch (params.type)
        {
            case Type::Opto:
                // Opto has program-dependent, slower response
                attackCoeff = std::exp(-1.0f / (sampleRate * attackSec * 3.0f));
                releaseCoeff = std::exp(-1.0f / (sampleRate * releaseSec * 2.0f));
                break;
                
            case Type::FET:
                // FET is very fast
                attackCoeff = std::exp(-1.0f / (sampleRate * attackSec * 0.5f));
                releaseCoeff = std::exp(-1.0f / (sampleRate * releaseSec * 0.8f));
                break;
                
            case Type::VCA:
                // VCA is precise, linear
                attackCoeff = std::exp(-1.0f / (sampleRate * attackSec));
                releaseCoeff = std::exp(-1.0f / (sampleRate * releaseSec));
                break;
                
            case Type::Vintage:
                // Vintage has smooth, slower response
                attackCoeff = std::exp(-1.0f / (sampleRate * attackSec * 2.0f));
                releaseCoeff = std::exp(-1.0f / (sampleRate * releaseSec * 1.5f));
                break;
                
            case Type::Peak:
                // Peak is instant attack
                attackCoeff = std::exp(-1.0f / (sampleRate * attackSec * 0.1f));
                releaseCoeff = std::exp(-1.0f / (sampleRate * releaseSec));
                break;
        }
    }

    // Opto compressor - smooth, program-dependent (LA-2A style)
    template <typename Block>
    void processOpto(Block& block)
    {
        const auto numSamples = block.getNumSamples();
        const auto channels = block.getNumChannels();
        
        float avgGainReduction = 0.0f;
        
        for (size_t i = 0; i < numSamples; ++i)
        {
            // Get max sample across channels
            float inputSample = 0.0f;
            for (size_t ch = 0; ch < channels; ++ch)
                inputSample = std::max(inputSample, std::abs(block.getChannelPointer(ch)[i]));
            
            // Program-dependent RMS-like detection (slow)
            envelope = envelope * 0.9995f + inputSample * 0.0005f;
            
            float inputDb = juce::Decibels::gainToDecibels(envelope + 1e-6f);
            float gainReductionDb = getGainReductionDb(inputDb);
            
            // Opto has very smooth, non-linear response
            float targetGain = juce::Decibels::decibelsToGain(-gainReductionDb);
            
            // Slow follower with program-dependent release
            float releaseSpeed = 0.9998f - (envelope * 0.0003f); // Faster release at higher levels
            optoGainReduction = optoGainReduction * releaseSpeed + targetGain * (1.0f - releaseSpeed);
            
            // Apply gain with soft saturation
            for (size_t ch = 0; ch < channels; ++ch)
            {
                float& sample = block.getChannelPointer(ch)[i];
                sample *= optoGainReduction;
                // Gentle tube-like saturation
                sample = std::tanh(sample * 0.9f) / 0.9f;
            }
            
            avgGainReduction += gainReductionDb;
        }
        
        currentGainReduction = avgGainReduction / numSamples;
    }

    // FET compressor - fast, aggressive (1176 style)
    template <typename Block>
    void processFET(Block& block)
    {
        const auto numSamples = block.getNumSamples();
        const auto channels = block.getNumChannels();
        
        float avgGainReduction = 0.0f;
        
        for (size_t i = 0; i < numSamples; ++i)
        {
            // Peak detection for FET
            float inputSample = 0.0f;
            for (size_t ch = 0; ch < channels; ++ch)
                inputSample = std::max(inputSample, std::abs(block.getChannelPointer(ch)[i]));
            
            // Fast peak follower
            if (inputSample > envelope)
                envelope = inputSample;  // Instant attack
            else
                envelope = envelope * releaseCoeff + inputSample * (1.0f - releaseCoeff);
            
            float inputDb = juce::Decibels::gainToDecibels(envelope + 1e-6f);
            float gainReductionDb = getGainReductionDb(inputDb);
            
            // FET adds harmonic distortion at high gain reduction
            float distortionAmount = gainReductionDb / 40.0f;
            float gain = juce::Decibels::decibelsToGain(-gainReductionDb);
            
            for (size_t ch = 0; ch < channels; ++ch)
            {
                float& sample = block.getChannelPointer(ch)[i];
                sample *= gain;
                // FET-style odd harmonic distortion
                if (distortionAmount > 0.01f)
                {
                    float saturated = std::tanh(sample * (1.0f + distortionAmount * 2.0f));
                    sample = sample * (1.0f - distortionAmount) + saturated * distortionAmount;
                }
            }
            
            avgGainReduction += gainReductionDb;
        }
        
        currentGainReduction = avgGainReduction / numSamples;
    }

    // VCA compressor - clean, precise (SSL style)
    template <typename Block>
    void processVCA(Block& block)
    {
        const auto numSamples = block.getNumSamples();
        const auto channels = block.getNumChannels();
        
        float avgGainReduction = 0.0f;
        
        for (size_t i = 0; i < numSamples; ++i)
        {
            // RMS detection for VCA (clean, predictable)
            float sumSquares = 0.0f;
            for (size_t ch = 0; ch < channels; ++ch)
            {
                float s = block.getChannelPointer(ch)[i];
                sumSquares += s * s;
            }
            float rms = std::sqrt(sumSquares / channels);
            
            // Smooth envelope follower
            if (rms > envelope)
                envelope = envelope * attackCoeff + rms * (1.0f - attackCoeff);
            else
                envelope = envelope * releaseCoeff + rms * (1.0f - releaseCoeff);
            
            float inputDb = juce::Decibels::gainToDecibels(envelope + 1e-6f);
            float gainReductionDb = getGainReductionDb(inputDb);
            float gain = juce::Decibels::decibelsToGain(-gainReductionDb);
            
            // VCA is completely transparent - no coloration
            for (size_t ch = 0; ch < channels; ++ch)
            {
                block.getChannelPointer(ch)[i] *= gain;
            }
            
            avgGainReduction += gainReductionDb;
        }
        
        currentGainReduction = avgGainReduction / numSamples;
    }

    // Vintage compressor - warm, tube saturation (Fairchild style)
    template <typename Block>
    void processVintage(Block& block)
    {
        const auto numSamples = block.getNumSamples();
        const auto channels = block.getNumChannels();
        
        float avgGainReduction = 0.0f;
        
        for (size_t i = 0; i < numSamples; ++i)
        {
            // Variable-mu style detection (RMS with transformer coloration)
            float sumSquares = 0.0f;
            for (size_t ch = 0; ch < channels; ++ch)
            {
                float s = block.getChannelPointer(ch)[i];
                sumSquares += s * s;
            }
            float rms = std::sqrt(sumSquares / channels);
            
            // Very smooth envelope (large capacitors)
            if (rms > envelope)
                envelope = envelope * attackCoeff + rms * (1.0f - attackCoeff);
            else
                envelope = envelope * releaseCoeff + rms * (1.0f - releaseCoeff);
            
            float inputDb = juce::Decibels::gainToDecibels(envelope + 1e-6f);
            float gainReductionDb = getGainReductionDb(inputDb);
            
            // Variable-mu has very soft knee naturally
            float softGainReduction = gainReductionDb * 0.85f; // Natural soft response
            float gain = juce::Decibels::decibelsToGain(-softGainReduction);
            
            for (size_t ch = 0; ch < channels; ++ch)
            {
                float& sample = block.getChannelPointer(ch)[i];
                sample *= gain;
                
                // Tube/transformer saturation (even harmonics)
                float x = sample;
                sample = x + 0.1f * x * x - 0.05f * x * x * x;
                sample = std::tanh(sample * 0.95f) / 0.95f;
            }
            
            avgGainReduction += softGainReduction;
        }
        
        currentGainReduction = avgGainReduction / numSamples;
    }

    // Peak compressor - tight transient control
    template <typename Block>
    void processPeak(Block& block)
    {
        const auto numSamples = block.getNumSamples();
        const auto channels = block.getNumChannels();
        
        float avgGainReduction = 0.0f;
        
        for (size_t i = 0; i < numSamples; ++i)
        {
            // True peak detection
            float peak = 0.0f;
            for (size_t ch = 0; ch < channels; ++ch)
                peak = std::max(peak, std::abs(block.getChannelPointer(ch)[i]));
            
            // Instant attack, smooth release
            if (peak > peakEnvelope)
                peakEnvelope = peak;  // Instant
            else
                peakEnvelope = peakEnvelope * releaseCoeff + peak * (1.0f - releaseCoeff);
            
            float inputDb = juce::Decibels::gainToDecibels(peakEnvelope + 1e-6f);
            float gainReductionDb = getGainReductionDb(inputDb);
            float gain = juce::Decibels::decibelsToGain(-gainReductionDb);
            
            // Peak mode is transparent but very responsive
            for (size_t ch = 0; ch < channels; ++ch)
            {
                block.getChannelPointer(ch)[i] *= gain;
            }
            
            avgGainReduction += gainReductionDb;
        }
        
        currentGainReduction = avgGainReduction / numSamples;
    }

    Params params;
    bool bypassed = false;
    bool isPrepared = false;
    
    double sampleRate = 44100.0;
    int numChannels = 2;
    
    // Envelope followers
    float envelope = 0.0f;
    float peakEnvelope = 0.0f;
    float optoGainReduction = 1.0f;
    
    // Coefficients
    float attackCoeff = 0.0f;
    float releaseCoeff = 0.0f;
    
    // Makeup gain
    juce::dsp::Gain<float> makeup;
    
    // Metering
    std::atomic<float> currentInputLevel { 0.0f };
    std::atomic<float> currentGainReduction { 0.0f };
};
