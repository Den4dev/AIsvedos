#pragma once

#include <array>
#include <atomic>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include "Biquad.h"
#include "Params.h"

namespace vp
{

class Analyzer
{
public:
    Analyzer();

    void prepare (double sampleRate, int numChannels);
    void reset();

    void process (const juce::AudioBuffer<float>& buffer) noexcept;

    BandFrame getFrame() const noexcept;
    float getPeakDb() const noexcept { return peakDb.load (std::memory_order_relaxed); }
    float getRmsDb() const noexcept { return rmsDb.load (std::memory_order_relaxed); }

    void beginCapture (float seconds);
    bool isCapturing() const noexcept { return capturing.load (std::memory_order_acquire); }
    bool isCaptureComplete() const noexcept { return captureComplete.load (std::memory_order_acquire); }
    float getCaptureProgress() const noexcept;
    BandProfile finishCapture();

private:
    void pushSamples (const float* mono, int numSamples) noexcept;
    void doFFT() noexcept;

    static constexpr int fftOrder = 13;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int hopSize = fftSize / 4;

    double sampleRate = 48000.0;
    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) fftSize, juce::dsp::WindowingFunction<float>::hann, false };

    std::vector<float> history;
    std::vector<float> pending;
    std::vector<float> fftBuffer;
    int pendingCount = 0;

    Biquad kShelf;
    Biquad kHighPass;
    float lufsMeanSquare = 0.0f;
    float lufsCoeff = 0.0f;
    float rmsMeanSquare = 0.0f;
    float rmsCoeff = 0.0f;

    std::array<std::atomic<float>, kNumBands> meterBands {};
    std::atomic<float> meterLufs { -100.0f };
    std::atomic<float> meterActivity { 0.0f };
    std::atomic<float> peakDb { -100.0f };
    std::atomic<float> rmsDb { -100.0f };
    float peakLinear = 0.0f;

    std::atomic<int> requestedCaptureFrames { 0 };
    std::atomic<int> captureTargetFrames { 0 };
    std::atomic<int> capturedFrames { 0 };
    std::atomic<bool> capturing { false };
    std::atomic<bool> captureComplete { false };
    std::array<double, kNumBands> capturePower {};
    double captureLufsSum = 0.0;
    int captureLufsFrames = 0;
};

} // namespace vp
