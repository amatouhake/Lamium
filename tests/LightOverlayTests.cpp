#include "overlay/LightOverlay.h"
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
}
