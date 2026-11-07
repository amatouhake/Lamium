#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// Block highlight (BACKLOG L-320). Pure: the designated block types, their
// colors, and how a highlighted vertex color is made. The tessellator glue
// (BlockHighlightHooks.cpp) tints designated blocks in the terrain mesh.
namespace lamium::visuals::highlight {
struct Entry {
    std::string name; // the block's type name, e.g. "minecraft:diamond_ore"
    int color = 0;    // index into the waypoint palette (map::waypointColors)
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

// Vertex colors in the chunk mesh are packed as 0xAABBGGRR. The vanilla color
// carries ambient occlusion and biome tint; the highlight keeps a share of it
// so the block's shading still reads, and mixes in the type's color.
inline constexpr float tintShare = .55f;
inline std::uint32_t tintVertex(std::uint32_t vertex, std::uint8_t r, std::uint8_t g, std::uint8_t b, float share = tintShare) {
    share = std::clamp(share, 0.f, 1.f);
    auto mix = [&](unsigned shift, std::uint8_t target) {
        float from = static_cast<float>((vertex >> shift) & 0xff);
        float to = from * (1 - share) + static_cast<float>(target) * share;
        return static_cast<std::uint32_t>(std::clamp(to + .5f, 0.f, 255.f)) << shift;
    };
    return (vertex & 0xff000000u) | mix(0, r) | mix(8, g) | mix(16, b);
}
}
