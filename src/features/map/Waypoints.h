#pragma once
#include "features/map/MapImage.h"
#include "features/map/MapTiles.h"
#include "features/map/MapView.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lamium::map {
// Waypoints (BACKLOG L-60 step 5, docs/demos/waypoints.html). Pure: the data,
// where markers sit on the minimap, the death point and storage names.

// Twelve colors, in the order the add prompt offers them.
inline constexpr std::array<std::uint32_t, 12> waypointColors{
    packColor(224, 74, 60), packColor(240, 154, 62), packColor(242, 204, 61), packColor(127, 214, 90),
    packColor(63, 191, 127), packColor(63, 208, 224), packColor(74, 141, 240), packColor(122, 108, 240),
    packColor(183, 124, 240), packColor(239, 127, 192), packColor(242, 242, 242), packColor(138, 143, 148)};
inline constexpr int clampColor(int index) { return std::clamp(index, 0, static_cast<int>(waypointColors.size()) - 1); }
// A new waypoint takes the color after the last one used, so neighbors differ.
inline constexpr int nextColor(int last) {
    return last < 0 ? 0 : (clampColor(last) + 1) % static_cast<int>(waypointColors.size());
}

struct Waypoint {
    std::string name;
    int color = 0;
    int x = 0, y = 0, z = 0;
    int dimension = 0; // 0 Overworld, 1 Nether, 2 End.
    bool visible = true;
    std::uint64_t id = 0; // session id (app/SessionIds.h, L-139); not saved
    bool operator==(Waypoint const&) const = default;
};
struct DeathPoint {
    int x = 0, y = 0, z = 0, dimension = 0;
    bool operator==(DeathPoint const&) const = default;
};
struct WaypointSet {
    std::vector<Waypoint> waypoints;
    std::optional<DeathPoint> death;
    int lastColor = -1; // The next waypoint starts at color 0.
    bool operator==(WaypointSet const&) const = default;
};
inline constexpr size_t maxWaypoints = 1000;
inline constexpr size_t maxNameBytes = 128;
// The name a new waypoint gets: the lowest "N" not used yet.
template<class Name>
std::string defaultWaypointName(std::vector<Waypoint> const& existing, Name name) {
    for (int n = 1;; ++n) {
        auto candidate = name(n);
        if (std::none_of(existing.begin(), existing.end(), [&](Waypoint const& w) { return w.name == candidate; }))
            return candidate;
    }
}

// Where a waypoint shows from the current dimension: its own, or with the
// cross-dimension option, Overworld ones in the Nether at 1/8 and Nether ones
// in the Overworld at 8x. Heights are kept.
struct Shown { double x, y, z; bool scaled; };
inline std::optional<Shown> shownPosition(int x, int y, int z, int dimension, int current, bool crossScale) {
    if (dimension == current) return Shown{x + .5, double(y), z + .5, false};
    if (!crossScale) return std::nullopt;
    if (dimension == 0 && current == 1) return Shown{x / 8.0 + .5, double(y), z / 8.0 + .5, true};
    if (dimension == 1 && current == 0) return Shown{x * 8.0 + .5, double(y), z * 8.0 + .5, true};
    return std::nullopt;
}

// A marker on the minimap: inside, or pulled onto the edge (square or round)
// in its direction from the center, `margin` pixels in.
struct EdgeMarker { double x, y; bool inside; };
inline EdgeMarker mapMarker(ViewTransform const& view, double centerX, double centerZ, double worldX, double worldZ,
                            double blocks, int pixels, bool round, double margin) {
    auto p = worldToPixel(view, centerX, centerZ, worldX, worldZ, blocks, pixels, margin);
    double half = pixels / 2.0, reach = half - margin;
    double dx = p.x - half, dy = p.y - half;
    if (round) {
        double d = std::hypot(dx, dy);
        if (d <= reach) return {p.x, p.y, true};
        return {half + dx / d * reach, half + dy / d * reach, false};
    }
    double m = std::max(std::abs(dx), std::abs(dy));
    if (m <= reach) return {p.x, p.y, true};
    return {half + dx / m * reach, half + dy / m * reach, false};
}
// A waypoint diamond: black edge, colored inside.
inline void drawDiamond(std::vector<std::uint32_t>& pixels, int n, double cx, double cy, double size, std::uint32_t color) {
    double outer = size / 2, inner = outer * .72;
    int x0 = std::max(0, int(std::floor(cx - outer - 1))), x1 = std::min(n - 1, int(std::ceil(cx + outer + 1)));
    int y0 = std::max(0, int(std::floor(cy - outer - 1))), y1 = std::min(n - 1, int(std::ceil(cy + outer + 1)));
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x) {
            double d = std::abs(x + .5 - cx) + std::abs(y + .5 - cy); // Diamond distance.
            float edge = static_cast<float>(std::clamp(outer + .5 - d, 0.0, 1.0));
            if (edge <= 0) continue;
            auto& pixel = pixels[static_cast<size_t>(y) * n + x];
            pixel = over(pixel, 0, 0, 0, edge);
            float fill = static_cast<float>(std::clamp(inner + .5 - d, 0.0, 1.0));
            if (fill > 0) pixel = over(pixel, channel(color, 0), channel(color, 1), channel(color, 2), fill);
        }
}
// The death point's cross is red: white read as a passive mob's dot
// (maintainer, 2026-10-01); its shape tells it from the red hostile dots.
inline constexpr std::uint32_t deathColor = packColor(230, 46, 46);
// The death point's cross: red with a black edge.
inline void drawCross(std::vector<std::uint32_t>& pixels, int n, double cx, double cy, double size) {
    double half = size / 2, thick = std::max(1.0, size / 5);
    int x0 = std::max(0, int(std::floor(cx - half - thick))), x1 = std::min(n - 1, int(std::ceil(cx + half + thick)));
    int y0 = std::max(0, int(std::floor(cy - half - thick))), y1 = std::min(n - 1, int(std::ceil(cy + half + thick)));
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x) {
            double px = x + .5 - cx, py = y + .5 - cy;
            if (std::max(std::abs(px), std::abs(py)) > half + thick) continue;
            // Distance to the nearer diagonal.
            double d = std::min(std::abs(px - py), std::abs(px + py)) / std::sqrt(2.0);
            if (std::max(std::abs(px), std::abs(py)) > half) d = std::max(d, std::max(std::abs(px), std::abs(py)) - half);
            float ring = static_cast<float>(std::clamp(thick + .5 - d, 0.0, 1.0));
            if (ring <= 0) continue;
            auto& pixel = pixels[static_cast<size_t>(y) * n + x];
            pixel = over(pixel, 0, 0, 0, ring);
            float fill = static_cast<float>(std::clamp(thick / 2 + .5 - d, 0.0, 1.0));
            if (fill > 0) pixel = over(pixel, channel(deathColor, 0), channel(deathColor, 1), channel(deathColor, 2), fill);
        }
}

// The rendered camera, copied each frame: where it is, its axes, and the
// projection's scale on each screen axis (1 / tan of the half field of view).
struct CameraView {
    double x = 0, y = 0, z = 0;
    std::array<double, 3> right{1, 0, 0}, up{0, 1, 0}, forward{0, 0, 1};
    double scaleX = 1, scaleY = 1;
};
struct ScreenPoint { double x, y, depth; };
// A world point on a screen of width x height GUI units; nothing when behind
// the camera or outside the screen (with `margin` units to spare).
inline std::optional<ScreenPoint> project(CameraView const& c, double wx, double wy, double wz, double width,
                                          double height, double margin = 0) {
    double rx = wx - c.x, ry = wy - c.y, rz = wz - c.z;
    auto dot = [&](std::array<double, 3> const& a) { return rx * a[0] + ry * a[1] + rz * a[2]; };
    double depth = dot(c.forward);
    if (!(depth > .05) || !std::isfinite(depth)) return std::nullopt;
    double nx = dot(c.right) * c.scaleX / depth, ny = dot(c.up) * c.scaleY / depth;
    double sx = (nx + 1) / 2 * width, sy = (1 - ny) / 2 * height;
    if (!std::isfinite(sx) || !std::isfinite(sy) || sx < -margin || sy < -margin || sx > width + margin || sy > height + margin)
        return std::nullopt;
    return ScreenPoint{sx, sy, depth};
}
// "Show in the world": always (the key hides while held), only while the
// key is held, or off (the key does nothing). Decided 2026-10-01.
enum class WorldMarkers { Always, WhileHeld, Off };
inline bool worldMarkersShown(int mode, bool held) {
    switch (static_cast<WorldMarkers>(mode)) {
    case WorldMarkers::Always: return !held;
    case WorldMarkers::WhileHeld: return held;
    default: return false;
    }
}
// The name shows when the crosshair is near the marker.
inline bool nearCrosshair(double sx, double sy, double width, double height) {
    return std::abs(sx - width / 2) < 20 && std::abs(sy - height / 2) < 30;
}
// Pixel rows of a diamond `size` units across (odd): half-width per row.
inline std::vector<int> diamondRows(int size) {
    size = std::max(1, size | 1);
    std::vector<int> rows;
    int half = size / 2;
    for (int r = -half; r <= half; ++r) rows.push_back(half - std::abs(r));
    return rows;
}

// The Waypoints screen lists this dimension's waypoints nearest first, then
// the others by name. Returns indices into `waypoints`.
inline std::vector<size_t> waypointOrder(std::vector<Waypoint> const& waypoints, int dimension, double x, double z) {
    std::vector<size_t> order(waypoints.size());
    for (size_t i = 0; i < order.size(); ++i) order[i] = i;
    auto distance = [&](Waypoint const& w) { return std::hypot(w.x + .5 - x, w.z + .5 - z); };
    std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        auto const& wa = waypoints[a];
        auto const& wb = waypoints[b];
        bool ha = wa.dimension == dimension, hb = wb.dimension == dimension;
        if (ha != hb) return ha;
        if (ha) return distance(wa) < distance(wb);
        return wa.name < wb.name;
    });
    return order;
}
// Editor rows of a selected waypoint, top to bottom.
enum class WaypointField { X, Y, Z, MoveHere, Visible, Color };
inline constexpr std::array<WaypointField, 6> waypointFields{
    WaypointField::X, WaypointField::Y, WaypointField::Z, WaypointField::MoveHere, WaypointField::Visible, WaypointField::Color};
// Coordinates the editor accepts.
inline constexpr int coordinateLimit = 30000000;

// Notices the local player's death from frame samples: alive to dead once.
class DeathWatch {
    bool wasAlive = true;
public:
    void reset() { wasAlive = true; }
    // True on the frame the player is first seen dead.
    bool update(bool alive) {
        bool died = wasAlive && !alive;
        wasAlive = alive;
        return died;
    }
};

// A file name for a server's waypoints: readable host and port, unsafe
// characters replaced. Empty when there is nothing to identify the server.
inline std::string serverFileName(std::string_view host, int port) {
    std::string name;
    for (unsigned char c : host) {
        if (name.size() >= 80) break;
        name += (std::isalnum(c) || c == '.' || c == '-') ? static_cast<char>(std::tolower(c)) : '_';
    }
    while (!name.empty() && (name.back() == '.' || name.back() == ' ')) name.pop_back();
    if (name.empty() || name.find_first_not_of('_') == std::string::npos || port <= 0 || port > 65535) return {};
    return name + "_" + std::to_string(port) + ".json";
}
}
