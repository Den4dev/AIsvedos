#pragma once

#include <array>

#include "Params.h"

namespace vp
{

struct GenrePreset
{
    const char* name = "Trap";
    float lowCutHz = 90.0f;
    float compAmount = 0.5f;
    float deEssAmount = 0.5f;
    float unmaskAmount = 0.5f;
    float saturation = 0.1f;
    std::array<float, kNumEqBands> eqDb {};
};

const GenrePreset& getGenrePreset (Genre genre) noexcept;
const char* genreName (Genre genre) noexcept;

} // namespace vp
