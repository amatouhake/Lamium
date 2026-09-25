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

// Most monsters spawn only where block light is 0; stored sky light of 7 or
// more keeps them away by day, so such a spot spawns only at night
// (minecraft.wiki, Bedrock; still to be confirmed in game).
enum class SpawnRisk { None, Night, Always };
inline SpawnRisk spawnRisk(LightSurface light) {
    if (light.block > 0) return SpawnRisk::None;
    return light.sky < 7 ? SpawnRisk::Always : SpawnRisk::Night;
}

// The horizontal direction the viewer looks. Floor digits turn so their top
// points that way and they read upright on screen.
enum class Facing { South, West, North, East };
// Minecraft yaw in degrees: 0 looks south, 90 west, 180 north, -90 east.
inline Facing facingFromYaw(float yaw) {
    if (!std::isfinite(yaw)) return Facing::North;
    int quarter = static_cast<int>(std::floor(std::fmod(std::fmod(yaw + 45.f, 360.f) + 360.f, 360.f) / 90.f)) % 4;
    return static_cast<Facing>(quarter);
}
struct Quad { std::array<Point,4> corners; };

// Seven-segment decimal digits in a unit box (u right, v down on screen);
// values 10..15 use two digits rather than a hexadecimal label. Row 0 fills
// the cell; rows -1 and 1 are the upper and lower half, used to show block
// and sky light together.
struct Stroke { double u0, v0, u1, v1; };
inline std::vector<Stroke> lightDigitStrokes(unsigned value, int row = 0) {
    std::vector<Stroke> strokes;
    if (value > 15 || row < -1 || row > 1) return strokes;
    constexpr std::array<unsigned,10> masks{0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x6f};
    constexpr std::array<std::array<double,4>,7> segments{{
        {0,0,1,0}, {1,0,1,.5}, {1,.5,1,1}, {0,1,1,1},
        {0,.5,0,1}, {0,0,0,.5}, {0,.5,1,.5}}};
    double scale = row ? .45 : 1, shift = row * .23;
    auto box = [&](double u, double v) { return std::pair{.5 + (u - .5) * scale, .5 + (v - .5) * scale + shift}; };
    bool two = value >= 10;
    auto digit = [&](unsigned number, double left) {
        double width = two ? .25 : .4;
        for (size_t i = 0; i < segments.size(); ++i) if (masks[number] & (1u << i)) {
            auto const& s = segments[i];
            auto [u0, v0] = box(left + s[0]*width, .2 + s[1]*.6);
            auto [u1, v1] = box(left + s[2]*width, .2 + s[3]*.6);
            strokes.push_back({u0, v0, u1, v1});
        }
    };
    if (two) { digit(value/10,.2); digit(value%10,.55); }
    else digit(value,.3);
    return strokes;
}
// Maps a unit-box position onto the floor of `air`, upright for `facing`.
inline Point floorPoint(Cell air, Facing facing, double u, double v, double lift) {
    static constexpr std::array<std::array<double,2>,4> forward{{{0,1},{-1,0},{0,-1},{1,0}}};
    auto f = forward[static_cast<size_t>(facing)];
    double right[2]{-f[1], f[0]}; // the viewer's right hand
    double du = u - .5, dv = v - .5;
    return {air.x + .5 + du * right[0] - dv * f[0], air.y + lift, air.z + .5 + du * right[1] - dv * f[1]};
}
// Thin digits for renderers that cannot show filled quads.
inline void appendLightNumberLines(std::vector<Line>& lines, Cell air, unsigned value,
                                   Facing facing = Facing::North, int row = 0) {
    for (auto s : lightDigitStrokes(value, row))
        lines.push_back({floorPoint(air, facing, s.u0, s.v0, .025), floorPoint(air, facing, s.u1, s.v1, .025)});
}
inline std::vector<Line> lightNumberLines(Cell air, unsigned value, Facing facing = Facing::North) {
    std::vector<Line> lines;
    appendLightNumberLines(lines, air, value, facing);
    return lines;
}
// Filled digits: each stroke widened into a quad that also covers its ends,
// so segments meet at the corners.
inline void appendLightNumberQuads(std::vector<Quad>& quads, Cell air, unsigned value,
                                   Facing facing = Facing::North, int row = 0) {
    double half = (row ? .45 : 1) * .035;
    for (auto s : lightDigitStrokes(value, row)) {
        double u0 = std::min(s.u0, s.u1) - half, u1 = std::max(s.u0, s.u1) + half;
        double v0 = std::min(s.v0, s.v1) - half, v1 = std::max(s.v0, s.v1) + half;
        quads.push_back({{floorPoint(air, facing, u0, v0, .03), floorPoint(air, facing, u1, v0, .03),
                          floorPoint(air, facing, u1, v1, .03), floorPoint(air, facing, u0, v1, .03)}});
    }
}
// The whole floor of a cell, slightly inset, for the spawn color.
inline Quad lightTintQuad(Cell air) {
    constexpr double in = .02, lift = .015;
    return {{Point{air.x + in, air.y + lift, air.z + in}, Point{air.x + 1 - in, air.y + lift, air.z + in},
             Point{air.x + 1 - in, air.y + lift, air.z + 1 - in}, Point{air.x + in, air.y + lift, air.z + 1 - in}}};
}

// Sampling reads many blocks, so the overlay keeps its markers and samples
// again only when the player moves to another block, the dimension or range
// changes, or `interval` seconds pass (placed torches show up within it).
class LightRefresh {
    struct Key { Cell center; int dimension; int radius; bool operator==(Key const&) const = default; };
    std::optional<Key> last;
    double sampledAt = 0;
public:
    static constexpr double interval = .25;
    bool due(Cell center, int dimension, int radius, double now) {
        Key key{center, dimension, radius};
        if (last == key && std::isfinite(now) && now >= sampledAt && now - sampledAt < interval) return false;
        last = key;
        sampledAt = now;
        return true;
    }
    void clear() { last.reset(); }
};
}
