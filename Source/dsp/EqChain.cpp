#include "EqChain.h"

#include <algorithm>
#include <cmath>

namespace vp
{

void EqChain::prepare (double newSampleRate, int numChannels)
{
    juce::ignoreUnused (numChannels);
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    coefficientsDirty = true;
    reset();
}

void EqChain::reset()
{
    for (auto& stage : highPass)
        for (auto& filter : stage)
            filter.reset();

    for (auto& band : statics)
        for (auto& filter : band)
            filter.reset();

    for (auto& band : dynamics)
        for (auto& filter : band)
            filter.reset();

    staticCurrent.fill (0.0f);
    dynamicCurrent.fill (0.0f);
}

void EqChain::setLowCut (float hz) noexcept
{
    lowCutHz = std::clamp (hz, 20.0f, 240.0f);
}

void EqChain::setStaticTargets (const std::array<float, kNumEqBands>& gainsDb) noexcept
{
    staticTarget = gainsDb;
}

void EqChain::setDynamicTargets (float lowCutDb, float highCutDb, float deEssCutDb) noexcept
{
    dynamicTarget[0] = std::clamp (lowCutDb, -12.0f, 0.0f);
    dynamicTarget[1] = std::clamp (highCutDb, -12.0f, 0.0f);
    dynamicTarget[2] = std::clamp (deEssCutDb, -12.0f, 0.0f);
}

void EqChain::tick() noexcept
{
    bool changed = false;

    if (std::abs (lowCutHz - lowCutCurrent) > 0.05f)
    {
        lowCutCurrent += 0.15f * (lowCutHz - lowCutCurrent);
        changed = true;
    }

    for (int i = 0; i < kNumEqBands; ++i)
    {
        const float delta = staticTarget[(size_t) i] - staticCurrent[(size_t) i];

        if (std::abs (delta) > 0.001f)
        {
            staticCurrent[(size_t) i] += 0.18f * delta;
            changed = true;
        }
    }

    for (int i = 0; i < kNumDynamic; ++i)
    {
        const float delta = dynamicTarget[(size_t) i] - dynamicCurrent[(size_t) i];

        if (std::abs (delta) > 0.001f)
        {
            dynamicCurrent[(size_t) i] += 0.25f * delta;
            changed = true;
        }
    }

    if (changed || coefficientsDirty)
        updateCoefficients();
}

void EqChain::updateCoefficients() noexcept
{
    for (int ch = 0; ch < 2; ++ch)
    {
        highPass[0][ch].setHighPass (sampleRate, lowCutCurrent, 0.707f);
        highPass[1][ch].setHighPass (sampleRate, lowCutCurrent, 0.707f);

        for (int i = 0; i < kNumEqBands; ++i)
            statics[i][ch].setPeak (sampleRate, kEqFreqs[(size_t) i], 1.1f, staticCurrent[(size_t) i]);

        for (int i = 0; i < kNumDynamic; ++i)
            dynamics[i][ch].setPeak (sampleRate, kDynamicFreqs[(size_t) i], kDynamicQ[(size_t) i], dynamicCurrent[(size_t) i]);
    }

    coefficientsDirty = false;
}

void EqChain::process (float* const* channels, int numChannels, int numSamples) noexcept
{
    for (int ch = 0; ch < numChannels && ch < 2; ++ch)
    {
        float* data = channels[ch];

        if (data == nullptr)
            continue;

        for (int i = 0; i < numSamples; ++i)
        {
            float x = data[i];
            x = highPass[0][ch].processSample (ch, x);
            x = highPass[1][ch].processSample (ch, x);

            for (int b = 0; b < kNumEqBands; ++b)
                x = statics[b][ch].processSample (ch, x);

            for (int b = 0; b < kNumDynamic; ++b)
                x = dynamics[b][ch].processSample (ch, x);

            data[i] = x;
        }
    }
}

} // namespace vp
