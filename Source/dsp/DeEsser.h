#pragma once

#include "Biquad.h"

namespace vp
{

class DeEsser
{
public:
    void prepare (double sampleRate);
    void reset();

    // Returns the maximum reduction in dB for this block (positive number = cut).
    float process (const float* left, const float* right, int numSamples, float amount) noexcept;

private:
    double sampleRate = 48000.0;
    Biquad detectorLeft;
    Biquad detectorRight;
    float envelope = 0.0f;
    float attackCoeff = 0.0f;
    float releaseCoeff = 0.0f;
    float amountSmoothed = 0.0f;
};

} // namespace vp
