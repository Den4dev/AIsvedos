#include "MixEngine.h"

#include <algorithm>
#include <cmath>

#include "Presets.h"

namespace vp
{

void MixEngine::prepare (double newSampleRate, int newMaxBlockSize, int numChannels)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    maxBlockSize = std::max (1, newMaxBlockSize);
    maxChannels = std::clamp (numChannels, 1, 2);

    inputAnalyzer.prepare (sampleRate, maxChannels);
    outputAnalyzer.prepare (sampleRate, maxChannels);
    eq.prepare (sampleRate, maxChannels);
    compA.prepare (sampleRate);
    compB.prepare (sampleRate);
    deEsser.prepare (sampleRate);
    dry.setSize (maxChannels, maxBlockSize, false, true, true);

    for (auto& value : curveMeters)
        value.store (0.0f, std::memory_order_relaxed);

    reset();
}

void MixEngine::reset()
{
    inputAnalyzer.reset();
    outputAnalyzer.reset();
    eq.reset();
    compA.reset();
    compB.reset();
    deEsser.reset();
    dry.clear();
    autoGainDb = 0.0f;
    unmaskCut = { 0.0f, 0.0f };
    grMeter.store (0.0f, std::memory_order_relaxed);
    deessMeter.store (0.0f, std::memory_order_relaxed);
    autoGainMeter.store (0.0f, std::memory_order_relaxed);
}

void MixEngine::setReferenceProfile (const BandProfile& profile)
{
    referenceBuffer.write (profile);
    referenceValid.store (profile.valid, std::memory_order_release);
}

void MixEngine::clearReference()
{
    BandProfile empty;
    referenceBuffer.write (empty);
    referenceValid.store (false, std::memory_order_release);
}

void MixEngine::setVocalProfile (const BandProfile& profile)
{
    vocalProfileBuffer.write (profile);
    vocalProfileValid.store (profile.valid, std::memory_order_release);
}

BandProfile MixEngine::takeVocalProfile()
{
    BandProfile profile = inputAnalyzer.finishCapture();

    if (profile.valid)
    {
        vocalProfileBuffer.write (profile);
        vocalProfileValid.store (true, std::memory_order_release);
    }

    return profile;
}

bool MixEngine::readOppositeFrame (BandFrame& out) const noexcept
{
    if (oppositeOverrideValid)
    {
        out = oppositeOverride;
        return true;
    }

    if (bus == nullptr)
        return false;

    return params.role == Role::vocal ? bus->readBeat (out) : bus->readVocal (out);
}

bool MixEngine::isOppositeLinked() const noexcept
{
    BandFrame frame;
    return readOppositeFrame (frame);
}

std::array<float, kNumEqBands> MixEngine::computeEqTargets (const BandFrame& ownFrame) const noexcept
{
    juce::ignoreUnused (ownFrame);

    const auto& preset = getGenrePreset (params.genre);
    const float fit = std::clamp (params.fitAmount, 0.0f, 1.0f);
    const bool isVocal = params.role == Role::vocal;

    std::array<float, kNumEqBands> targets {};

    for (int i = 0; i < kNumEqBands; ++i)
        targets[(size_t) i] = preset.eqDb[(size_t) i] * fit * (isVocal ? 1.0f : 0.6f);

    if (isVocal && params.mode == Mode::reference
        && referenceValid.load (std::memory_order_acquire)
        && vocalProfileValid.load (std::memory_order_acquire))
    {
        const auto reference = referenceBuffer.read();
        const auto vocal = vocalProfileBuffer.read();
        const float amount = std::clamp (params.refMatch, 0.0f, 1.0f);

        for (int i = 0; i < kNumEqBands; ++i)
        {
            const float freq = kEqFreqs[(size_t) i];
            const float difference = bandValueAt (reference.tiltDb, freq) - bandValueAt (vocal.tiltDb, freq);

            float weight = 1.0f;

            if (freq < 150.0f)
                weight = 0.0f;
            else if (freq < 250.0f)
                weight = (freq - 150.0f) / 100.0f;
            else if (freq > 14000.0f)
                weight = 0.0f;
            else if (freq > 10000.0f)
                weight = (14000.0f - freq) / 4000.0f;

            targets[(size_t) i] += std::clamp (difference, -7.0f, 7.0f) * weight * amount;
        }
    }

    return targets;
}

void MixEngine::computeUnmask (const BandFrame& ownFrame) noexcept
{
    BandFrame opposite;
    const bool linked = readOppositeFrame (opposite);
    const bool isVocal = params.role == Role::vocal;
    const auto& preset = getGenrePreset (params.genre);
    const float strength = std::clamp (preset.unmaskAmount * params.unmaskAmount * 2.0f, 0.0f, 1.0f);

    std::array<float, 2> targets { 0.0f, 0.0f };

    if (linked && strength > 0.001f)
    {
        constexpr std::array<float, 2> freqs { 1600.0f, 3500.0f };

        for (int k = 0; k < 2; ++k)
        {
            const float freq = freqs[(size_t) k];
            const float oppositeLevel = bandValueAt (opposite.bandsDb, freq);
            const float ownLevel = bandValueAt (ownFrame.bandsDb, freq);

            if (isVocal)
            {
                const float masking = std::clamp ((oppositeLevel - ownLevel - 2.0f) / 12.0f, 0.0f, 1.0f);
                targets[(size_t) k] = -6.0f * strength * masking;
            }
            else
            {
                const float activity = std::clamp (opposite.activity, 0.0f, 1.0f);
                const float masking = std::clamp ((oppositeLevel - ownLevel) / 12.0f, 0.0f, 1.0f);
                targets[(size_t) k] = -4.0f * strength * activity * masking;
            }
        }
    }

    for (int k = 0; k < 2; ++k)
        unmaskCut[(size_t) k] += 0.18f * (targets[(size_t) k] - unmaskCut[(size_t) k]);
}

void MixEngine::process (juce::AudioBuffer<float>& buffer, float tempo) noexcept
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = std::min (buffer.getNumChannels(), maxChannels);

    if (numSamples <= 0 || numChannels <= 0)
        return;

    inputAnalyzer.process (buffer);

    BandFrame ownFrame = inputAnalyzer.getFrame();
    ownFrame.tempo = tempo;

    if (bus != nullptr)
    {
        if (params.role == Role::vocal)
            bus->publishVocal (ownFrame);
        else
            bus->publishBeat (ownFrame);
    }

    if (params.bypass)
    {
        outputAnalyzer.process (buffer);
        grMeter.store (0.0f, std::memory_order_relaxed);
        deessMeter.store (0.0f, std::memory_order_relaxed);
        return;
    }

    const bool isVocal = params.role == Role::vocal;
    const auto& preset = getGenrePreset (params.genre);
    const float fit = std::clamp (params.fitAmount, 0.0f, 1.0f);
    const float compAmount = std::clamp (preset.compAmount * params.compAmount * 2.0f, 0.0f, 1.0f);
    const float deEssAmount = std::clamp (preset.deEssAmount * params.deEssAmount * 2.0f, 0.0f, 1.0f);
    const float saturation = isVocal
        ? std::clamp (preset.saturation * params.saturation * 2.0f, 0.0f, 1.0f)
        : 0.0f;

    if (isVocal && params.autoGain)
    {
        const float measuredLufs = outputAnalyzer.getFrame().lufs;

        if (measuredLufs > -60.0f && ownFrame.activity > 0.15f)
        {
            float targetLufs = -18.0f;
            BandFrame opposite;

            if (readOppositeFrame (opposite) && opposite.lufs > -50.0f)
                targetLufs = std::clamp (opposite.lufs - 3.0f, -30.0f, -12.0f);

            autoGainDb = std::clamp (autoGainDb + (targetLufs - measuredLufs) * 0.01f, -12.0f, 12.0f);
        }
    }
    else if (! isVocal)
    {
        autoGainDb = 0.0f;
    }

    autoGainMeter.store (autoGainDb, std::memory_order_relaxed);

    if (std::abs (autoGainDb) > 0.01f)
        buffer.applyGain (dbToGain (autoGainDb));

    for (int ch = 0; ch < numChannels; ++ch)
        dry.copyFrom (ch, 0, buffer, ch, 0, numSamples);

    const auto eqTargets = computeEqTargets (ownFrame);

    for (int i = 0; i < kNumEqBands; ++i)
        curveMeters[(size_t) i].store (eqTargets[(size_t) i], std::memory_order_relaxed);

    eq.setStaticTargets (eqTargets);
    eq.setLowCut (isVocal ? params.lowCutHz : 25.0f);

    computeUnmask (ownFrame);

    if (isVocal)
    {
        compA.setParams (compAmount, -20.0f, 3.0f, 12.0f, 90.0f, 6.0f);
        compB.setParams (compAmount, -26.0f, 1.8f, 35.0f, 320.0f, 8.0f);
    }
    else
    {
        compA.setParams (0.12f * fit, -14.0f, 1.5f, 30.0f, 250.0f, 8.0f);
        compB.setParams (0.0f, -12.0f, 1.0f, 30.0f, 250.0f, 0.0f);
    }

    float blockGr = 0.0f;
    float blockDeEss = 0.0f;
    constexpr int chunkSize = 64;

    for (int offset = 0; offset < numSamples; offset += chunkSize)
    {
        const int chunk = std::min (chunkSize, numSamples - offset);

        float* chunkChannels[2] {};
        chunkChannels[0] = buffer.getWritePointer (0) + offset;
        chunkChannels[1] = numChannels > 1 ? buffer.getWritePointer (1) + offset : nullptr;

        const float deEssCut = isVocal
            ? deEsser.process (chunkChannels[0], chunkChannels[1], chunk, deEssAmount)
            : 0.0f;

        blockDeEss = std::max (blockDeEss, deEssCut);

        eq.setDynamicTargets (unmaskCut[0], unmaskCut[1], -deEssCut);
        eq.tick();
        eq.process (chunkChannels, numChannels, chunk);

        compA.process (chunkChannels[0], chunkChannels[1], chunk);
        compB.process (chunkChannels[0], chunkChannels[1], chunk);

        blockGr = std::max (blockGr, std::max (compA.getMaxGainReductionDb(), compB.getMaxGainReductionDb()));

        if (saturation > 0.001f)
        {
            const float drive = 1.0f + 6.0f * saturation;
            const float normaliser = 1.0f / std::tanh (drive);
            const float blend = saturation * 0.35f;

            for (int ch = 0; ch < numChannels; ++ch)
            {
                float* data = chunkChannels[ch];

                if (data == nullptr)
                    continue;

                for (int i = 0; i < chunk; ++i)
                {
                    const float x = data[i];
                    const float y = std::tanh (x * drive) * normaliser;
                    data[i] = x + blend * (y - x);
                }
            }
        }
    }

    grMeter.store (blockGr, std::memory_order_relaxed);
    deessMeter.store (blockDeEss, std::memory_order_relaxed);

    const float outputGain = dbToGain (params.outputGainDb);
    const float mix = std::clamp (params.mix, 0.0f, 1.0f);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* wet = buffer.getWritePointer (ch);
        const float* dryData = dry.getReadPointer (ch);

        for (int i = 0; i < numSamples; ++i)
        {
            const float processed = wet[i] * outputGain;
            wet[i] = dryData[i] + mix * (processed - dryData[i]);
        }
    }

    outputAnalyzer.process (buffer);
}

void MixEngine::getEqCurve (float* out) const noexcept
{
    for (int i = 0; i < kNumEqBands; ++i)
        out[i] = curveMeters[(size_t) i].load (std::memory_order_relaxed);
}

} // namespace vp
