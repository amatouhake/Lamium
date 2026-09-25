#include "overlay/LightOverlay.h"
#include <algorithm>
#include <limits>
#include <stdexcept>

void lightOverlayTests() {
    using namespace lamium::overlay;
    auto check = [](bool pass, char const* message) { if (!pass) throw std::runtime_error(message); };
    int calls = 0;
    auto markers = sampleLightSurfaces({-16,64,-1},1,2,[&](Cell cell) -> std::optional<LightSurface> {
        ++calls;
        if (cell.x == -17) return {}; // Unavailable must not become zero light.
        if (cell.y == 62 || cell.y == 65) return LightSurface{0,15};
        return {};
    });
    check(calls == 45 && markers.size() == 12, "bounded scan includes both loaded floors at negative coordinates");
    for (auto marker : markers)
        check(marker.air.x != -17 && marker.light.sky == 15, "unknown cells must not create dark markers");
    check(sampleLightSurfaces({0,0,0},0,0,[](Cell) { return std::optional{LightSurface{16,0}}; }).empty(),
        "out-of-range light must not be drawn");
    bool rejected = false;
    try { sampleLightSurfaces({std::numeric_limits<int>::max(),0,0},1,0,
        [](Cell) -> std::optional<LightSurface> { throw std::runtime_error("provider must not run"); }); }
    catch (std::out_of_range const&) { rejected = true; }
    check(rejected, "reject overflowing bounds before sampling");
    for (unsigned level = 0; level <= 15; ++level) {
        auto lines = lightNumberLines({-1,-64,-16},level);
        check(!lines.empty() && lines.size() <= 14, "each light level has bounded decimal geometry");
        for (auto line : lines) for (auto p : {line.from,line.to})
            check(finite(p) && p.x > -1 && p.x < 0 && p.z > -16 && p.z < -15
                && p.y > -64 && p.y < -63.9, "digits stay just above their own floor");
    }
    check(lightNumberLines({0,0,0},0).size() == 6, "zero is a visible closed digit");
    check(lightNumberLines({0,0,0},15).size() == 7, "fifteen is decimal one plus five");
    check(lightNumberLines({0,0,0},16).empty(), "invalid brightness has no numeric marker");
    std::vector<Line> batch{{{1,2,3},{4,5,6}}};
    batch.reserve(1+16*14);
    auto const* storage = batch.data();
    size_t expected = 1;
    for (unsigned level = 0; level <= 15; ++level) {
        auto single = lightNumberLines({-1,-64,-16},level);
        appendLightNumberLines(batch,{-1,-64,-16},level);
        check(batch.size() == expected+single.size(), "append preserves earlier markers");
        for (size_t i = 0; i < single.size(); ++i)
            check(batch[expected+i].from == single[i].from && batch[expected+i].to == single[i].to,
                "batch geometry matches individual digits");
        expected = batch.size();
    }
    appendLightNumberLines(batch,{0,0,0},16);
    check(batch.size() == expected && batch.front().from == Point{1,2,3}
        && batch.front().to == Point{4,5,6}, "invalid value leaves existing batch intact");
    check(batch.data() == storage, "reserved batch needs no per-marker growth");
    check(spawnRisk({0,3}) == SpawnRisk::Always && spawnRisk({0,6}) == SpawnRisk::Always
        && spawnRisk({0,7}) == SpawnRisk::Night && spawnRisk({0,15}) == SpawnRisk::Night
        && spawnRisk({1,0}) == SpawnRisk::None && spawnRisk({14,15}) == SpawnRisk::None,
        "block light keeps spawns away; low sky light spawns at any time, open sky only at night");
    check(facingFromYaw(0) == Facing::South && facingFromYaw(44) == Facing::South && facingFromYaw(46) == Facing::West
        && facingFromYaw(180) == Facing::North && facingFromYaw(-180) == Facing::North && facingFromYaw(-90) == Facing::East
        && facingFromYaw(-44) == Facing::South && facingFromYaw(720) == Facing::South
        && facingFromYaw(std::numeric_limits<float>::quiet_NaN()) == Facing::North, "yaw maps to the nearest quarter");
    // The top of a digit points where the viewer looks; its right side to the viewer's right.
    std::array<std::pair<Facing, std::array<double,4>>, 4> frames{{
        {Facing::South, {0,1,-1,0}}, {Facing::West, {-1,0,0,-1}}, {Facing::North, {0,-1,1,0}}, {Facing::East, {1,0,0,1}}}};
    for (auto [facing, expect] : frames) {
        Cell air{-3,70,5};
        Point top = floorPoint(air, facing, .5, 0, 0), right = floorPoint(air, facing, 1, .5, 0);
        check(std::abs(top.x - (air.x+.5) - expect[0]*.5) < 1e-9 && std::abs(top.z - (air.z+.5) - expect[1]*.5) < 1e-9
            && std::abs(right.x - (air.x+.5) - expect[2]*.5) < 1e-9 && std::abs(right.z - (air.z+.5) - expect[3]*.5) < 1e-9,
            "floor digits turn toward the viewing direction");
        for (unsigned level = 0; level <= 15; ++level)
            for (int row : {-1, 0, 1}) {
                std::vector<Quad> quads;
                appendLightNumberQuads(quads, air, level, facing, row);
                check(!quads.empty(), "every level has filled digits in every row");
                for (auto const& quad : quads) for (auto p : quad.corners)
                    check(finite(p) && p.x > air.x && p.x < air.x+1 && p.z > air.z && p.z < air.z+1
                        && p.y > air.y && p.y < air.y+.1, "filled digits stay on their own floor");
            }
    }
    check(lightNumberLines({0,0,0},8,Facing::East).size() == 7, "turned digits keep their segments");
    std::vector<Quad> none;
    appendLightNumberQuads(none,{0,0,0},16);
    appendLightNumberQuads(none,{0,0,0},5,Facing::North,2);
    check(none.empty(), "invalid values and rows draw nothing");
    auto upper = lightDigitStrokes(8,-1), lower = lightDigitStrokes(8,1);
    check(std::all_of(upper.begin(), upper.end(), [](Stroke s) { return s.v0 < .5 && s.v1 < .5; })
        && std::all_of(lower.begin(), lower.end(), [](Stroke s) { return s.v0 > .5 && s.v1 > .5; }),
        "block and sky rows do not overlap");
    auto tint = lightTintQuad({2,64,-7});
    check(tint.corners[0].x > 2 && tint.corners[2].x < 3 && tint.corners[0].z > -7 && tint.corners[2].z < -6
        && tint.corners[0].y > 64 && tint.corners[0].y < tint.corners[0].y + 1, "the spawn tint covers its own floor");
    LightRefresh refresh;
    check(refresh.due({0,64,0},0,8,10), "the first frame samples");
    check(!refresh.due({0,64,0},0,8,10.1), "standing still reuses samples");
    check(refresh.due({0,64,0},0,8,10.3), "samples refresh after the interval");
    check(refresh.due({1,64,0},0,8,10.31) && refresh.due({1,64,0},1,8,10.32) && refresh.due({1,64,0},1,12,10.33),
        "moving, changing dimension or range samples at once");
    check(refresh.due({1,64,0},1,12,5), "a clock going backwards samples again");
    refresh.clear();
    check(refresh.due({1,64,0},1,12,5.01), "clearing forces a sample");
}
