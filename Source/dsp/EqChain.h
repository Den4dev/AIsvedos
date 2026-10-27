#pragma once

#include <array>

#include "Biquad.h"
#include "Params.h"

namespace vp
{

class EqChain
{
public:
    void prepare (double sampleRate, int numChannels);
    void reset();

    void setLowCut (float hz) noexcept;
    void setStaticTargets (const std::array<float, kNumEqBands>& gainsDb) noexcept;
    void setDynamicTargets (float lowCutDb, float highCutDb, float deEssCutDb) noexcept;

    void tick() noexcept;
    void process (float* const* channels, int numChannels, int numSamples) noexcept;

private:
    void updateCoefficients() noexcept;

    static constexpr int kNumDynamic = 3;
    static constexpr std::array<float, kNumDynamic> kDynamicFreqs { 1600.0f, 3500.0f, 6500.0f };
    static constexpr std::array<float, kNumDynamic> kDynamicQ { 1.0f, 1.0f, 0.85f };

    double sampleRate = 48000.0;

    Biquad highPass[2][2];
    Biquad statics[kNumEqBands][2];
    Biquad dynamics[kNumDynamic][2];

    std::array<float, kNumEqBands> staticTarget {};
    std::array<float, kNumEqBands> staticCurrent {};
    std::array<float, kNumDynamic> dynamicTarget {};
    std::array<float, kNumDynamic> dynamicCurrent {};

    float lowCutHz = 90.0f;
    float lowCutCurrent = 90.0f;
    bool coefficientsDirty = true;
};

} // namespace vp
