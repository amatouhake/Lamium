#pragma once
#include "overlay/Geometry.h"
#include <optional>
namespace lamium::overlay {
inline std::vector<Line> chunkBorders(Point position, int minY, int maxY) {
    if (!finite(position) || minY >= maxY || int64_t(maxY)-minY > 65536)
        throw std::invalid_argument("Invalid chunk border bounds");
    double x = std::floor(position.x/16)*16, z = std::floor(position.z/16)*16;
    checkedCoordinate(x); checkedCoordinate(x+16); checkedCoordinate(z); checkedCoordinate(z+16);
    auto box = wireBox({x,double(minY),z}, {x+16,double(maxY),z+16});
    std::vector<Line> result(box.begin(), box.end());
    int64_t y = static_cast<int64_t>(std::floor(double(minY)/16))*16+16;
    for (; y<maxY; y+=16) {
        double h = static_cast<double>(y);
        result.push_back({{x,h,z},{x+16,h,z}});
        result.push_back({{x+16,h,z},{x+16,h,z+16}});
        result.push_back({{x+16,h,z+16},{x,h,z+16}});
        result.push_back({{x,h,z+16},{x,h,z}});
    }
    return result;
}

// Geometry is independent of the world identity. Retain no player, dimension,
// render context, or graphics resources across frames.
class ChunkBorderCache {
    struct Key {
        int x, z, minY, maxY;
        bool operator==(Key const&) const = default;
    };
    std::optional<Key> key;
    std::vector<Line> lines;
public:
    std::vector<Line> const& get(Point position, int minY, int maxY) {
        if (!finite(position) || minY >= maxY || int64_t(maxY)-minY > 65536)
            throw std::invalid_argument("Invalid chunk border bounds");
        double x = std::floor(position.x/16)*16, z = std::floor(position.z/16)*16;
        checkedCoordinate(x); checkedCoordinate(x+16);
        checkedCoordinate(z); checkedCoordinate(z+16);
        Key next{static_cast<int>(x), static_cast<int>(z), minY, maxY};
        if (key != next) {
            // Commit only after generation succeeds; a failed request must not
            // associate old geometry with new bounds.
            auto generated = chunkBorders(position, minY, maxY);
            lines = std::move(generated);
            key = next;
        }
        return lines;
    }
};
}
