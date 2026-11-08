#pragma once
#include "overlay/Geometry.h"
#include <algorithm>
#include <cmath>
#include <array>
#include <bit>
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

// The list screen (step 2). A type's name without the vanilla namespace,
// as shown beside its display name.
inline std::string_view shortName(std::string_view name) {
    return name.starts_with("minecraft:") ? name.substr(10) : name;
}
// The deepslate form of a plain type or the plain form of a deepslate one
// ("minecraft:diamond_ore" <-> "minecraft:deepslate_diamond_ore"); the
// caller checks that the name exists. Empty for a name without a part
// after its namespace.
inline std::string siblingName(std::string_view name) {
    auto colon = name.find(':');
    std::string_view space = colon == std::string_view::npos ? std::string_view{} : name.substr(0, colon + 1);
    std::string_view rest = colon == std::string_view::npos ? name : name.substr(colon + 1);
    if (rest.empty()) return {};
    constexpr std::string_view prefix = "deepslate_";
    if (rest.starts_with(prefix)) return rest.size() > prefix.size() ? std::string(space) + std::string(rest.substr(prefix.size())) : std::string{};
    return std::string(space) + std::string(prefix) + std::string(rest);
}
// Every space-separated word of `query` appears in the display name or in
// the name without "minecraft:" (ASCII case ignored), so "diamond_ore",
// "diamond ore" and "ダイヤ" all find diamond ore. An empty query matches
// nothing: the screen shows its suggestions instead.
inline bool queryMatches(std::string_view query, std::string_view name, std::string_view display) {
    auto fold = [](std::string_view s) {
        std::string out(s);
        for (auto& c : out) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
        return out;
    };
    auto q = fold(query), n = fold(shortName(name)), d = fold(display);
    bool any = false;
    for (size_t begin = 0; begin < q.size();) {
        begin = q.find_first_not_of(' ', begin);
        if (begin == std::string::npos) break;
        auto end = std::min(q.find(' ', begin), q.size());
        auto word = std::string_view(q).substr(begin, end - begin);
        if (n.find(word) == std::string::npos && d.find(word) == std::string::npos) return false;
        any = true;
        begin = end;
    }
    return any;
}
// What the screen offers before anything is typed: blocks people usually
// look for. Names missing from the game are skipped by the caller.
inline constexpr std::array<std::string_view, 16> suggestedNames{
    "minecraft:diamond_ore", "minecraft:deepslate_diamond_ore", "minecraft:ancient_debris", "minecraft:emerald_ore",
    "minecraft:deepslate_emerald_ore", "minecraft:gold_ore", "minecraft:deepslate_gold_ore", "minecraft:iron_ore",
    "minecraft:deepslate_iron_ore", "minecraft:mob_spawner", "minecraft:trial_spawner", "minecraft:vault",
    "minecraft:budding_amethyst", "minecraft:suspicious_sand", "minecraft:suspicious_gravel", "minecraft:end_portal_frame"};

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
// At most this many faces are drawn a frame, the nearest first, so a list
// of common blocks (tuff, gravel) cannot flood the overlay; each draw holds
// at most `glowBatchFaces` so a mesh stays far below 65536 vertices.
inline constexpr size_t maxGlowFaces = 16384;
inline constexpr size_t glowBatchFaces = 4096;
inline int faceCount(Mark const& mark) { return std::popcount(static_cast<unsigned>(mark.faces & 0x3f)); }
// The marks to draw: all when their faces fit `limit`, else the nearest to
// `eye` until the faces reach it. Reorders `marks`; returns how many to draw
// from the front and whether some were left out.
struct Chosen { size_t count; bool capped; };
inline Chosen chooseNearest(std::vector<Mark>& marks, overlay::Cell eye, size_t limit) {
    size_t total = 0;
    for (auto const& m : marks) total += static_cast<size_t>(faceCount(m));
    if (total <= limit) return {marks.size(), false};
    auto distance = [&](Mark const& m) {
        double dx = m.cell.x - eye.x, dy = m.cell.y - eye.y, dz = m.cell.z - eye.z;
        return dx * dx + dy * dy + dz * dz;
    };
    std::sort(marks.begin(), marks.end(), [&](Mark const& a, Mark const& b) { return distance(a) < distance(b); });
    size_t faces = 0, count = 0;
    while (count < marks.size() && faces + static_cast<size_t>(faceCount(marks[count])) <= limit)
        faces += static_cast<size_t>(faceCount(marks[count++]));
    return {count, true};
}
}
