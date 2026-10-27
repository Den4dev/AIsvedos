#pragma once

#include <array>
#include <atomic>
#include <memory>

#include <juce_audio_basics/juce_audio_basics.h>

#include "Analyzer.h"
#include "Compressor.h"
#include "DeEsser.h"
#include "EqChain.h"
#include "Params.h"
#include "SessionBus.h"

namespace vp
{

template <typename T>
class DoubleBuffer
{
public:
    void write (const T& value) noexcept
    {
        const juce::SpinLock::ScopedLockType lock (writeLock);
        const int next = (index.load (std::memory_order_relaxed) + 1) % 3;
        buffers[next] = value;
        index.store (next, std::memory_order_release);
    }

    T read() const noexcept
    {
        for (;;)
        {
            const int slot = index.load (std::memory_order_acquire);
            const T copy = buffers[slot];

            if (index.load (std::memory_order_acquire) == slot)
                return copy;
        }
    }

private:
    T buffers[3] {};
    std::atomic<int> index { 0 };
    juce::SpinLock writeLock;
};

class MixEngine
{
public:
    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset();

    void setParams (const MixParams& newParams) noexcept { params = newParams; }
    void setBus (SessionBus* newBus) noexcept { bus = newBus; }

    void setOppositeFrameOverride (const BandFrame& frame) noexcept
    {
        oppositeOverride = frame;
        oppositeOverrideValid = true;
    }

    void clearOppositeFrameOverride() noexcept { oppositeOverrideValid = false; }

    void process (juce::AudioBuffer<float>& buffer, float tempo) noexcept;

    BandFrame getInputFrame() const noexcept { return inputAnalyzer.getFrame(); }
    BandFrame getOutputFrame() const noexcept { return outputAnalyzer.getFrame(); }
    float getGainReductionDb() const noexcept { return grMeter.load (std::memory_order_relaxed); }
    float getDeEssDb() const noexcept { return deessMeter.load (std::memory_order_relaxed); }
    float getAutoGainDb() const noexcept { return autoGainMeter.load (std::memory_order_relaxed); }
    void getEqCurve (float* out) const noexcept;

    void beginVocalCapture (float seconds) { inputAnalyzer.beginCapture (seconds); }
    bool isCapturing() const noexcept { return inputAnalyzer.isCapturing(); }
    bool isCaptureComplete() const noexcept { return inputAnalyzer.isCaptureComplete(); }
    float getCaptureProgress() const noexcept { return inputAnalyzer.getCaptureProgress(); }
    BandProfile takeVocalProfile();

    void setReferenceProfile (const BandProfile& profile);
    void clearReference();
    bool hasReference() const noexcept { return referenceValid.load (std::memory_order_acquire); }
    BandProfile getReferenceProfile() const noexcept { return referenceBuffer.read(); }

    void setVocalProfile (const BandProfile& profile);
    bool hasVocalProfile() const noexcept { return vocalProfileValid.load (std::memory_order_acquire); }
    BandProfile getVocalProfile() const noexcept { return vocalProfileBuffer.read(); }

    bool isOppositeLinked() const noexcept;

private:
    bool readOppositeFrame (BandFrame& out) const noexcept;
    std::array<float, kNumEqBands> computeEqTargets (const BandFrame& ownFrame) const noexcept;
    void computeUnmask (const BandFrame& ownFrame, int numSamples) noexcept;

    MixParams params;
    double sampleRate = 48000.0;
    int maxBlockSize = 512;
    int maxChannels = 2;

    Analyzer inputAnalyzer;
    Analyzer outputAnalyzer;
    EqChain eq;
    Compressor compA;
    Compressor compB;
    DeEsser deEsser;
    juce::AudioBuffer<float> dry;

    SessionBus* bus = nullptr;
    bool oppositeOverrideValid = false;
    BandFrame oppositeOverride;

    std::atomic<float> grMeter { 0.0f };
    std::atomic<float> deessMeter { 0.0f };
    std::atomic<float> autoGainMeter { 0.0f };
    std::array<std::atomic<float>, kNumEqBands> curveMeters {};

    float autoGainDb = 0.0f;
    std::array<float, 2> unmaskCut { 0.0f, 0.0f };
    float outputGainSmoothed = 1.0f;
    float mixSmoothed = 1.0f;

    DoubleBuffer<BandProfile> referenceBuffer;
    std::atomic<bool> referenceValid { false };
    DoubleBuffer<BandProfile> vocalProfileBuffer;
    std::atomic<bool> vocalProfileValid { false };
};

} // namespace vp
