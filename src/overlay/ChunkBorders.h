#pragma once
#include "overlay/Geometry.h"
#include <optional>
namespace lamium::overlay {
// Java F3+G-like chunk borders (BACKLOG L-10): yellow 2-block grid on the
// current chunk walls, blue 16-block section lines and current corners, red
// neighbor-chunk corners. Counts and positions are tested; the exact Java
// look still needs a maintainer screenshot comparison in game.
inline constexpr std::array<float, 3> chunkYellow{1, 1, 0};
inline constexpr std::array<float, 3> chunkBlue{0, 0, 1};
inline constexpr std::array<float, 3> chunkRed{1, 0, 0};
struct ChunkBorderGroups { std::vector<Line> yellow, blue, red; };
inline int64_t floorMod16(int64_t value) {
    int64_t mod = value % 16;
    return mod < 0 ? mod + 16 : mod;
}
inline ChunkBorderGroups chunkBorderGroups(Point position, int minY, int maxY) {
    if (!finite(position) || minY >= maxY || int64_t(maxY) - minY > 65536)
        throw std::invalid_argument("Invalid chunk border bounds");
    double x0 = std::floor(position.x / 16) * 16, z0 = std::floor(position.z / 16) * 16;
    checkedCoordinate(x0);
    checkedCoordinate(x0 + 16);
    checkedCoordinate(z0);
    checkedCoordinate(z0 + 16);
    ChunkBorderGroups groups;
    double low = minY, high = maxY;
    auto vertical = [&](double x, double z, std::vector<Line>& out) {
        out.push_back({{x, low, z}, {x, high, z}});
    };
    auto ring = [&](double height, std::vector<Line>& out) {
        out.push_back({{x0, height, z0}, {x0 + 16, height, z0}});
        out.push_back({{x0 + 16, height, z0}, {x0 + 16, height, z0 + 16}});
        out.push_back({{x0 + 16, height, z0 + 16}, {x0, height, z0 + 16}});
        out.push_back({{x0, height, z0 + 16}, {x0, height, z0}});
    };
    // Yellow grid every 2 blocks on the four walls; exact corners are blue.
    for (int step = 2; step < 16; step += 2) {
        vertical(x0 + step, z0, groups.yellow);
        vertical(x0 + step, z0 + 16, groups.yellow);
        vertical(x0, z0 + step, groups.yellow);
        vertical(x0 + 16, z0 + step, groups.yellow);
    }
    for (int64_t y = minY; y <= maxY; y += 2) {
        if (floorMod16(y) != 0) ring(static_cast<double>(y), groups.yellow);
    }
    // Blue 16-block section lines and the current chunk's corners.
    for (int64_t y = minY + (16 - floorMod16(minY)) % 16; y <= maxY; y += 16)
        ring(static_cast<double>(y), groups.blue);
    vertical(x0, z0, groups.blue);
    vertical(x0 + 16, z0, groups.blue);
    vertical(x0, z0 + 16, groups.blue);
    vertical(x0 + 16, z0 + 16, groups.blue);
    // Red verticals at the neighboring chunks' corners (shared points once).
    for (int i = -1; i <= 2; ++i)
        for (int j = -1; j <= 2; ++j) {
            if (i >= 0 && i <= 1 && j >= 0 && j <= 1) continue;
            vertical(x0 + i * 16, z0 + j * 16, groups.red);
        }
    return groups;
}

// Geometry is independent of the world identity. Retain no player, dimension,
// render context, or graphics resources across frames.
class ChunkBorderCache {
    struct Key {
        int x, z, minY, maxY;
        bool operator==(Key const&) const = default;
    };
    std::optional<Key> key;
    ChunkBorderGroups groups;
public:
    ChunkBorderGroups const& get(Point position, int minY, int maxY) {
        if (!finite(position) || minY >= maxY || int64_t(maxY) - minY > 65536)
            throw std::invalid_argument("Invalid chunk border bounds");
        double x = std::floor(position.x / 16) * 16, z = std::floor(position.z / 16) * 16;
        checkedCoordinate(x);
        checkedCoordinate(x + 16);
        checkedCoordinate(z);
        checkedCoordinate(z + 16);
        Key next{static_cast<int>(x), static_cast<int>(z), minY, maxY};
        if (key != next) {
            // Commit only after generation succeeds; a failed request must not
            // associate old geometry with new bounds.
            auto generated = chunkBorderGroups(position, minY, maxY);
            groups = std::move(generated);
            key = next;
        }
        return groups;
    }
};
}
