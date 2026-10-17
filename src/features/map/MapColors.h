#pragma once
#include "features/map/MapTiles.h"
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace lamium::map {
// Block colors for the map (BACKLOG L-60): the average of the block's top
// texture, times the biome tint the world uses for it. Pure.

// Average of RGBA8 pixels weighted by alpha, so cut-out texels (leaves,
// glass edges) do not darken the color. Empty when nothing is visible.
inline std::optional<std::uint32_t> averageColor(std::uint8_t const* rgba, size_t pixelCount) {
    if (!rgba || !pixelCount) return std::nullopt;
    double r = 0, g = 0, b = 0, weight = 0;
    for (size_t i = 0; i < pixelCount; ++i) {
        auto const* p = rgba + i * 4;
        double a = p[3] / 255.0;
        r += p[0] * a;
        g += p[1] * a;
        b += p[2] * a;
        weight += a;
    }
    if (weight < .5) return std::nullopt;
    auto c = [&](double v) { return static_cast<int>(std::lround(v / weight)); };
    return packColor(c(r), c(g), c(b));
}
// A biome tint the map can use. The block renderer's tint comes back black
// (0, 0, 0) for a while after a chunk loads; black grass was saved over
// explored land (2026-10-07). No biome tints anything black.
inline bool usableTint(float r, float g, float b) {
    return std::isfinite(r) && std::isfinite(g) && std::isfinite(b) && r + g + b > .03f;
}
// The world multiplies gray grass, leaf and water textures by a biome color.
inline std::uint32_t tinted(std::uint32_t color, float r, float g, float b) {
    auto mul = [](int value, float factor) {
        return static_cast<int>(std::lround(value * std::clamp(std::isfinite(factor) ? factor : 1.f, 0.f, 1.f)));
    };
    return packColor(mul(channel(color, 0), r), mul(channel(color, 1), g), mul(channel(color, 2), b), channel(color, 3));
}
// Which block of a surface column the map shows. The heightmap is the top
// light-blocking block, so the scan starts one above its position: a
// covering block there (snow layer, carpet) wins over the ground below it.
// Plants and anything else that does not cover its column are looked
// through, and from the heightmap down any visible block counts.
inline constexpr bool columnBlockWins(bool skip, bool cover, int y, int heightmapY) {
    return !skip && (cover || y < heightmapY);
}
// A block covers its column when its footprint spans it: thin blocks that
// still cover the ground (snow layers, carpets) count on the map, plants
// and partial blocks do not.
inline constexpr bool coversColumn(bool plant, float spanX, float spanZ) {
    return !plant && spanX > .99f && spanZ > .99f;
}
}
