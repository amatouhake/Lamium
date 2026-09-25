#pragma once
#include "overlay/Geometry.h"
#include <map>
#include <optional>

namespace lamium::overlay {
// Stored client light, not a prediction of spawning or effective night light.
struct LightSurface {
    unsigned block = 0;
    unsigned sky = 0;
    bool operator==(LightSurface const&) const = default;
};
struct LightMarker {
    Cell air;
    LightSurface light;
    bool operator==(LightMarker const&) const = default;
};

// The provider returns a value only for a loaded air cell above a supported
// surface. It must not load chunks. Enumerate every qualifying floor in range,
// including stacked floors; do not silently select just the topmost surface.
// Every qualifying cell in an inclusive box, at most one chunk column of the
// tallest dimension, so a caller cannot ask for an unbounded scan.
template <class Sample>
std::vector<LightMarker> sampleLightBox(Cell low, Cell high, Sample&& sample) {
    if (low.x > high.x || low.y > high.y || low.z > high.z
        || double(high.x) - low.x > 128 || double(high.z) - low.z > 128 || double(high.y) - low.y > 512)
        throw std::invalid_argument("Light overlay range out of bounds");
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
template <class Sample>
std::vector<LightMarker> sampleLightSurfaces(Cell center, int radius, int verticalRadius, Sample&& sample) {
    if (radius < 0 || radius > 64 || verticalRadius < 0 || verticalRadius > 64)
        throw std::invalid_argument("Light overlay range out of bounds");
    Cell low{checkedCoordinate(double(center.x)-radius), checkedCoordinate(double(center.y)-verticalRadius),
             checkedCoordinate(double(center.z)-radius)};
    Cell high{checkedCoordinate(double(center.x)+radius), checkedCoordinate(double(center.y)+verticalRadius),
              checkedCoordinate(double(center.z)+radius)};
    return sampleLightBox(low, high, std::forward<Sample>(sample));
}

// Large ranges are read a few chunk columns per frame and kept per chunk
// (the way Shapes keep meshes), so radius 64 does not stall a frame.
struct ChunkColumn {
    int x = 0, z = 0;
    auto operator<=>(ChunkColumn const&) const = default;
};
inline int floorDiv16(int value) { return value >= 0 ? value / 16 : -((-value + 15) / 16); }
inline ChunkColumn chunkOf(Cell cell) { return {floorDiv16(cell.x), floorDiv16(cell.z)}; }
// Columns that touch the square of `radius` blocks around `center`, nearest
// first; each is shown whole so moving inside a chunk re-reads nothing.
inline std::vector<ChunkColumn> chunksInRange(Cell center, int radius) {
    radius = std::clamp(radius, 0, 64);
    auto low = chunkOf({center.x - radius, 0, center.z - radius}), high = chunkOf({center.x + radius, 0, center.z + radius});
    auto home = chunkOf(center);
    std::vector<ChunkColumn> columns;
    for (int x = low.x; x <= high.x; ++x)
        for (int z = low.z; z <= high.z; ++z) columns.push_back({x, z});
    auto distance = [&](ChunkColumn c) { return std::max(std::abs(c.x - home.x), std::abs(c.z - home.z)); };
    std::stable_sort(columns.begin(), columns.end(), [&](ChunkColumn a, ChunkColumn b) { return distance(a) < distance(b); });
    return columns;
}
// Picks the columns to read this frame: unread ones first, then those older
// than their refresh interval (fast next to the viewer, slower further out),
// nearest first and at most `budget` columns.
class LightSchedule {
    std::map<ChunkColumn, double> readAt;
public:
    static constexpr double nearInterval = .25, farInterval = 2;
    std::vector<ChunkColumn> pick(std::vector<ChunkColumn> const& wanted, ChunkColumn home, double now, size_t budget) {
        std::vector<ChunkColumn> chosen;
        for (int pass = 0; pass < 2 && chosen.size() < budget; ++pass)
            for (auto column : wanted) {
                if (chosen.size() >= budget) break;
                auto found = readAt.find(column);
                bool unread = found == readAt.end();
                if (pass == 0 ? !unread : unread) continue;
                if (!unread) {
                    bool near = std::max(std::abs(column.x - home.x), std::abs(column.z - home.z)) <= 1;
                    double age = now - found->second;
                    if (age >= 0 && age < (near ? nearInterval : farInterval)) continue;
                }
                readAt[column] = now;
                chosen.push_back(column);
            }
        return chosen;
    }
    // Columns no longer shown are read afresh if they come back.
    void keepOnly(std::vector<ChunkColumn> const& wanted) {
        std::erase_if(readAt, [&](auto const& entry) {
            return std::find(wanted.begin(), wanted.end(), entry.first) == wanted.end();
        });
    }
    void clear() { readAt.clear(); }
};

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
// The same from a horizontal view direction (the detached camera's).
inline Facing facingFromDirection(double dx, double dz) {
    if (!std::isfinite(dx) || !std::isfinite(dz) || (dx == 0 && dz == 0)) return Facing::North;
    if (std::abs(dz) >= std::abs(dx)) return dz > 0 ? Facing::South : Facing::North;
    return dx > 0 ? Facing::East : Facing::West;
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
        lines.push_back({floorPoint(air, facing, s.u0, s.v0, .075), floorPoint(air, facing, s.u1, s.v1, .075)});
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
        quads.push_back({{floorPoint(air, facing, u0, v0, .07), floorPoint(air, facing, u1, v0, .07),
                          floorPoint(air, facing, u1, v1, .07), floorPoint(air, facing, u0, v1, .07)}});
    }
}
// The whole floor of a cell, slightly inset, for the spawn color. Tint and
// digits sit clearly above the floor and apart from each other: closer
// layers flickered against the ground in Simple and Vibrant Visuals.
inline Quad lightTintQuad(Cell air) {
    constexpr double in = .02, lift = .04;
    return {{Point{air.x + in, air.y + lift, air.z + in}, Point{air.x + 1 - in, air.y + lift, air.z + in},
             Point{air.x + 1 - in, air.y + lift, air.z + 1 - in}, Point{air.x + in, air.y + lift, air.z + 1 - in}}};
}

}
