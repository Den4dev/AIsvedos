#include "DeEsser.h"

#include <algorithm>
#include <cmath>

namespace vp
{

void DeEsser::prepare (double newSampleRate)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    detectorLeft.setBandPass (sampleRate, 7000.0f, 0.9f);
    detectorRight.setBandPass (sampleRate, 7000.0f, 0.9f);
    attackCoeff = std::exp (-1.0f / (float) (sampleRate * 0.0005));
    releaseCoeff = std::exp (-1.0f / (float) (sampleRate * 0.035));
    reset();
}

void DeEsser::reset()
{
    detectorLeft.reset();
    detectorRight.reset();
    envelope = 0.0f;
    amountSmoothed = 0.0f;
}

float DeEsser::process (const float* left, const float* right, int numSamples, float amount) noexcept
{
    amountSmoothed += 0.2f * (std::clamp (amount, 0.0f, 1.0f) - amountSmoothed);

    if (amountSmoothed < 0.001f)
        return 0.0f;

    float maxReduction = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        const float l = std::abs (detectorLeft.processSample (0, left[i]));
        const float r = right != nullptr ? std::abs (detectorRight.processSample (1, right[i])) : l;
        const float peak = std::max (l, r);

        const float coeff = peak > envelope ? attackCoeff : releaseCoeff;
        envelope = coeff * envelope + (1.0f - coeff) * peak;

        const float levelDb = 20.0f * std::log10 (envelope + 1.0e-9f);
        const float over = levelDb + 32.0f; // threshold is -32 dBFS on the sibilance band
        const float reduction = over > 0.0f ? std::min (over * 0.9f, 10.0f) * amountSmoothed : 0.0f;
        maxReduction = std::max (maxReduction, reduction);
    }

    return maxReduction;
}

} // namespace vp
