#pragma once
#include "features/map/MapView.h"
#include <cmath>
#include <cstdint>
#include <vector>

// Slime chunks on the minimap (BACKLOG L-295): which texture pixels lie in a
// slime chunk, per square map of `pixels` across, as the terrain is
// composed. Built when the terrain is; the overlay only blends it.
namespace lamium::map {
inline constexpr std::uint8_t slimeInside = 1, slimeBorder = 2;
// 0 outside, slimeInside, or slimeBorder for an inside pixel next to one that
// is not (the area's outline; the map's own edge is not a border).
template <class IsSlime>
std::vector<std::uint8_t> slimeMask(ViewTransform const& view, double centerX, double centerZ, double blocks, int pixels,
                                    IsSlime&& isSlime) {
    std::vector<std::uint8_t> mask(static_cast<size_t>(pixels) * static_cast<size_t>(pixels), 0);
    double scale = pixels / std::max(1.0, blocks);
    // Neighboring pixels mostly share a chunk.
    long long lastX = 0, lastZ = 0;
    bool known = false, last = false;
    for (int y = 0; y < pixels; ++y)
        for (int x = 0; x < pixels; ++x) {
            auto w = view.toWorld((x + .5 - pixels / 2.0) / scale, (y + .5 - pixels / 2.0) / scale);
            auto cx = static_cast<long long>(std::floor((centerX + w.x) / 16)), cz = static_cast<long long>(std::floor((centerZ + w.z) / 16));
            if (!known || cx != lastX || cz != lastZ) {
                last = isSlime(static_cast<int>(cx), static_cast<int>(cz));
                lastX = cx, lastZ = cz, known = true;
            }
            if (last) mask[static_cast<size_t>(y * pixels + x)] = slimeInside;
        }
    auto at = [&](int x, int y) { return mask[static_cast<size_t>(y * pixels + x)]; };
    auto outside = [&](int x, int y) { return x >= 0 && y >= 0 && x < pixels && y < pixels && at(x, y) == 0; };
    std::vector<std::uint8_t> result = mask;
    for (int y = 0; y < pixels; ++y)
        for (int x = 0; x < pixels; ++x)
            if (at(x, y) && (outside(x - 1, y) || outside(x + 1, y) || outside(x, y - 1) || outside(x, y + 1)))
                result[static_cast<size_t>(y * pixels + x)] = slimeBorder;
    return result;
}
// `color` over a packed RGBA pixel at `alpha`; transparent pixels (outside
// a round map) stay as they are.
inline std::uint32_t washPixel(std::uint32_t pixel, std::uint32_t color, float alpha) {
    if ((pixel >> 24) == 0) return pixel;
    auto mix = [&](int shift) {
        float a = static_cast<float>((pixel >> shift) & 255), b = static_cast<float>((color >> shift) & 255);
        return static_cast<std::uint32_t>(std::lround(a + (b - a) * alpha)) << shift;
    };
    return mix(0) | mix(8) | mix(16) | (pixel & 0xff000000u);
}
}
