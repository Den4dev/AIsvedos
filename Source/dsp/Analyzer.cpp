#include "Analyzer.h"

#include <cstring>

namespace vp
{

Analyzer::Analyzer()
{
    for (auto& band : meterBands)
        band.store (-100.0f, std::memory_order_relaxed);
}

void Analyzer::prepare (double newSampleRate, int numChannels)
{
    juce::ignoreUnused (numChannels);

    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    history.assign ((size_t) fftSize, 0.0f);
    pending.assign ((size_t) hopSize, 0.0f);
    fftBuffer.assign ((size_t) fftSize * 2, 0.0f);
    pendingCount = 0;

    kShelf.setHighShelf (sampleRate, 1500.0f, 4.0f);
    kHighPass.setHighPass (sampleRate, 38.0f, 0.5f);

    lufsCoeff = 1.0f - std::exp (-1.0f / (float) (sampleRate * 0.4));
    rmsCoeff = 1.0f - std::exp (-1.0f / (float) (sampleRate * 0.1));

    reset();
}

void Analyzer::reset()
{
    kShelf.reset();
    kHighPass.reset();
    lufsMeanSquare = 0.0f;
    rmsMeanSquare = 0.0f;
    peakLinear = 0.0f;
    pendingCount = 0;
    std::fill (history.begin(), history.end(), 0.0f);

    for (auto& band : meterBands)
        band.store (-100.0f, std::memory_order_relaxed);

    meterLufs.store (-100.0f, std::memory_order_relaxed);
    meterActivity.store (0.0f, std::memory_order_relaxed);
    peakDb.store (-100.0f, std::memory_order_relaxed);
    rmsDb.store (-100.0f, std::memory_order_relaxed);

    capturePower.fill (0.0);
    capturedFrames.store (0, std::memory_order_relaxed);
    captureComplete.store (false, std::memory_order_relaxed);
    capturing.store (false, std::memory_order_relaxed);
    captureLufsSum = 0.0;
    captureLufsFrames = 0;
}

void Analyzer::process (const juce::AudioBuffer<float>& buffer) noexcept
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (numSamples <= 0 || numChannels <= 0)
        return;

    const float* left = buffer.getReadPointer (0);
    const float* right = numChannels > 1 ? buffer.getReadPointer (1) : left;

    float monoChunk[512];

    for (int offset = 0; offset < numSamples; offset += 512)
    {
        const int chunk = std::min (512, numSamples - offset);

        for (int i = 0; i < chunk; ++i)
        {
            const float m = 0.5f * (left[offset + i] + right[offset + i]);
            monoChunk[i] = m;

            const float k = kHighPass.processSample (0, kShelf.processSample (0, m));
            lufsMeanSquare += lufsCoeff * (k * k - lufsMeanSquare);
            rmsMeanSquare += rmsCoeff * (m * m - rmsMeanSquare);
            peakLinear = std::max (peakLinear, std::abs (m));
        }

        pushSamples (monoChunk, chunk);
    }

    const float lufs = -0.691f + 10.0f * std::log10 (lufsMeanSquare + 1.0e-12f);
    const float rms = 10.0f * std::log10 (rmsMeanSquare + 1.0e-12f);
    const float peak = 20.0f * std::log10 (peakLinear + 1.0e-9f);

    meterLufs.store (std::max (lufs, -100.0f), std::memory_order_relaxed);
    rmsDb.store (std::max (rms, -100.0f), std::memory_order_relaxed);
    peakDb.store (std::max (peak, -100.0f), std::memory_order_relaxed);
    meterActivity.store (std::clamp ((rms + 55.0f) / 45.0f, 0.0f, 1.0f), std::memory_order_relaxed);

    peakLinear *= 0.999f;
}

void Analyzer::pushSamples (const float* mono, int numSamples) noexcept
{
    for (int i = 0; i < numSamples; ++i)
    {
        pending[(size_t) pendingCount++] = mono[i];

        if (pendingCount == hopSize)
        {
            std::memmove (history.data(), history.data() + hopSize, sizeof (float) * (size_t) (fftSize - hopSize));
            std::memcpy (history.data() + (fftSize - hopSize), pending.data(), sizeof (float) * (size_t) hopSize);
            pendingCount = 0;
            doFFT();
        }
    }
}

void Analyzer::doFFT() noexcept
{
    if (const int requested = requestedCaptureFrames.exchange (0, std::memory_order_acq_rel); requested > 0)
    {
        capturePower.fill (0.0);
        capturedFrames.store (0, std::memory_order_relaxed);
        captureTargetFrames.store (requested, std::memory_order_relaxed);
        captureComplete.store (false, std::memory_order_relaxed);
        capturing.store (true, std::memory_order_release);
        captureLufsSum = 0.0;
        captureLufsFrames = 0;
    }

    std::copy (history.begin(), history.end(), fftBuffer.begin());
    window.multiplyWithWindowingTable (fftBuffer.data(), (size_t) fftSize);
    std::fill (fftBuffer.begin() + fftSize, fftBuffer.end(), 0.0f);
    fft.performFrequencyOnlyForwardTransform (fftBuffer.data());

    const float powerNorm = 1.0f / std::pow ((float) fftSize * 0.25f, 2.0f);
    const float binWidth = (float) sampleRate / (float) fftSize;
    std::array<float, kNumBands> powers {};

    for (int bin = 1; bin < fftSize / 2; ++bin)
    {
        const float freq = (float) bin * binWidth;
        const float position = bandPosition (freq);

        if (position < 0.0f || position > (float) (kNumBands - 1))
            continue;

        const int band = (int) std::lround (position);
        const float magnitude = fftBuffer[(size_t) bin];
        powers[(size_t) std::clamp (band, 0, kNumBands - 1)] += magnitude * magnitude * powerNorm;
    }

    for (int i = 0; i < kNumBands; ++i)
    {
        const float db = 10.0f * std::log10 (powers[(size_t) i] + 1.0e-12f);
        const float previous = meterBands[(size_t) i].load (std::memory_order_relaxed);
        const float smoothed = db > previous ? db : previous + 0.25f * (db - previous);
        meterBands[(size_t) i].store (smoothed, std::memory_order_relaxed);
    }

    if (capturing.load (std::memory_order_acquire))
    {
        const float rmsLevel = 10.0f * std::log10 (rmsMeanSquare + 1.0e-12f);
        const float activity = std::clamp ((rmsLevel + 55.0f) / 45.0f, 0.0f, 1.0f);

        if (activity > 0.15f)
        {
            for (int i = 0; i < kNumBands; ++i)
                capturePower[(size_t) i] += powers[(size_t) i];

            captureLufsSum += (double) lufsMeanSquare;
            ++captureLufsFrames;

            const int frames = capturedFrames.fetch_add (1, std::memory_order_acq_rel) + 1;

            if (frames >= captureTargetFrames.load (std::memory_order_relaxed))
            {
                capturing.store (false, std::memory_order_release);
                captureComplete.store (true, std::memory_order_release);
            }
        }
    }
}

BandFrame Analyzer::getFrame() const noexcept
{
    BandFrame frame;
    frame.lufs = meterLufs.load (std::memory_order_relaxed);
    frame.activity = meterActivity.load (std::memory_order_relaxed);

    for (int i = 0; i < kNumBands; ++i)
        frame.bandsDb[(size_t) i] = meterBands[(size_t) i].load (std::memory_order_relaxed);

    return frame;
}

void Analyzer::beginCapture (float seconds)
{
    const int frames = (int) std::max (1.0f, seconds * (float) sampleRate / (float) hopSize);
    requestedCaptureFrames.store (frames, std::memory_order_release);
}

float Analyzer::getCaptureProgress() const noexcept
{
    const int target = captureTargetFrames.load (std::memory_order_relaxed);

    if (target <= 0)
        return 0.0f;

    return std::clamp ((float) capturedFrames.load (std::memory_order_relaxed) / (float) target, 0.0f, 1.0f);
}

BandProfile Analyzer::finishCapture()
{
    BandProfile profile;
    const int frames = std::max (1, capturedFrames.load (std::memory_order_acquire));
    profile.valid = capturedFrames.load (std::memory_order_acquire) > 0;

    for (int i = 0; i < kNumBands; ++i)
        profile.bandsDb[(size_t) i] = 10.0f * std::log10 ((float) (capturePower[(size_t) i] / frames) + 1.0e-12f);

    normaliseProfile (profile);
    profile.lufs = captureLufsFrames > 0
        ? -0.691f + 10.0f * std::log10 ((float) (captureLufsSum / (double) captureLufsFrames) + 1.0e-12f)
        : meterLufs.load (std::memory_order_relaxed);
    profile.crestDb = peakDb.load (std::memory_order_relaxed) - rmsDb.load (std::memory_order_relaxed);

    captureComplete.store (false, std::memory_order_release);
    return profile;
}

} // namespace vp
