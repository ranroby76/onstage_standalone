#pragma once
// ==============================================================================
//  TunerProcessor.h
//  OnStage — pYIN-inspired pitch detector with sticky-note HMM
//
//  ALGORITHM:
//    1. Multi-threshold YIN (20 thresholds, Beta(2,18) prior).
//    2. Observation suppression: notes with < 40% of peak are zeroed.
//    3. Sticky-note bonus: the currently held note gets an observation
//       boost, requiring overwhelming evidence to switch.
//    4. Online HMM with strong self-transition and heavy jump penalty.
//    5. Sample-and-hold output.
//
//  MONO INPUT (1-in/1-out). Pass-through audio, analysis only.
// ==============================================================================

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <cmath>
#include <algorithm>
#include <array>
#include <vector>

class TunerProcessor
{
public:
    struct Result
    {
        int   midiNote = -1;
        bool  active   = false;
    };

    void prepare (double sr, int /*blockSize*/)
    {
        sampleRate = sr;

        yinWindowSize = (int) std::round (sr * 0.046);
        yinWindowSize = juce::nextPowerOfTwo (yinWindowSize);

        inputRing.resize ((size_t) yinWindowSize * 2, 0.0f);
        diffBuffer.resize ((size_t) yinWindowSize / 2, 0.0f);
        ringWritePos = 0;
        samplesCollected = 0;

        hopSize = juce::jmax (1, (int) std::round (sr * 0.011));

        rmsSmoothed = 0.0f;
        heldNote = -1;
        hmmActive = false;

        for (int i = 0; i < 128; ++i)
            hmmLogProb[i] = 0.0f;

        computeBetaPrior();

        detectedNote.store (-1);
        pitchActive.store (false);
    }

    void reset()
    {
        std::fill (inputRing.begin(), inputRing.end(), 0.0f);
        ringWritePos = 0;
        samplesCollected = 0;
        rmsSmoothed = 0.0f;
        heldNote = -1;
        hmmActive = false;

        for (int i = 0; i < 128; ++i)
            hmmLogProb[i] = 0.0f;

        detectedNote.store (-1);
        pitchActive.store (false);
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        const int numSamples = buffer.getNumSamples();
        if (numSamples == 0 || buffer.getNumChannels() == 0) return;

        const float* src = buffer.getReadPointer (0);

        for (int i = 0; i < numSamples; ++i)
        {
            inputRing[(size_t) ringWritePos] = src[i];
            ringWritePos = (ringWritePos + 1) % (int) inputRing.size();
            ++samplesCollected;

            if (samplesCollected >= hopSize)
            {
                samplesCollected = 0;
                analyseFrame();
            }
        }
    }

    Result getResult() const
    {
        Result r;
        r.midiNote = detectedNote.load (std::memory_order_relaxed);
        r.active   = pitchActive.load (std::memory_order_relaxed);
        return r;
    }

    static juce::String noteNameFromMidi (int midiNote)
    {
        if (midiNote < 0 || midiNote > 127) return "-";
        static const char* names[] = { "C", "C#", "D", "D#", "E", "F",
                                        "F#", "G", "G#", "A", "A#", "B" };
        int octave = (midiNote / 12) - 1;
        return juce::String (names[midiNote % 12]) + juce::String (octave);
    }

    static float freqFromMidi (int midiNote)
    {
        return 440.0f * std::pow (2.0f, ((float) midiNote - 69.0f) / 12.0f);
    }

private:
    static constexpr int   NumThresholds = 20;
    static constexpr float ThresholdStep = 0.01f;
    static constexpr float ThresholdMin  = 0.01f;

    float betaPrior[NumThresholds] {};

    void computeBetaPrior()
    {
        constexpr float a = 2.0f, b = 18.0f;
        float sum = 0.0f;
        for (int i = 0; i < NumThresholds; ++i)
        {
            float x = ThresholdMin + (float) i * ThresholdStep;
            float w = std::pow (x, a - 1.0f) * std::pow (1.0f - x, b - 1.0f);
            betaPrior[i] = w;
            sum += w;
        }
        if (sum > 0.0f)
            for (int i = 0; i < NumThresholds; ++i)
                betaPrior[i] /= sum;
    }

    static int midiNoteFromFreq (float freq)
    {
        if (freq <= 0.0f) return -1;
        return juce::roundToInt (69.0f + 12.0f * std::log2 (freq / 440.0f));
    }

    void analyseFrame()
    {
        const int W = yinWindowSize;
        const int halfW = W / 2;
        const int ringSize = (int) inputRing.size();

        thread_local std::vector<float> frame;
        frame.resize ((size_t) W);
        int readPos = ((ringWritePos - W) % ringSize + ringSize) % ringSize;
        for (int i = 0; i < W; ++i)
            frame[(size_t) i] = inputRing[(size_t) ((readPos + i) % ringSize)];

        // ── RMS silence gate ────────────────────────────────────────────
        float rms = 0.0f;
        for (int i = 0; i < W; ++i)
            rms += frame[(size_t) i] * frame[(size_t) i];
        rms = std::sqrt (rms / (float) W);
        rmsSmoothed += (rms - rmsSmoothed) * 0.15f;

        if (rmsSmoothed < silenceThreshold)
            return;

        // ── Compute CMND ────────────────────────────────────────────────
        diffBuffer.resize ((size_t) halfW);
        diffBuffer[0] = 1.0f;

        float runningSum = 0.0f;
        for (int tau = 1; tau < halfW; ++tau)
        {
            float sum = 0.0f;
            for (int j = 0; j < halfW; ++j)
            {
                float delta = frame[(size_t) j] - frame[(size_t) (j + tau)];
                sum += delta * delta;
            }
            runningSum += sum;
            diffBuffer[(size_t) tau] = (runningSum > 0.0f)
                                         ? sum * (float) tau / runningSum
                                         : 1.0f;
        }

        // ── Multi-threshold candidate extraction ────────────────────────
        float obsProb[128] = {};
        float totalVoicedProb = 0.0f;

        for (int t = 0; t < NumThresholds; ++t)
        {
            float thresh = ThresholdMin + (float) t * ThresholdStep;

            int tauEstimate = -1;
            for (int tau = 2; tau < halfW; ++tau)
            {
                if (diffBuffer[(size_t) tau] < thresh)
                {
                    while (tau + 1 < halfW &&
                           diffBuffer[(size_t) (tau + 1)] < diffBuffer[(size_t) tau])
                        ++tau;
                    tauEstimate = tau;
                    break;
                }
            }

            if (tauEstimate < 1) continue;

            float betterTau = (float) tauEstimate;
            if (tauEstimate > 0 && tauEstimate < halfW - 1)
            {
                float s0 = diffBuffer[(size_t) (tauEstimate - 1)];
                float s1 = diffBuffer[(size_t) tauEstimate];
                float s2 = diffBuffer[(size_t) (tauEstimate + 1)];
                float denom = 2.0f * (2.0f * s1 - s2 - s0);
                if (std::abs (denom) > 1e-10f)
                    betterTau += (s2 - s0) / denom;
            }

            float freq = (float) sampleRate / betterTau;
            if (freq < 50.0f || freq > 2000.0f) continue;

            int note = midiNoteFromFreq (freq);
            if (note < 0 || note > 127) continue;

            obsProb[note] += betaPrior[t];
            totalVoicedProb += betaPrior[t];
        }

        if (totalVoicedProb < 0.10f)
            return;

        // ── Observation suppression: kill weak candidates ───────────────
        // Find peak observation probability
        float peakObs = 0.0f;
        for (int i = 0; i < 128; ++i)
            if (obsProb[i] > peakObs)
                peakObs = obsProb[i];

        // Zero out anything below 40% of peak — eliminates bleed
        if (peakObs > 0.0f)
        {
            float cutoff = peakObs * 0.40f;
            for (int i = 0; i < 128; ++i)
                if (obsProb[i] < cutoff)
                    obsProb[i] = 0.0f;
        }

        // ── Sticky-note bonus: boost the currently held note ────────────
        // Makes it much harder to dislodge — requires strong evidence
        if (heldNote >= 0 && heldNote < 128 && obsProb[heldNote] > 0.0f)
            obsProb[heldNote] *= stickyBoost;

        // ── HMM update ──────────────────────────────────────────────────
        if (! hmmActive)
        {
            for (int s = 0; s < 128; ++s)
            {
                hmmLogProb[s] = (obsProb[s] > 1e-8f)
                              ? std::log (obsProb[s])
                              : -30.0f;
            }
            hmmActive = true;
        }
        else
        {
            constexpr float logSelf  = 0.0f;
            constexpr float logJump  = -10.0f;   // Very heavy jump penalty
            constexpr float logDecay = -0.05f;    // Slow decay

            float globalMax = -1e9f;
            for (int i = 0; i < 128; ++i)
                if (hmmLogProb[i] > globalMax)
                    globalMax = hmmLogProb[i];

            float newLogProb[128];
            for (int s = 0; s < 128; ++s)
            {
                float fromSelf = hmmLogProb[s] + logSelf + logDecay;
                float fromAny  = globalMax + logJump;
                float trans = juce::jmax (fromSelf, fromAny);

                float logObs = (obsProb[s] > 1e-8f)
                             ? std::log (obsProb[s])
                             : -15.0f;

                newLogProb[s] = trans + logObs;
            }

            for (int i = 0; i < 128; ++i)
                hmmLogProb[i] = newLogProb[i];
        }

        // ── Find best state ─────────────────────────────────────────────
        int bestNote = -1;
        float bestProb = -1e9f;
        for (int i = 0; i < 128; ++i)
        {
            if (hmmLogProb[i] > bestProb)
            {
                bestProb = hmmLogProb[i];
                bestNote = i;
            }
        }

        // ── Sample-and-hold ─────────────────────────────────────────────
        if (bestNote >= 0 && bestNote != heldNote)
        {
            heldNote = bestNote;
            detectedNote.store (bestNote, std::memory_order_relaxed);
        }

        if (bestNote >= 0)
            pitchActive.store (true, std::memory_order_relaxed);
    }

    // =========================================================================
    //  Members
    // =========================================================================
    double sampleRate = 44100.0;
    int    yinWindowSize = 2048;
    int    hopSize       = 512;

    std::vector<float> inputRing;
    int ringWritePos     = 0;
    int samplesCollected = 0;

    std::vector<float> diffBuffer;

    float rmsSmoothed = 0.0f;
    static constexpr float silenceThreshold = 0.035f;

    // Sticky-note: multiply held note's observation by this factor.
    // 3.0 means the held note needs to lose by 3:1 ratio to be replaced.
    static constexpr float stickyBoost = 3.0f;

    // HMM
    float hmmLogProb[128] {};
    bool  hmmActive = false;

    // Sample-and-hold
    int heldNote = -1;

    // Atomic outputs
    std::atomic<int>  detectedNote { -1 };
    std::atomic<bool> pitchActive  { false };
};