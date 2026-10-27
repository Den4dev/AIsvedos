#pragma once

#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>

#include "Biquad.h"

namespace vp
{

class Space
{
public:
    void prepare (double sampleRate, int maxBlockSize);
    void reset();

    void process (float* const* channels, int numChannels, int numSamples,
                  float reverbAmount, float delayAmount, float tempo) noexcept;

private:
    float readLine (const std::vector<float>& line, int writePos, float delaySamples) const noexcept;

    double sampleRate = 48000.0;

    juce::Reverb reverb;
    juce::AudioBuffer<float> reverbBuffer;

    std::vector<float> preDelay[2];
    int preDelayWrite = 0;
    float preDelaySamples = 0.0f;
    float preDelayTarget = 0.0f;

    std::vector<float> delayLine[2];
    int delayWrite = 0;
    float delaySamples = 0.0f;
    float delayTarget = 0.0f;

    Biquad reverbHighPass[2];
    Biquad reverbLowPass[2];
    Biquad delayHighPass[2];
    Biquad delayLowPass[2];

    float reverbSmoothed = 0.0f;
    float delaySmoothed = 0.0f;
    float duckEnvelope = 0.0f;
    float duckAttack = 0.99f;
    float duckRelease = 0.999f;
};

} // namespace vp
