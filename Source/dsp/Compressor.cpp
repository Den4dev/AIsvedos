#include "Compressor.h"

#include <algorithm>
#include <cmath>

namespace vp
{

void Compressor::prepare (double newSampleRate)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    reset();
}

void Compressor::reset()
{
    envelope = 0.0f;
    maxGrDb = 0.0f;
}

void Compressor::setParams (float amount,
                            float thresholdDb,
                            float baseRatio,
                            float attackMs,
                            float releaseMs,
                            float kneeDb) noexcept
{
    const float a = std::clamp (amount, 0.0f, 1.0f);
    threshold = thresholdDb - 2.0f * a;
    ratio = 1.0f + (baseRatio - 1.0f) * a;
    knee = kneeDb;

    attackCoeff = std::exp (-1.0f / (float) (sampleRate * std::max (attackMs, 0.05f) * 0.001));
    releaseCoeff = std::exp (-1.0f / (float) (sampleRate * std::max (releaseMs, 1.0f) * 0.001));

    const float makeupDb = -threshold * (1.0f - 1.0f / std::max (ratio, 1.0f)) * 0.35f;
    makeupGain = std::pow (10.0f, makeupDb / 20.0f);
}

void Compressor::process (float* left, float* right, int numSamples) noexcept
{
    const float invRatio = 1.0f - 1.0f / std::max (ratio, 1.0f);
    maxGrDb = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        const float l = left[i];
        const float r = right != nullptr ? right[i] : l;
        const float power = 0.5f * (l * l + r * r);

        const float coeff = power > envelope ? attackCoeff : releaseCoeff;
        envelope = coeff * envelope + (1.0f - coeff) * power;

        const float levelDb = 10.0f * std::log10 (envelope + 1.0e-12f);
        const float over = levelDb - threshold;
        float reduction = 0.0f;

        if (over > knee * 0.5f)
            reduction = over * invRatio;
        else if (over > -knee * 0.5f)
        {
            const float t = over + knee * 0.5f;
            reduction = t * t * invRatio / (2.0f * std::max (knee, 0.01f));
        }

        reduction = std::max (0.0f, reduction);
        maxGrDb = std::max (maxGrDb, reduction);

        const float gain = std::pow (10.0f, -reduction / 20.0f) * makeupGain;
        left[i] = l * gain;

        if (right != nullptr)
            right[i] = r * gain;
    }
}

} // namespace vp
