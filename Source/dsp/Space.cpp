#include "Space.h"

#include <algorithm>
#include <cmath>

namespace vp
{

namespace
{
float onePoleCoeff (double sampleRate, double tauSeconds) noexcept
{
    return (float) std::exp (-1.0 / (sampleRate * tauSeconds));
}
} // namespace

void Space::prepare (double newSampleRate, int maxBlockSize)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;

    reverb.setSampleRate (sampleRate);

    juce::Reverb::Parameters reverbParams;
    reverbParams.roomSize = 0.70f;
    reverbParams.damping = 0.45f;
    reverbParams.wetLevel = 0.33f;
    reverbParams.dryLevel = 0.0f;
    reverbParams.width = 1.0f;
    reverb.setParameters (reverbParams);

    const int preDelaySize = (int) std::ceil (sampleRate * 0.15);
    const int delaySize = (int) std::ceil (sampleRate * 2.0);

    for (int ch = 0; ch < 2; ++ch)
    {
        preDelay[ch].assign ((size_t) std::max (16, preDelaySize), 0.0f);
        delayLine[ch].assign ((size_t) std::max (16, delaySize), 0.0f);

        reverbHighPass[ch].setHighPass (sampleRate, 180.0f, 0.707f);
        reverbLowPass[ch].setLowPass (sampleRate, 9000.0f, 0.707f);
        delayHighPass[ch].setHighPass (sampleRate, 280.0f, 0.707f);
        delayLowPass[ch].setLowPass (sampleRate, 5500.0f, 0.707f);
    }

    reverbBuffer.setSize (2, std::max (1, maxBlockSize), false, true, true);

    duckAttack = onePoleCoeff (sampleRate, 0.005);
    duckRelease = onePoleCoeff (sampleRate, 0.150);

    reset();
}

void Space::reset()
{
    reverb.reset();

    for (int ch = 0; ch < 2; ++ch)
    {
        std::fill (preDelay[ch].begin(), preDelay[ch].end(), 0.0f);
        std::fill (delayLine[ch].begin(), delayLine[ch].end(), 0.0f);
        reverbHighPass[ch].reset();
        reverbLowPass[ch].reset();
        delayHighPass[ch].reset();
        delayLowPass[ch].reset();
    }

    reverbBuffer.clear();
    preDelayWrite = 0;
    delayWrite = 0;
    preDelaySamples = preDelayTarget = (float) (sampleRate * 0.03);
    delaySamples = delayTarget = (float) (sampleRate * 0.25);
    reverbSmoothed = 0.0f;
    delaySmoothed = 0.0f;
    duckEnvelope = 0.0f;
}

float Space::readLine (const std::vector<float>& line, int writePos, float delaySamples) const noexcept
{
    const int size = (int) line.size();

    if (size <= 0)
        return 0.0f;

    const float clampedDelay = std::clamp (delaySamples, 0.0f, (float) (size - 2));
    float pos = (float) writePos - clampedDelay;

    while (pos < 0.0f)
        pos += (float) size;

    const int i0 = (int) pos;

    if (i0 >= size)
        return 0.0f;

    const int i1 = (i0 + 1) % size;
    const float t = pos - std::floor (pos);
    return line[(size_t) i0] * (1.0f - t) + line[(size_t) i1] * t;
}

void Space::process (float* const* channels, int numChannels, int numSamples,
                     float reverbAmount, float delayAmount, float tempo) noexcept
{
    if (channels == nullptr || channels[0] == nullptr || numChannels <= 0 || numSamples <= 0)
        return;

    if (numSamples > reverbBuffer.getNumSamples())
        reverbBuffer.setSize (2, numSamples, false, true, false);

    const float blockCoeff = 1.0f - std::exp (-(float) numSamples / (float) (sampleRate * 0.02));
    reverbSmoothed += blockCoeff * (std::clamp (reverbAmount, 0.0f, 1.0f) - reverbSmoothed);
    delaySmoothed += blockCoeff * (std::clamp (delayAmount, 0.0f, 1.0f) - delaySmoothed);

    const float tempoSafe = tempo > 20.0f ? tempo : 120.0f;
    preDelayTarget = (float) std::clamp ((double) sampleRate * 60.0 / (double) tempoSafe / 8.0,
                                         sampleRate * 0.005, sampleRate * 0.12);
    delayTarget = (float) std::clamp ((double) sampleRate * 60.0 / (double) tempoSafe * 0.5,
                                      sampleRate * 0.06, sampleRate * 1.5);

    const float glide = 1.0f - std::exp (-(float) numSamples / (float) (sampleRate * 0.05));
    preDelaySamples += glide * (preDelayTarget - preDelaySamples);
    delaySamples += glide * (delayTarget - delaySamples);

    float* const revLeft = reverbBuffer.getWritePointer (0);
    float* const revRight = reverbBuffer.getWritePointer (1);
    const int preDelaySize = (int) preDelay[0].size();

    for (int i = 0; i < numSamples; ++i)
    {
        const float left = channels[0][i];
        const float right = numChannels > 1 ? channels[1][i] : left;

        preDelay[0][(size_t) preDelayWrite] = left;
        preDelay[1][(size_t) preDelayWrite] = right;
        revLeft[i] = readLine (preDelay[0], preDelayWrite, preDelaySamples);
        revRight[i] = readLine (preDelay[1], preDelayWrite, preDelaySamples);
        preDelayWrite = (preDelayWrite + 1) % preDelaySize;
    }

    reverb.processStereo (revLeft, revRight, numSamples);

    const float reverbGain = reverbSmoothed * reverbSmoothed * 0.75f;
    const float delayGain = delaySmoothed * delaySmoothed * 0.55f;
    constexpr float feedback = 0.34f;
    const int delaySize = (int) delayLine[0].size();

    for (int i = 0; i < numSamples; ++i)
    {
        const float left = channels[0][i];
        const float right = numChannels > 1 ? channels[1][i] : left;

        const float peak = std::max (std::abs (left), std::abs (right));
        const float envCoeff = peak > duckEnvelope ? duckAttack : duckRelease;
        duckEnvelope = envCoeff * duckEnvelope + (1.0f - envCoeff) * peak;
        const float duck = std::clamp ((duckEnvelope - 0.02f) / 0.06f, 0.0f, 1.0f);

        float wetReverbLeft = reverbLowPass[0].processSample (0, reverbBuffer.getSample (0, i));
        wetReverbLeft = reverbHighPass[0].processSample (0, wetReverbLeft);
        float wetReverbRight = reverbLowPass[1].processSample (1, reverbBuffer.getSample (1, i));
        wetReverbRight = reverbHighPass[1].processSample (1, wetReverbRight);

        float wetDelayLeft = delayLowPass[0].processSample (
            0, readLine (delayLine[0], delayWrite, delaySamples));
        wetDelayLeft = delayHighPass[0].processSample (0, wetDelayLeft);
        float wetDelayRight = delayLowPass[1].processSample (
            1, readLine (delayLine[1], delayWrite, delaySamples));
        wetDelayRight = delayHighPass[1].processSample (1, wetDelayRight);

        delayLine[0][(size_t) delayWrite] = left + wetDelayRight * feedback;
        delayLine[1][(size_t) delayWrite] = right + wetDelayLeft * feedback;

        const float reverbDuck = 1.0f - 0.70f * duck;
        const float delayDuck = 1.0f - 0.55f * duck;

        channels[0][i] = left
                       + wetReverbLeft * reverbGain * reverbDuck
                       + wetDelayLeft * delayGain * delayDuck;

        if (numChannels > 1)
            channels[1][i] = right
                           + wetReverbRight * reverbGain * reverbDuck
                           + wetDelayRight * delayGain * delayDuck;

        delayWrite = (delayWrite + 1) % delaySize;
    }
}

} // namespace vp
