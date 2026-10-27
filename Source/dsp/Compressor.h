#pragma once

#include <juce_core/juce_core.h>

namespace vp
{

class Compressor
{
public:
    void prepare (double sampleRate);
    void reset();

    void setParams (float amount,
                    float thresholdDb,
                    float ratio,
                    float attackMs,
                    float releaseMs,
                    float kneeDb) noexcept;

    void process (float* left, float* right, int numSamples) noexcept;

    float getMaxGainReductionDb() const noexcept { return maxGrDb; }
    void resetMeter() noexcept { maxGrDb = 0.0f; }

private:
    double sampleRate = 48000.0;
    float attackCoeff = 0.0f;
    float releaseCoeff = 0.0f;
    float envelope = 0.0f;
    float threshold = -18.0f;
    float ratio = 2.0f;
    float knee = 6.0f;
    float makeupGain = 1.0f;
    float maxGrDb = 0.0f;
};

} // namespace vp
