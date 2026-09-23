#pragma once
#include "overlay/Geometry.h"
#include <optional>

namespace lamium::overlay {
// Stored client light, not a prediction of spawning or effective night light.
struct LightSurface {
    unsigned block = 0;
    unsigned sky = 0;
};
struct LightMarker {
    Cell air;
    LightSurface light;
};

// The provider returns a value only for a loaded air cell above a supported
// surface. It must not load chunks. Enumerate every qualifying floor in range,
// including stacked floors; do not silently select just the topmost surface.
template <class Sample>
std::vector<LightMarker> sampleLightSurfaces(Cell center, int radius, int verticalRadius, Sample&& sample) {
    if (radius < 0 || radius > 16 || verticalRadius < 0 || verticalRadius > 8)
        throw std::invalid_argument("Light overlay range out of bounds");
    Cell low{checkedCoordinate(double(center.x)-radius), checkedCoordinate(double(center.y)-verticalRadius),
             checkedCoordinate(double(center.z)-radius)};
    Cell high{checkedCoordinate(double(center.x)+radius), checkedCoordinate(double(center.y)+verticalRadius),
              checkedCoordinate(double(center.z)+radius)};
    std::vector<LightMarker> result;
    for (int x = low.x; x <= high.x; ++x)
        for (int z = low.z; z <= high.z; ++z)
            for (int y = low.y; y <= high.y; ++y) {
                Cell cell{x,y,z};
                auto light = sample(cell);
                if (light && light->block <= 15 && light->sky <= 15) result.push_back({cell,*light});
            }
    return result;
}

// Seven-segment decimal digits laid flat within the sampled cell. North is the
// top of a digit. Values 10..15 use two digits rather than a hexadecimal label.
inline void appendLightNumberLines(std::vector<Line>& lines, Cell air, unsigned value) {
    if (value > 15) return;
    constexpr std::array<unsigned,10> masks{0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x6f};
    constexpr std::array<std::array<double,4>,7> segments{{
        {0,0,1,0}, {1,0,1,.5}, {1,.5,1,1}, {0,1,1,1},
        {0,.5,0,1}, {0,0,0,.5}, {0,.5,1,.5}}};
    bool two = value >= 10;
    auto digit = [&](unsigned number, double left) {
        double width = two ? .25 : .4;
        for (size_t i = 0; i < segments.size(); ++i) if (masks[number] & (1u << i)) {
            auto const& s = segments[i];
            lines.push_back({{air.x+left+s[0]*width,air.y+.025,air.z+.2+s[1]*.6},
                             {air.x+left+s[2]*width,air.y+.025,air.z+.2+s[3]*.6}});
        }
    };
    if (two) { digit(value/10,.2); digit(value%10,.55); }
    else digit(value,.3);
}

inline std::vector<Line> lightNumberLines(Cell air, unsigned value) {
    std::vector<Line> lines;
    appendLightNumberLines(lines, air, value);
    return lines;
}
}
