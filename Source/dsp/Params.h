#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace vp
{

static constexpr int kNumBands = 32;
static constexpr int kNumEqBands = 13;

static constexpr std::array<float, kNumEqBands> kEqFreqs {
    63.0f, 100.0f, 160.0f, 250.0f, 400.0f, 630.0f, 1000.0f,
    1600.0f, 2500.0f, 4000.0f, 6300.0f, 10000.0f, 16000.0f
};

inline float bandFrequency (int index) noexcept
{
    return 40.0f * std::pow (400.0f, (float) index / (float) (kNumBands - 1));
}

inline float bandPosition (float hz) noexcept
{
    const float clamped = std::max (20.0f, hz);
    return std::log (clamped / 40.0f) / std::log (400.0f) * (float) (kNumBands - 1);
}

inline float bandValueAt (const std::array<float, kNumBands>& values, float hz) noexcept
{
    const float pos = std::clamp (bandPosition (hz), 0.0f, (float) (kNumBands - 1));
    const int i0 = (int) std::floor (pos);
    const int i1 = std::min (kNumBands - 1, i0 + 1);
    const float t = pos - (float) i0;
    return values[(size_t) i0] * (1.0f - t) + values[(size_t) i1] * t;
}

inline float dbToGain (float db) noexcept
{
    return std::pow (10.0f, db * 0.05f);
}

inline float gainToDb (float gain) noexcept
{
    return 20.0f * std::log10 (std::max (gain, 1.0e-9f));
}

struct BandFrame
{
    std::array<float, kNumBands> bandsDb {};
    float lufs = -100.0f;
    float activity = 0.0f;
    float tempo = 0.0f;
};

struct BandProfile
{
    std::array<float, kNumBands> bandsDb {};
    std::array<float, kNumBands> tiltDb {};
    float lufs = -100.0f;
    float crestDb = 0.0f;
    bool valid = false;
};

inline void normaliseProfile (BandProfile& profile) noexcept
{
    double sum = 0.0;
    int count = 0;

    for (int i = 0; i < kNumBands; ++i)
    {
        const float f = bandFrequency (i);

        if (f >= 200.0f && f <= 8000.0f)
        {
            sum += profile.bandsDb[(size_t) i];
            ++count;
        }
    }

    const float mean = count > 0 ? (float) (sum / count) : 0.0f;

    for (int i = 0; i < kNumBands; ++i)
        profile.tiltDb[(size_t) i] = profile.bandsDb[(size_t) i] - mean;
}

enum class Role
{
    vocal = 0,
    beat = 1
};

enum class Mode
{
    fit = 0,
    style = 1,
    reference = 2
};

enum class Genre
{
    trap = 0,
    drill = 1,
    hiphop = 2,
    pop = 3,
    rnb = 4,
    rock = 5,
    indie = 6
};

struct MixParams
{
    Role role = Role::vocal;
    Mode mode = Mode::fit;
    Genre genre = Genre::trap;
    float fitAmount = 0.5f;
    float refMatch = 0.5f;
    float lowCutHz = 90.0f;
    float compAmount = 0.5f;
    float deEssAmount = 0.5f;
    float unmaskAmount = 0.5f;
    float saturation = 0.5f;
    float reverbAmount = 0.35f;
    float delayAmount = 0.35f;
    float outputGainDb = 0.0f;
    float mix = 1.0f;
    bool bypass = false;
    bool autoGain = true;
};

} // namespace vp
