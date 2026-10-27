#pragma once

#include <algorithm>
#include <cmath>

#include <juce_core/juce_core.h>

namespace vp
{

struct Biquad
{
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    float z1[2] {}, z2[2] {};

    void reset() noexcept
    {
        z1[0] = z1[1] = z2[0] = z2[1] = 0.0f;
    }

    inline float processSample (int ch, float x) noexcept
    {
        const float y = b0 * x + z1[ch];
        z1[ch] = b1 * x - a1 * y + z2[ch];
        z2[ch] = b2 * x - a2 * y;
        return y;
    }

    void setPeak (double sr, float freq, float q, float gainDb) noexcept
    {
        const float A = std::pow (10.0f, gainDb / 40.0f);
        const float w0 = omega (sr, freq);
        const float alpha = std::sin (w0) / (2.0f * std::max (q, 0.05f));
        const float c = std::cos (w0);
        const float a0 = 1.0f + alpha / A;

        b0 = (1.0f + alpha * A) / a0;
        b1 = (-2.0f * c) / a0;
        b2 = (1.0f - alpha * A) / a0;
        a1 = (-2.0f * c) / a0;
        a2 = (1.0f - alpha / A) / a0;
    }

    void setHighPass (double sr, float freq, float q) noexcept
    {
        const float w0 = omega (sr, freq);
        const float alpha = std::sin (w0) / (2.0f * std::max (q, 0.05f));
        const float c = std::cos (w0);
        const float a0 = 1.0f + alpha;

        b0 = ((1.0f + c) * 0.5f) / a0;
        b1 = -(1.0f + c) / a0;
        b2 = b0;
        a1 = (-2.0f * c) / a0;
        a2 = (1.0f - alpha) / a0;
    }

    void setLowPass (double sr, float freq, float q) noexcept
    {
        const float w0 = omega (sr, freq);
        const float alpha = std::sin (w0) / (2.0f * std::max (q, 0.05f));
        const float c = std::cos (w0);
        const float a0 = 1.0f + alpha;

        b0 = ((1.0f - c) * 0.5f) / a0;
        b1 = (1.0f - c) / a0;
        b2 = b0;
        a1 = (-2.0f * c) / a0;
        a2 = (1.0f - alpha) / a0;
    }

    void setHighShelf (double sr, float freq, float gainDb, float slope = 1.0f) noexcept
    {
        const float A = std::pow (10.0f, gainDb / 40.0f);
        const float w0 = omega (sr, freq);
        const float c = std::cos (w0);
        const float s = std::sin (w0);
        const float alpha = s * 0.5f * std::sqrt ((A + 1.0f / A) * (1.0f / std::max (slope, 0.01f) - 1.0f) + 2.0f);
        const float twoSqrtAAlpha = 2.0f * std::sqrt (A) * alpha;
        const float a0 = (A + 1.0f) - (A - 1.0f) * c + twoSqrtAAlpha;

        b0 = A * ((A + 1.0f) + (A - 1.0f) * c + twoSqrtAAlpha) / a0;
        b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * c) / a0;
        b2 = A * ((A + 1.0f) + (A - 1.0f) * c - twoSqrtAAlpha) / a0;
        a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * c) / a0;
        a2 = ((A + 1.0f) - (A - 1.0f) * c - twoSqrtAAlpha) / a0;
    }

    void setBandPass (double sr, float freq, float q) noexcept
    {
        const float w0 = omega (sr, freq);
        const float alpha = std::sin (w0) / (2.0f * std::max (q, 0.05f));
        const float c = std::cos (w0);
        const float a0 = 1.0f + alpha;

        b0 = alpha / a0;
        b1 = 0.0f;
        b2 = -alpha / a0;
        a1 = (-2.0f * c) / a0;
        a2 = (1.0f - alpha) / a0;
    }

private:
    static float omega (double sr, float freq) noexcept
    {
        const float safe = std::clamp (freq, 10.0f, (float) (sr * 0.49));
        return juce::MathConstants<float>::twoPi * safe / (float) sr;
    }
};

} // namespace vp
