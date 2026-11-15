#pragma once
#include "features/map/MapColors.h"
#include "features/map/MapRegion.h"
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

// A small picture of a saved map (BACKLOG L-343): the Saved data page shows
// one beside a data's waypoint names, so a person can tell whose data it is.
// Pure: the caller reads the region files and uploads the pixels.
namespace lamium::map::thumbnail {
// Three regions across, eight blocks to a pixel: 768 blocks in 96 pixels.
inline constexpr int regionsAcross = 3, blocksPerPixel = 8;
inline constexpr int regionPixels = regionBlocks / blocksPerPixel, side = regionsAcross * regionPixels;
// The regions drawn: `center` and the ring around it.
struct Area {
    RegionKey center;
    int originX() const { return (center.x - regionsAcross / 2) * regionBlocks; }
    int originZ() const { return (center.z - regionsAcross / 2) * regionBlocks; }
    // The region at (column, row) of the picture.
    RegionKey region(int column, int row) const { return {center.x - regionsAcross / 2 + column, center.z - regionsAcross / 2 + row}; }
    // Where a block lies in the picture, 0-1 both ways; none outside it.
    std::optional<std::pair<float, float>> at(int x, int z) const {
        int const dx = x - originX(), dz = z - originZ(), blocks = regionsAcross * regionBlocks;
        if (dx < 0 || dz < 0 || dx >= blocks || dz >= blocks) return std::nullopt;
        return std::pair{(dx + .5f) / blocks, (dz + .5f) / blocks};
    }
};
inline std::vector<std::uint32_t> blank() { return std::vector<std::uint32_t>(static_cast<size_t>(side) * side, 0); }
// One region's shaded pixels (shadeRegion: one per block) into its part of the picture: each pixel is the
// average of the recorded blocks it covers, clear where none is recorded.
inline void blit(std::vector<std::uint32_t>& image, std::vector<std::uint32_t> const& shaded, int column, int row) {
    if (image.size() != static_cast<size_t>(side) * side || shaded.size() != static_cast<size_t>(regionColumns)
        || column < 0 || row < 0 || column >= regionsAcross || row >= regionsAcross) return;
    for (int pz = 0; pz < regionPixels; ++pz)
        for (int px = 0; px < regionPixels; ++px) {
            int sum[3]{}, count = 0;
            for (int bz = 0; bz < blocksPerPixel; ++bz)
                for (int bx = 0; bx < blocksPerPixel; ++bx) {
                    auto color = shaded[static_cast<size_t>((pz * blocksPerPixel + bz) * regionBlocks + px * blocksPerPixel + bx)];
                    if (!(color >> 24)) continue;
                    for (int c = 0; c < 3; ++c) sum[c] += channel(color, c);
                    ++count;
                }
            image[static_cast<size_t>((row * regionPixels + pz) * side + column * regionPixels + px)] =
                count ? packColor(sum[0] / count, sum[1] / count, sum[2] / count) : 0;
        }
}
}
