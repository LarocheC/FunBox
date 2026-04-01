#pragma once

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include "shy_fft.h"
#include "fourier.h"
#include "wave.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace funbox_dsp
{

// Simple sine/triangle oscillator (replaces DaisySP Oscillator)
class SimpleOscillator
{
public:
    void init(float sampleRate)
    {
        sampleRate_ = sampleRate;
        phase_ = 0.0f;
        freq_ = 0.01f;
        waveform_ = 0; // 0=sin, 1=tri
    }

    void setFreq(float freq) { freq_ = freq; }
    void setWaveform(int wf) { waveform_ = wf; }

    float process()
    {
        float out;
        if (waveform_ == 0) // sine
            out = std::sin(2.0f * (float)M_PI * phase_);
        else // triangle
            out = 4.0f * std::abs(phase_ - 0.5f) - 1.0f;

        phase_ += freq_ / sampleRate_;
        phase_ -= std::floor(phase_);
        return out;
    }

private:
    float sampleRate_ = 44100.0f;
    float phase_ = 0.0f;
    float freq_ = 0.01f;
    int waveform_ = 0;
};

// Simple one-pole lowpass filter (replaces DaisySP Tone)
class SimpleTone
{
public:
    void init(float sampleRate)
    {
        sampleRate_ = sampleRate;
        setFreq(8000.0f);
        y_ = 0.0f;
    }

    void setFreq(float freq)
    {
        float w = 2.0f * (float)M_PI * freq / sampleRate_;
        coeff_ = w / (1.0f + w);
    }

    float process(float in)
    {
        y_ += coeff_ * (in - y_);
        return y_;
    }

private:
    float sampleRate_ = 44100.0f;
    float coeff_ = 0.5f;
    float y_ = 0.0f;
};

// Simple sample rate reducer (replaces DaisySP SampleRateReducer)
class SimpleSampleRateReducer
{
public:
    void init()
    {
        phase_ = 0.0f;
        held_ = 0.0f;
        freq_ = 1.0f;
    }

    void setFreq(float freq) { freq_ = std::max(0.01f, std::min(1.0f, freq)); }

    float process(float in)
    {
        phase_ += freq_;
        if (phase_ >= 1.0f)
        {
            phase_ -= 1.0f;
            held_ = in;
        }
        return held_;
    }

private:
    float phase_ = 0.0f;
    float held_ = 0.0f;
    float freq_ = 1.0f;
};

// Main spectral reverb engine ported from Venus
class SpectralReverb
{
public:
    static constexpr size_t ORDER = 12;
    static constexpr size_t N = (1 << ORDER);     // 4096
    static constexpr size_t LAPS = 4;
    static constexpr size_t BUFFSIZE = 2 * LAPS * N;

    SpectralReverb() = default;
    ~SpectralReverb()
    {
        delete stft_;
        delete fft_;
    }

    void init(float sampleRate)
    {
        sampleRate_ = sampleRate;

        // Clear buffers
        std::memset(inBuf_, 0, sizeof(inBuf_));
        std::memset(middleBuf_, 0, sizeof(middleBuf_));
        std::memset(outBuf_, 0, sizeof(outBuf_));
        std::memset(reverbEnergy_, 0, sizeof(reverbEnergy_));

        // Initialize FFT and STFT
        delete fft_;
        delete stft_;
        fft_ = new ShyFFT<float, N, RotationPhasor>();
        fft_->Init();

        // The reverb callback is a static function that references this instance
        currentInstance_ = this;
        stft_ = new soundmath::Fourier<float, N>(
            &SpectralReverb::reverbCallback, fft_, &hann_, LAPS, inBuf_, middleBuf_, outBuf_);

        // Initialize lo-fi processors
        sampleRateReducer_.init();
        sampleRateReducer_.setFreq(0.3f);
        lowpass_.init(sampleRate);
        lowpass_.setFreq(8000.0f);

        // Initialize drift oscillators
        for (int i = 0; i < 4; i++)
            driftOsc_[i].init(sampleRate);

        intervalSamples_ = std::ceil((float)BUFFSIZE / LAPS);
    }

    // Parameters (call before processing)
    void setDecay(float v) { decay_ = v * 99.0f + 1.0f; }        // 0-1 -> 1-100
    void setMix(float v) { mix_ = v; }                            // 0-1
    void setDamp(float v) { damp_ = v; }                          // 0-1 (exponential mapping done by host)
    void setShimmer(float v) { shimmer_ = v * 0.1f; }             // 0-1 -> 0-0.1
    void setShimmerTone(float v) { shimmerTone_ = v * 0.3f; }     // 0-1 -> 0-0.3
    void setDetune(float v) { detune_ = v * 0.3f - 0.15f; }      // 0-1 -> -0.15 to 0.15
    void setFreeze(bool v) { freeze_ = v; }

    void setShimmerMode(int mode) { shimmerMode_ = mode; }  // 0=down, 1=up, 2=both
    void setLofiMode(int mode)
    {
        lofiMode_ = mode;
        if (mode == 0) { // less lofi
            sampleRateReducer_.setFreq(0.3f);
            lowpass_.setFreq(8000.0f);
        } else if (mode == 2) { // more lofi
            sampleRateReducer_.setFreq(0.2f);
        }
    }
    void setDriftMode(int mode)
    {
        driftMode_ = mode;
        if (mode == 0) { // slow drift
            driftOsc_[0].setFreq(0.009f); driftOsc_[0].setWaveform(0);
            driftOsc_[1].setFreq(0.01f);  driftOsc_[1].setWaveform(0);
            driftOsc_[2].setFreq(0.011f); driftOsc_[2].setWaveform(0);
            driftOsc_[3].setFreq(0.012f); driftOsc_[3].setWaveform(0);
        } else if (mode == 2) { // fast drift
            driftOsc_[0].setFreq(0.020f); driftOsc_[0].setWaveform(1);
            driftOsc_[1].setFreq(0.025f); driftOsc_[1].setWaveform(1);
            driftOsc_[2].setFreq(0.030f); driftOsc_[2].setWaveform(1);
            driftOsc_[3].setFreq(0.035f); driftOsc_[3].setWaveform(1);
        }
    }

    // Process a single sample (mono in, mono out)
    float processSample(float input)
    {
        // Update drift oscillators
        for (int i = 0; i < 4; i++)
            driftMult_[i] = driftOsc_[i].process();

        // Compute derived parameters (matches ProcessControls logic)
        updateDerivedParams();

        // Feed STFT
        stft_->write(input);

        // Read processed output with optional lo-fi
        float wet = 0.0f;
        if (lofiMode_ == 0) // less lofi
            wet = lowpass_.process(sampleRateReducer_.process(stft_->read()));
        else if (lofiMode_ == 1) // normal
            wet = stft_->read();
        else // more lofi
            wet = sampleRateReducer_.process(stft_->read());

        return wet * mix_ + input * (1.0f - mix_);
    }

private:
    void updateDerivedParams()
    {
        float vdamp = damp_;
        float vshimmer = shimmer_;
        float vshimmerTone = shimmerTone_;
        float vdetune = std::abs(detune_);

        // Drift modulation
        if (driftMode_ == 0 || driftMode_ == 2) {
            vdamp = vdamp * std::abs(driftMult_[0]) * 0.7f + 0.3f;
            vshimmer *= std::abs(driftMult_[1]);
            vshimmerTone *= std::abs(driftMult_[2]);
            vdetune *= std::abs(driftMult_[3]);
        }

        int detuneMode = 1;
        int detuneMul = 1;
        if (vdetune > 0.03f) {
            vdetune -= 0.029f;
            if (detune_ >= 0)
                { detuneMode = 2; detuneMul = 1; }
            else
                { detuneMode = 0; detuneMul = -1; }
        } else {
            detuneMode = 1;
        }

        float octUpPerSec = std::pow(8.0f, vshimmer) - 1.0f;
        float octUpPerInterval = std::min(0.75f, octUpPerSec / sampleRate_ * intervalSamples_);

        float octUpPerSec2 = std::pow(8.0f, vshimmerTone) - 1.0f;
        float octUpPerInterval2 = std::min(0.75f, octUpPerSec2 / sampleRate_ * intervalSamples_);

        // Store for use in reverb callback
        cachedDamp_ = vdamp;
        cachedShimmerDouble_ = octUpPerInterval * (1.0f - vshimmerTone / 1.58f);
        cachedShimmerTriple_ = (octUpPerInterval2 / 1.58f) * vshimmerTone;
        cachedShimmerRemainder_ = 1.0f - cachedShimmerDouble_ - cachedShimmerTriple_;

        float detDoubleRate = std::pow(8.0f, vdetune) - 1.0f;
        float detPerInterval = std::min(0.75f, detDoubleRate / sampleRate_ * intervalSamples_);
        cachedDetuneDouble_ = detPerInterval;
        cachedDetuneRemainder_ = 1.0f - cachedDetuneDouble_;
        cachedDetuneMode_ = detuneMode;
        cachedDetuneMul_ = detuneMul;
    }

    // Static callback bridge for Fourier class
    static void reverbCallback(const float* in, float* out)
    {
        currentInstance_->reverbProcess(in, out);
    }

    // The core spectral reverb algorithm (ported directly from venus.cpp)
    void reverbProcess(const float* in, float* out)
    {
        static constexpr size_t offset = N / 2;
        float fftSize = N / 2.0f;

        for (size_t i = 0; i < N / 2; i++)
        {
            float fftBin = (float)(i + 1);

            float real = in[i];
            float imag = in[i + offset];
            float energy = real * real + imag * imag;

            // Amplitude from stored reverb energy
            float reverbAmp = std::sqrt(reverbEnergy_[i]);
            if (fftBin / fftSize > cachedDamp_)
                reverbAmp *= cachedDamp_ * fftSize / fftBin;

            // Random phase
            float randomPhase = ((float)std::rand() / RAND_MAX) * 2.0f * (float)M_PI;
            real = reverbAmp * std::cos(randomPhase);
            imag = reverbAmp * std::sin(randomPhase);

            if (!freeze_)
            {
                reverbEnergy_[i] += energy / LAPS;

                float decayFactor = 1.0f / decay_;
                reverbEnergy_[i] *= 1.0f - decayFactor;

                float halfFftSize = fftSize / 2.0f;
                float current = reverbEnergy_[i];

                if (i > 0 && i < (size_t)halfFftSize - 2)
                {
                    // Shimmer: octave up
                    if (shimmerMode_ == 1 || shimmerMode_ == 2) {
                        reverbEnergy_[2*i - 1] += 0.123f * cachedShimmerDouble_ * current;
                        reverbEnergy_[2*i]     += 0.25f  * cachedShimmerDouble_ * current;
                        reverbEnergy_[2*i + 1] += 0.123f * cachedShimmerDouble_ * current;
                    }
                    // Shimmer: octave down
                    if ((shimmerMode_ == 0 || shimmerMode_ == 2) && i > 1 && !(i % 2)) {
                        reverbEnergy_[i/2 - 1] += 0.75f * cachedShimmerDouble_ * current;
                        reverbEnergy_[i/2]     += 1.5f  * cachedShimmerDouble_ * current;
                        reverbEnergy_[i/2 + 1] += 0.75f * cachedShimmerDouble_ * current;
                    }

                    // Shimmer: octave + fifth (3x frequency)
                    if (3*i + 1 < (size_t)halfFftSize) {
                        reverbEnergy_[3*i - 2] += 0.055f * cachedShimmerTriple_ * current;
                        reverbEnergy_[3*i - 1] += 0.11f  * cachedShimmerTriple_ * current;
                        reverbEnergy_[3*i]     += 0.17f  * cachedShimmerTriple_ * current;
                        reverbEnergy_[3*i + 1] += 0.11f  * cachedShimmerTriple_ * current;
                        reverbEnergy_[3*i + 2] += 0.105f * cachedShimmerTriple_ * current;
                    }

                    // Detune
                    if (i > 2 && i < (size_t)halfFftSize - 2 && cachedDetuneMode_ != 1) {
                        reverbEnergy_[i + (3 * cachedDetuneMul_)] += 0.123f * cachedDetuneDouble_ * current;
                        reverbEnergy_[i + (2 * cachedDetuneMul_)] += 0.25f  * cachedDetuneDouble_ * current;
                        reverbEnergy_[i + (1 * cachedDetuneMul_)] += 0.123f * cachedDetuneDouble_ * current;
                    }
                }

                float detRem = (cachedDetuneMode_ == 1) ? 1.0f : cachedDetuneRemainder_;
                reverbEnergy_[i] = detRem * cachedShimmerRemainder_ * current;
            }

            out[i] = real;
            out[i + offset] = imag;
        }
    }

    // Instance pointer for static callback
    static inline SpectralReverb* currentInstance_ = nullptr;

    float sampleRate_ = 44100.0f;
    float intervalSamples_ = 0.0f;

    // STFT buffers
    float inBuf_[BUFFSIZE] = {};
    float middleBuf_[BUFFSIZE] = {};
    float outBuf_[BUFFSIZE] = {};
    float reverbEnergy_[N / 2] = {};

    // FFT / STFT objects
    ShyFFT<float, N, RotationPhasor>* fft_ = nullptr;
    soundmath::Fourier<float, N>* stft_ = nullptr;

    // Window function
    soundmath::Wave<float> hann_{[](float phase) -> float {
        return 0.5f * (1.0f - std::cos(2.0f * (float)M_PI * phase));
    }};

    // Lo-fi processors
    SimpleSampleRateReducer sampleRateReducer_;
    SimpleTone lowpass_;

    // Drift oscillators
    SimpleOscillator driftOsc_[4];
    float driftMult_[4] = {1.0f, 1.0f, 1.0f, 1.0f};

    // User parameters
    float decay_ = 10.0f;
    float mix_ = 0.5f;
    float damp_ = 0.1f;
    float shimmer_ = 0.0f;
    float shimmerTone_ = 0.0f;
    float detune_ = 0.0f;
    bool freeze_ = false;
    int shimmerMode_ = 1;  // 0=down, 1=up, 2=both
    int lofiMode_ = 1;     // 0=less, 1=normal, 2=more
    int driftMode_ = 1;    // 0=slow, 1=none, 2=fast

    // Cached derived values (updated each sample)
    float cachedDamp_ = 0.1f;
    float cachedShimmerDouble_ = 0.0f;
    float cachedShimmerTriple_ = 0.0f;
    float cachedShimmerRemainder_ = 1.0f;
    float cachedDetuneDouble_ = 0.0f;
    float cachedDetuneRemainder_ = 1.0f;
    int cachedDetuneMode_ = 1;
    int cachedDetuneMul_ = 1;
};

} // namespace funbox_dsp
