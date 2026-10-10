#pragma once
#include "ui/Widgets.h"
#include <algorithm>
#include <cmath>

namespace lamium::map {

// Pure logic for the minimap's ambient day/night cycle (ticks 0..24000).
// Overworld applies the cycle; Nether, The End, and Cave view stay full brightness.
// Night dims slightly with a soft moonlight cool tint without obscuring terrain readability.

inline constexpr float minDaylightFactor = 0.55f;

// Returns a normalized lighting factor in [minDaylightFactor, 1.0f].
inline float daylightFactor(int worldTicks, int dimensionId = 0, bool isCaveView = false) {
    if (dimensionId != 0 || isCaveView || worldTicks < 0) return 1.0f;

    int timeOfDay = (worldTicks % 24000 + 24000) % 24000;

    // Full daylight: 1000..11000
    if (timeOfDay >= 1000 && timeOfDay <= 11000) return 1.0f;

    constexpr float pi = 3.14159265358979323846f;

    // Sunset / dusk: 11000..13500 (smooth cosine curve from 1.0 down to minFactor)
    if (timeOfDay > 11000 && timeOfDay < 13500) {
        float t = static_cast<float>(timeOfDay - 11000) / 2500.0f;
        float smoothT = 0.5f * (1.0f + std::cos(t * pi));
        return minDaylightFactor + (1.0f - minDaylightFactor) * smoothT;
    }

    // Deep night: 13500..22500
    if (timeOfDay >= 13500 && timeOfDay <= 22500) return minDaylightFactor;

    // Sunrise / dawn: 22500..24000 / 0..1000 (span 2500 ticks from minFactor up to 1.0)
    float t = 0.0f;
    if (timeOfDay > 22500) {
        t = static_cast<float>(timeOfDay - 22500) / 2500.0f;
    } else {
        t = static_cast<float>(timeOfDay + 1500) / 2500.0f;
    }
    float smoothT = 0.5f * (1.0f - std::cos(t * pi));
    return minDaylightFactor + (1.0f - minDaylightFactor) * smoothT;
}

// Color tint for rendering the minimap texture via UI shader vertex multiplication.
inline ui::Rgb daylightTint(int worldTicks, int dimensionId = 0, bool isCaveView = false, bool enabled = true) {
    if (!enabled) return ui::palette::white;

    float factor = daylightFactor(worldTicks, dimensionId, isCaveView);
    if (factor >= 0.999f) return ui::palette::white;

    // Normalized progress in [0, 1] where 1.0 is full day and 0.0 is deep night.
    float tDay = std::clamp((factor - minDaylightFactor) / (1.0f - minDaylightFactor), 0.0f, 1.0f);

    // Deep night target: slight cool tint (R: 0.50, G: 0.54, B: 0.62)
    constexpr ui::Rgb nightColor{0.50f, 0.54f, 0.62f};

    return ui::Rgb{
        nightColor.r + (1.0f - nightColor.r) * tDay,
        nightColor.g + (1.0f - nightColor.g) * tDay,
        nightColor.b + (1.0f - nightColor.b) * tDay
    };
}

} // namespace lamium::map
