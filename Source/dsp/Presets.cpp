#include "Presets.h"

namespace vp
{

const GenrePreset& getGenrePreset (Genre genre) noexcept
{
    static const GenrePreset trap {
        "Trap", 100.0f, 0.55f, 0.55f, 0.65f, 0.10f,
        { -1.0f, 0.0f, 1.0f, 1.0f, 0.0f, -0.5f, 0.0f, 0.5f, 1.5f, 1.0f, 0.0f, 1.0f, 0.5f }
    };
    static const GenrePreset drill {
        "Drill", 105.0f, 0.60f, 0.55f, 0.65f, 0.12f,
        { -1.5f, 0.0f, 0.5f, 0.5f, -0.5f, -0.5f, 0.0f, 0.5f, 1.5f, 1.0f, 0.5f, 1.0f, 0.5f }
    };
    static const GenrePreset hiphop {
        "Hip-Hop", 90.0f, 0.50f, 0.50f, 0.55f, 0.12f,
        { 0.0f, 1.0f, 1.0f, 0.0f, -1.0f, 0.0f, 0.5f, 1.0f, 0.5f, 0.0f, 0.0f, 0.5f, 0.0f }
    };
    static const GenrePreset pop {
        "Pop", 80.0f, 0.40f, 0.45f, 0.50f, 0.06f,
        { 0.0f, 0.0f, 0.0f, 0.0f, -0.5f, 0.0f, 0.0f, 0.5f, 1.0f, 1.0f, 1.0f, 1.5f, 1.0f }
    };
    static const GenrePreset rnb {
        "R&B", 85.0f, 0.42f, 0.45f, 0.50f, 0.08f,
        { 0.5f, 0.5f, 0.5f, 0.0f, -0.5f, 0.0f, 0.0f, 0.5f, 1.0f, 1.0f, 1.0f, 1.5f, 1.0f }
    };
    static const GenrePreset rock {
        "Rock", 70.0f, 0.35f, 0.40f, 0.40f, 0.18f,
        { 0.0f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f }
    };
    static const GenrePreset indie {
        "Indie", 75.0f, 0.30f, 0.40f, 0.35f, 0.12f,
        { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.5f, 0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 0.0f }
    };

    switch (genre)
    {
        case Genre::drill:  return drill;
        case Genre::hiphop: return hiphop;
        case Genre::pop:    return pop;
        case Genre::rnb:    return rnb;
        case Genre::rock:   return rock;
        case Genre::indie:  return indie;
        case Genre::trap:
        default:            return trap;
    }
}

const char* genreName (Genre genre) noexcept
{
    return getGenrePreset (genre).name;
}

} // namespace vp
