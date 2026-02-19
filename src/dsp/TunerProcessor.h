#pragma once
// ==============================================================================
//  TunerProcessor.h
//  OnStage — Chromatic tuner using FFT + Harmonic Product Spectrum (HPS)
//
//  REWRITTEN: Replaced failing pYIN+HMM with proven FFT+HPS approach.
//  Based on algorithm from TomSchimansky/GuitarTuner (GPL-2.0).
//
//  ALGORITHM:
//    1. Ring buffer collects ~93ms of mono audio (4096 samples @ 44.1k)
//    2. Hanning window applied to reduce spectral leakage
//    3. Zero-padded FFT for high frequency resolution (~2.7 Hz/bin @ 44.1k)
//    4. Mains hum suppression (0–62 Hz zeroed)
//    5. White noise floor suppression (per-band average energy gating)
//    6. HPS with 5 harmonics — multiplies downsampled spectra to find
//       fundamental frequency, eliminating octave/harmonic errors
//    7. Octave-error correction: if a sub-octave peak is strong, prefer it
//    8. Majority vote filter: note must win 3 consecutive frames to register
//    9. Cents deviation output for UI needle display
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
#include <complex>

class TunerProcessor
{
public:
    // =========================================================================
    //  Result struct — read by UI
    // =========================================================================
    struct Result
    {
        int   midiNote   = -1;      // MIDI note number (0–127), -1 = no note
        float centsOff   = 0.0f;    // Deviation from nearest note in cents (-50 to +50)
        float frequency  = 0.0f;    // Detected fundamental frequency in Hz
        bool  active     = false;   // true = valid pitch detected
    };

    // =========================================================================
    //  Lifecycle
    // =========================================================================
    void prepare (double sr, int /*blockSize*/)
    {
        sampleRate = sr;

        // Analysis window: ~93ms (4096 @ 44.1k), next power of 2
        analysisSize = (int) std::round (sr * 0.093);
        analysisSize = juce::nextPowerOfTwo (analysisSize);
        analysisSize = juce::jmax (analysisSize, 2048);

        // Zero-padded FFT: 4x for ~2.7 Hz/bin resolution at 44.1k
        fftOrder = (int) std::round (std::log2 ((double) analysisSize * 4));
        fftSize  = 1 << fftOrder;
        fft = std::make_unique<juce::dsp::FFT> (fftOrder);

        // Allocate buffers
        inputRing.resize ((size_t) analysisSize, 0.0f);
        hanningWindow.resize ((size_t) analysisSize);
        fftData.resize ((size_t) fftSize * 2, 0.0f);   // interleaved real/imag
        magnitudeSpectrum.resize ((size_t) (fftSize / 2 + 1), 0.0f);
        hpsSpectrum.resize ((size_t) (fftSize / 2 + 1), 0.0f);

        ringWritePos = 0;
        samplesCollected = 0;

        // Hop: analyse every ~11ms (512 samples @ 44.1k) for fast response
        hopSize = juce::jmax (1, analysisSize / 8);

        // Precompute Hanning window
        for (int i = 0; i < analysisSize; ++i)
            hanningWindow[(size_t) i] = 0.5f * (1.0f - std::cos (2.0f * juce::MathConstants<float>::pi
                                                                   * (float) i / (float) analysisSize));

        // Reset state
        rmsSmoothed = 0.0f;
        voteCount = 0;
        lastVotedNote = -1;

        detectedNote.store (-1);
        detectedCents.store (0.0f);
        detectedFreq.store (0.0f);
        pitchActive.store (false);

        // Compute mains hum cutoff bin (62 Hz — covers both 50 Hz and 60 Hz + harmonics)
        mainsHumBinCutoff = (int) std::ceil (62.0 * (double) fftSize / sampleRate);
    }

    void reset()
    {
        std::fill (inputRing.begin(), inputRing.end(), 0.0f);
        ringWritePos = 0;
        samplesCollected = 0;
        rmsSmoothed = 0.0f;
        voteCount = 0;
        lastVotedNote = -1;

        detectedNote.store (-1);
        detectedCents.store (0.0f);
        detectedFreq.store (0.0f);
        pitchActive.store (false);
    }

    // =========================================================================
    //  Process — called from audio thread
    // =========================================================================
    void process (juce::AudioBuffer<float>& buffer)
    {
        const int numSamples = buffer.getNumSamples();
        if (numSamples == 0 || buffer.getNumChannels() == 0) return;

        const float* src = buffer.getReadPointer (0);

        for (int i = 0; i < numSamples; ++i)
        {
            inputRing[(size_t) ringWritePos] = src[i];
            ringWritePos = (ringWritePos + 1) % analysisSize;
            ++samplesCollected;

            if (samplesCollected >= hopSize)
            {
                samplesCollected = 0;
                analyseFrame();
            }
        }
    }

    // =========================================================================
    //  Read results (thread-safe, called from UI)
    // =========================================================================
    Result getResult() const
    {
        Result r;
        r.midiNote  = detectedNote.load (std::memory_order_relaxed);
        r.centsOff  = detectedCents.load (std::memory_order_relaxed);
        r.frequency = detectedFreq.load (std::memory_order_relaxed);
        r.active    = pitchActive.load (std::memory_order_relaxed);
        return r;
    }

    // =========================================================================
    //  Helpers
    // =========================================================================
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
    // =========================================================================
    //  Core analysis — called every hop
    // =========================================================================
    void analyseFrame()
    {
        const int halfSpectrum = fftSize / 2 + 1;

        // ── Copy ring buffer into linear frame with Hanning window ──────────
        int readPos = ((ringWritePos - analysisSize) % analysisSize + analysisSize) % analysisSize;

        std::fill (fftData.begin(), fftData.end(), 0.0f);
        for (int i = 0; i < analysisSize; ++i)
        {
            int idx = (readPos + i) % analysisSize;
            fftData[(size_t) i] = inputRing[(size_t) idx] * hanningWindow[(size_t) i];
        }
        // Remaining samples are zero (zero-padding for interpolation)

        // ── RMS silence gate ────────────────────────────────────────────────
        float rms = 0.0f;
        for (int i = 0; i < analysisSize; ++i)
            rms += fftData[(size_t) i] * fftData[(size_t) i];
        rms = std::sqrt (rms / (float) analysisSize);
        rmsSmoothed += (rms - rmsSmoothed) * 0.15f;

        if (rmsSmoothed < silenceThreshold)
        {
            // Silence — decay the vote counter
            if (voteCount > 0) --voteCount;
            if (voteCount == 0)
            {
                pitchActive.store (false, std::memory_order_relaxed);
                detectedNote.store (-1, std::memory_order_relaxed);
            }
            return;
        }

        // ── FFT ─────────────────────────────────────────────────────────────
        fft->performRealOnlyForwardTransform (fftData.data(), true);

        // Extract magnitude spectrum
        for (int i = 0; i < halfSpectrum; ++i)
        {
            float re = fftData[(size_t) (i * 2)];
            float im = fftData[(size_t) (i * 2 + 1)];
            magnitudeSpectrum[(size_t) i] = std::sqrt (re * re + im * im);
        }

        // ── Mains hum suppression (0–62 Hz) ────────────────────────────────
        for (int i = 0; i < juce::jmin (mainsHumBinCutoff, halfSpectrum); ++i)
            magnitudeSpectrum[(size_t) i] = 0.0f;

        // ── White noise floor suppression ───────────────────────────────────
        // Divide spectrum into bands, compute average energy per band,
        // zero out bins below their band's average (removes flat noise floor)
        suppressNoiseFloor (halfSpectrum);

        // ── Harmonic Product Spectrum (HPS) ─────────────────────────────────
        // Multiply the magnitude spectrum with itself downsampled by 2, 3, 4, 5
        // Peaks at fundamental frequency are reinforced; harmonics are suppressed
        const int hpsLength = halfSpectrum / numHarmonics;

        for (int i = 0; i < hpsLength; ++i)
            hpsSpectrum[(size_t) i] = magnitudeSpectrum[(size_t) i];

        for (int h = 2; h <= numHarmonics; ++h)
        {
            for (int i = 0; i < hpsLength; ++i)
            {
                int srcBin = i * h;
                if (srcBin < halfSpectrum)
                    hpsSpectrum[(size_t) i] *= magnitudeSpectrum[(size_t) srcBin];
                else
                    hpsSpectrum[(size_t) i] = 0.0f;
            }
        }

        // ── Find peak in HPS spectrum ───────────────────────────────────────
        // Only search above minimum frequency (65 Hz ≈ C2)
        int minBin = (int) std::ceil (65.0 * (double) fftSize / sampleRate);
        int maxBin = (int) std::floor (2000.0 * (double) fftSize / sampleRate);
        maxBin = juce::jmin (maxBin, hpsLength - 1);

        int    peakBin  = -1;
        float  peakVal  = 0.0f;

        for (int i = minBin; i <= maxBin; ++i)
        {
            if (hpsSpectrum[(size_t) i] > peakVal)
            {
                peakVal = hpsSpectrum[(size_t) i];
                peakBin = i;
            }
        }

        if (peakBin < 1 || peakVal < 1e-10f)
        {
            if (voteCount > 0) --voteCount;
            if (voteCount == 0) pitchActive.store (false, std::memory_order_relaxed);
            return;
        }

        // ── Parabolic interpolation for sub-bin accuracy ────────────────────
        float alpha = hpsSpectrum[(size_t) (peakBin - 1)];
        float beta  = hpsSpectrum[(size_t) peakBin];
        float gamma = hpsSpectrum[(size_t) (peakBin + 1)];

        float denom = alpha - 2.0f * beta + gamma;
        float interpOffset = 0.0f;
        if (std::abs (denom) > 1e-10f)
            interpOffset = 0.5f * (alpha - gamma) / denom;

        float exactBin = (float) peakBin + interpOffset;
        float freq = (float) (exactBin * sampleRate / (double) fftSize);

        if (freq < 50.0f || freq > 2000.0f)
            return;

        // ── Octave-error correction ─────────────────────────────────────────
        // If there's a strong peak at half the frequency, the true fundamental
        // is likely the sub-octave. Check ratio against threshold.
        freq = correctOctaveError (freq, halfSpectrum);

        // ── Convert to MIDI note + cents ────────────────────────────────────
        float noteFloat = 69.0f + 12.0f * std::log2 (freq / 440.0f);
        int   midiNote  = juce::roundToInt (noteFloat);
        float cents     = (noteFloat - (float) midiNote) * 100.0f;

        if (midiNote < 0 || midiNote > 127)
            return;

        // ── Majority vote filter (3 consecutive agreements) ─────────────────
        if (midiNote == lastVotedNote)
        {
            voteCount = juce::jmin (voteCount + 1, voteThreshold + 2);
        }
        else
        {
            lastVotedNote = midiNote;
            voteCount = 1;
        }

        if (voteCount >= voteThreshold)
        {
            detectedNote.store (midiNote, std::memory_order_relaxed);
            detectedCents.store (cents, std::memory_order_relaxed);
            detectedFreq.store (freq, std::memory_order_relaxed);
            pitchActive.store (true, std::memory_order_relaxed);
        }
    }

    // =========================================================================
    //  White noise floor suppression
    //  Divides spectrum into bands, zeros bins below per-band average energy
    // =========================================================================
    void suppressNoiseFloor (int spectrumLength)
    {
        constexpr int numBands = 16;
        int bandSize = juce::jmax (1, spectrumLength / numBands);

        for (int band = 0; band < numBands; ++band)
        {
            int start = band * bandSize;
            int end   = juce::jmin (start + bandSize, spectrumLength);

            // Compute average energy in this band
            float avg = 0.0f;
            for (int i = start; i < end; ++i)
                avg += magnitudeSpectrum[(size_t) i];
            avg /= (float) (end - start);

            // Suppress bins below average (removes noise floor)
            float threshold = avg * noiseFloorMultiplier;
            for (int i = start; i < end; ++i)
            {
                if (magnitudeSpectrum[(size_t) i] < threshold)
                    magnitudeSpectrum[(size_t) i] = 0.0f;
            }
        }
    }

    // =========================================================================
    //  Octave-error correction
    //  If a sub-octave peak exists with significant amplitude, prefer it
    // =========================================================================
    float correctOctaveError (float detectedFreq, int spectrumLength)
    {
        float subOctaveFreq = detectedFreq * 0.5f;
        if (subOctaveFreq < 50.0f) return detectedFreq;

        int detectedBin  = juce::roundToInt ((float) ((double) detectedFreq * (double) fftSize / sampleRate));
        int subOctaveBin = juce::roundToInt ((float) ((double) subOctaveFreq * (double) fftSize / sampleRate));

        if (detectedBin < 0 || detectedBin >= spectrumLength) return detectedFreq;
        if (subOctaveBin < 1 || subOctaveBin >= spectrumLength) return detectedFreq;

        // Look in a small window around the sub-octave bin for a peak
        float subPeak = 0.0f;
        int   searchRadius = 3;
        for (int i = juce::jmax (1, subOctaveBin - searchRadius);
             i <= juce::jmin (spectrumLength - 1, subOctaveBin + searchRadius); ++i)
        {
            subPeak = juce::jmax (subPeak, magnitudeSpectrum[(size_t) i]);
        }

        float detectedPeak = magnitudeSpectrum[(size_t) detectedBin];

        // If sub-octave peak is at least 20% of the detected peak, it's likely
        // the true fundamental (HPS sometimes misses it when one harmonic is weak)
        if (detectedPeak > 0.0f && subPeak / detectedPeak > octaveCorrectionThreshold)
            return subOctaveFreq;

        return detectedFreq;
    }

    // =========================================================================
    //  Members
    // =========================================================================
    double sampleRate    = 44100.0;
    int    analysisSize  = 4096;
    int    fftOrder      = 14;       // 2^14 = 16384
    int    fftSize       = 16384;
    int    hopSize       = 1024;

    std::unique_ptr<juce::dsp::FFT> fft;

    std::vector<float> inputRing;
    std::vector<float> hanningWindow;
    std::vector<float> fftData;
    std::vector<float> magnitudeSpectrum;
    std::vector<float> hpsSpectrum;

    int ringWritePos     = 0;
    int samplesCollected = 0;

    float rmsSmoothed = 0.0f;

    // ── Tuning parameters ───────────────────────────────────────────────────
    static constexpr float silenceThreshold = 0.015f;       // RMS gate (lower than before — old was 0.035)
    static constexpr int   numHarmonics     = 5;            // HPS harmonic count (5 = standard)
    static constexpr float noiseFloorMultiplier = 1.0f;     // Suppress bins below band average × this
    static constexpr float octaveCorrectionThreshold = 0.2f; // Sub-octave ratio to trigger correction
    static constexpr int   voteThreshold    = 2;            // Consecutive agreements needed (was 3)

    int mainsHumBinCutoff = 0;

    // Majority vote state
    int lastVotedNote = -1;
    int voteCount     = 0;

    // ── Atomic outputs ──────────────────────────────────────────────────────
    std::atomic<int>   detectedNote  { -1 };
    std::atomic<float> detectedCents { 0.0f };
    std::atomic<float> detectedFreq  { 0.0f };
    std::atomic<bool>  pitchActive   { false };
};
