#pragma once
#include "features/schematic/Placement.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <span>
#include <utility>
#include <vector>

// The ghost cells whose drawing depends on the camera (BACKLOG L-256 item 7):
// within `reach` cells per axis of the eye or feet cell. Sections are built
// without the camera, so walking does not rebuild them; these cells are built
// with it into a small mesh of their own, and the sections are drawn in
// vertex runs that leave them out.
namespace lamium::schematic::nearcells {
inline constexpr int reach = 2;

struct Box {
    std::array<std::optional<Point>, 2> cells; // eye and feet; none: an empty box
    bool operator==(Box const&) const = default;
    bool empty() const { return !cells[0] && !cells[1]; }
    // `around`: also the cells that many cells outside the box.
    bool contains(Point p, int around = 0) const {
        int r = reach + around;
        for (auto const& c : cells)
            if (c && std::abs(c->x - p.x) <= r && std::abs(c->y - p.y) <= r && std::abs(c->z - p.z) <= r) return true;
        return false;
    }
    // Whether any cell of the cube of `size` cells from `low` is in the box.
    bool touches(Point low, int size) const {
        for (auto const& c : cells)
            if (c && c->x + reach >= low.x && c->x - reach < low.x + size && c->y + reach >= low.y && c->y - reach < low.y + size
                && c->z + reach >= low.z && c->z - reach < low.z + size)
                return true;
        return false;
    }
    // The cells' bounds, [low, high); an empty box gives low == high.
    std::pair<Point, Point> bounds() const {
        Point low{}, high{};
        bool any = false;
        for (auto const& c : cells) {
            if (!c) continue;
            Point a{c->x - reach, c->y - reach, c->z - reach}, b{c->x + reach + 1, c->y + reach + 1, c->z + reach + 1};
            if (!any) { low = a; high = b; any = true; continue; }
            low = {std::min(low.x, a.x), std::min(low.y, a.y), std::min(low.z, a.z)};
            high = {std::max(high.x, b.x), std::max(high.y, b.y), std::max(high.z, b.z)};
        }
        return {low, high};
    }
};

// A mesh's cells in the order they were appended, each with its first
// vertex. No cell: vertices that never depend on the camera (mistake
// outlines), always drawn.
struct CellStart {
    std::optional<Point> cell;
    std::uint32_t first = 0;
};
struct Run {
    std::uint32_t first = 0, count = 0;
    bool operator==(Run const&) const = default;
};
// The vertex runs to draw of a mesh of `total` vertices, leaving out the
// cells `box` holds. Vertices before the first start are kept.
inline std::vector<Run> runs(std::span<CellStart const> starts, std::uint32_t total, Box const& box) {
    std::vector<Run> out;
    std::uint32_t from = 0; // where the kept stretch began
    for (size_t i = 0; i < starts.size(); ++i) {
        if (!starts[i].cell || !box.contains(*starts[i].cell)) continue;
        std::uint32_t begin = std::min(starts[i].first, total);
        std::uint32_t end = i + 1 < starts.size() ? std::min(starts[i + 1].first, total) : total;
        if (begin > from) out.push_back({from, begin - from});
        from = std::max(from, end);
    }
    if (total > from) out.push_back({from, total - from});
    return out;
}
}
