#pragma once
#include "features/map/MapImage.h"
#include "features/map/MapTiles.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

namespace lamium::map {
// Minimap radar (BACKLOG L-60 step 4): simple dots by kind with a black ring.
// Pure; Minimap.cpp collects owned positions from the client's actors.
enum class DotKind { Item, Passive, Hostile, Player }; // Drawing order, last on top.
struct RadarSwitches { bool players = true, hostile = true, passive = true, items = false; };
// What a client actor shows as, from its type flags; nothing for the rest
// (projectiles, boats, paintings...).
inline std::optional<DotKind> classify(bool player, bool item, bool monster, bool mob) {
    if (player) return DotKind::Player;
    if (item) return DotKind::Item;
    if (monster) return DotKind::Hostile;
    if (mob) return DotKind::Passive;
    return std::nullopt;
}
inline bool shown(DotKind kind, RadarSwitches const& switches) {
    switch (kind) {
    case DotKind::Player: return switches.players;
    case DotKind::Hostile: return switches.hostile;
    case DotKind::Passive: return switches.passive;
    default: return switches.items;
    }
}
inline constexpr std::uint32_t dotColor(DotKind kind) {
    switch (kind) {
    case DotKind::Player: return packColor(98, 208, 255);
    case DotKind::Hostile: return packColor(255, 79, 63);
    case DotKind::Passive: return packColor(255, 255, 255);
    default: return packColor(242, 201, 76);
    }
}
// Dots keep their size up to 128 blocks across and shrink beyond it, to half
// at 512, so a wide map is not buried under them.
inline double dotScale(double blocks) {
    if (!(blocks > 0)) return 1;
    return std::clamp(std::sqrt(128.0 / blocks), .5, 1.0);
}
// Dots this far above or below the player are drawn fainter.
inline constexpr double faintHeight = 8;
inline float dotAlpha(double dy) { return std::abs(dy) >= faintHeight ? .4f : 1.f; }
// A player out of entity range, at the last position vanilla's locator
// state received (L-89, docs/demos/distant-players.html option 2).
inline constexpr float distantAlpha = .7f;

struct Dot {
    DotKind kind;
    double x, z, dy; // World position; height relative to the player.
    std::string name; // Players only.
    int face = -1;    // A mob face (RadarFaces) drawn instead of the dot.
    bool distant = false; // Players only: not loaded, from the locator state.
    bool riding = false;  // On another actor: drawn over its mount.
};
// A dot placed on the map, in texture pixels.
struct PlacedDot {
    DotKind kind;
    double px, py;
    float alpha;
    std::string name;
    int face = -1;
    bool distant = false;
    bool operator==(PlacedDot const&) const = default;
};
// Dots inside the map (square or round, `margin` pixels in), lowest kind
// first so players end on top. At most `limit`, nearest kept. Within a kind
// riders are drawn over the rest, then by their pixel. A skeleton on a
// spider changed places with it by their distance in the world, by their
// pixel (a pixel apart at times) and by their height, where the rider's
// feet are not above its mount's (seen 2026-11-26 and 27).
inline std::vector<PlacedDot> placeDots(std::vector<Dot> dots, RadarSwitches const& switches, ViewTransform const& view,
                                        double centerX, double centerZ, double blocks, int pixels, bool round,
                                        double margin, size_t limit = 256) {
    std::erase_if(dots, [&](Dot const& d) { return !shown(d.kind, switches); });
    std::sort(dots.begin(), dots.end(), [&](Dot const& a, Dot const& b) {
        return std::hypot(a.x - centerX, a.z - centerZ) < std::hypot(b.x - centerX, b.z - centerZ);
    });
    struct Kept { PlacedDot dot; double from; bool riding; };
    std::vector<Kept> kept;
    for (auto const& d : dots) {
        if (kept.size() >= limit) break;
        auto p = worldToPixel(view, centerX, centerZ, d.x, d.z, blocks, pixels, margin);
        if (!p.inside) continue;
        if (round && std::hypot(p.x - pixels / 2.0, p.y - pixels / 2.0) > pixels / 2.0 - margin) continue;
        // Whole pixels: a mob shuffling within a pixel does not redraw the map.
        // Players never fade with height: for them faint means out of range (L-89).
        float alpha = d.kind != DotKind::Player ? dotAlpha(d.dy) : d.distant ? distantAlpha : 1.f;
        double const px = std::round(p.x), py = std::round(p.y);
        kept.push_back({{d.kind, px, py, alpha, d.kind == DotKind::Player ? d.name : "", d.face, d.distant},
                        std::hypot(px - pixels / 2.0, py - pixels / 2.0), d.riding});
    }
    std::stable_sort(kept.begin(), kept.end(), [](Kept const& a, Kept const& b) {
        return std::tuple(a.dot.kind, a.riding, a.from) < std::tuple(b.dot.kind, b.riding, b.from);
    });
    std::vector<PlacedDot> placed;
    for (auto& k : kept) placed.push_back(std::move(k.dot));
    return placed;
}
// A filled dot with a black ring, smoothed over one pixel.
inline void drawDot(std::vector<std::uint32_t>& pixels, int n, double cx, double cy, double radius, double ring,
                    std::uint32_t color, float alpha) {
    double outer = radius + ring;
    int x0 = std::max(0, int(std::floor(cx - outer - 1))), x1 = std::min(n - 1, int(std::ceil(cx + outer + 1)));
    int y0 = std::max(0, int(std::floor(cy - outer - 1))), y1 = std::min(n - 1, int(std::ceil(cy + outer + 1)));
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x) {
            double d = std::hypot(x + .5 - cx, y + .5 - cy);
            float ringCover = static_cast<float>(std::clamp(outer + .5 - d, 0.0, 1.0));
            if (ringCover <= 0) continue;
            auto& pixel = pixels[static_cast<size_t>(y) * n + x];
            pixel = over(pixel, 0, 0, 0, ringCover * alpha);
            float fill = static_cast<float>(std::clamp(radius + .5 - d, 0.0, 1.0));
            if (fill > 0) pixel = over(pixel, channel(color, 0), channel(color, 1), channel(color, 2), fill * alpha);
        }
}
}
