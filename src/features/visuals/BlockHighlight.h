#pragma once
#include "overlay/Geometry.h"
#include <algorithm>
#include <cmath>
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// Block highlight (BACKLOG L-320). Pure: the designated block types and the
// glow drawn over their exposed faces. The glue (BlockHighlightHooks.cpp)
// records where designated blocks were drawn as chunk sections are built.
namespace lamium::visuals::highlight {
struct Entry {
    std::string name; // the block's type name, e.g. "minecraft:diamond_ore"
    int color = 0;    // a waypoint palette index; saved, unused since the rainbow (2026-11-08)
    bool operator==(Entry const&) const = default;
};
inline constexpr size_t maxEntries = 32;
inline constexpr int paletteSize = 12;

// The color after the last entry's, so neighbours in the list differ.
inline int nextColor(std::vector<Entry> const& entries) {
    if (entries.empty()) return 0;
    return (std::clamp(entries.back().color, 0, paletteSize - 1) + 1) % paletteSize;
}
enum class Change { Added, Removed, Full, Invalid };
// Adds `name` with the next color, or removes it when already listed.
inline Change toggle(std::vector<Entry>& entries, std::string_view name) {
    if (name.empty()) return Change::Invalid;
    auto found = std::find_if(entries.begin(), entries.end(), [&](Entry const& e) { return e.name == name; });
    if (found != entries.end()) { entries.erase(found); return Change::Removed; }
    if (entries.size() >= maxEntries) return Change::Full;
    entries.push_back({std::string(name), nextColor(entries)});
    return Change::Added;
}
// Drops empty and repeated names and clamps colors; keeps the first of each.
inline void normalize(std::vector<Entry>& entries) {
    std::vector<Entry> out;
    for (auto& e : entries) {
        if (e.name.empty() || out.size() >= maxEntries) continue;
        if (std::any_of(out.begin(), out.end(), [&](Entry const& o) { return o.name == e.name; })) continue;
        out.push_back({e.name, std::clamp(e.color, 0, paletteSize - 1)});
    }
    entries = std::move(out);
}

// A designated block seen while a chunk section was built: its cell, the
// faces the game drew (bits in the game's facing order: down, up, north,
// south, west, east). The glow overlay draws these.
struct Mark {
    overlay::Cell cell;
    std::uint8_t faces = 0;
    bool operator==(Mark const&) const = default;
};
// The game's facing bit for each overlay face (overlay::Face order).
inline constexpr std::array<int, 6> facingBit{4, 5, 0, 1, 2, 3};
inline void appendFaces(std::vector<overlay::CellFace>& out, Mark const& mark) {
    for (int f = 0; f < 6; ++f)
        if (mark.faces >> facingBit[static_cast<size_t>(f)] & 1) out.push_back({mark.cell, static_cast<overlay::Face>(f)});
}
// The glow's strength over time, 0..1: a smooth pulse so the marks catch the
// eye without blinking hard.
inline constexpr double pulsePeriod = 1.6;
inline float pulse(double seconds) {
    double phase = std::fmod(seconds, pulsePeriod) / pulsePeriod;
    if (phase < 0) phase += 1;
    return static_cast<float>(.5 - .5 * std::cos(phase * 6.283185307179586));
}
// The glow's color: a rainbow flowing across the world. The hue moves with
// time and with the cell's position, so neighbouring blocks differ a little
// and the colors run through a vein (decided 2026-11-08: one look for every
// type instead of per-type colors).
inline constexpr double rainbowPeriod = 3.0;   // seconds for one turn of the hue
inline constexpr double rainbowSpread = .035;  // hue turns per block along x + y + z
inline float rainbowHue(double seconds, overlay::Cell cell) {
    double hue = seconds / rainbowPeriod - (double(cell.x) + cell.y + cell.z) * rainbowSpread;
    hue -= std::floor(hue);
    return static_cast<float>(hue);
}
// A saturated, bright color for `hue` in 0..1 (red, yellow, green, cyan,
// blue, magenta and back).
inline std::array<float, 3> hueColor(float hue, float saturation = .8f) {
    hue = hue - std::floor(hue);
    float h = hue * 6, x = 1 - std::abs(std::fmod(h, 2.f) - 1);
    std::array<float, 3> c{};
    switch (static_cast<int>(h) % 6) {
    case 0: c = {1, x, 0}; break;
    case 1: c = {x, 1, 0}; break;
    case 2: c = {0, 1, x}; break;
    case 3: c = {0, x, 1}; break;
    case 4: c = {x, 0, 1}; break;
    default: c = {1, 0, x}; break;
    }
    for (auto& v : c) v = 1 - saturation + saturation * v;
    return c;
}
// At most this many faces are drawn a frame, so a list of common blocks
// cannot flood the overlay.
inline constexpr size_t maxGlowFaces = 24576;
}
