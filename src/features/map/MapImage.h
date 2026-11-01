#pragma once
#include "features/map/MapTiles.h"
#include "features/map/MapView.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace lamium::map {
// Composes the minimap's RGBA pixels from owned tiles (BACKLOG L-60, look in
// docs/demos/minimap.html). Pure; Minimap.cpp uploads the result.

// Height shading against the north and west neighbors: higher than them is
// lighter, lower is darker. Strengthened 2026-10-07 (was .06 per block,
// .7-1.2) because relief was hard to read.
inline float shadeFactor(int height, int north, int west) {
    float k = 1.f + .1f * float(height - north) + .1f * float(height - west);
    return std::clamp(k, .6f, 1.3f);
}
inline std::uint32_t shade(std::uint32_t color, float k) {
    auto c = [&](int i) { return static_cast<int>(std::lround(std::min(255.f, channel(color, i) * k))); };
    return packColor(c(0), c(1), c(2), channel(color, 3));
}
// Source-over blend of a straight-alpha color onto a straight-alpha pixel.
inline std::uint32_t over(std::uint32_t destination, int r, int g, int b, float alpha) {
    alpha = std::clamp(alpha, 0.f, 1.f);
    float da = channel(destination, 3) / 255.f;
    float oa = alpha + da * (1 - alpha);
    if (oa <= 0) return 0;
    auto mix = [&](int source, int i) {
        return static_cast<int>(std::lround((source * alpha + channel(destination, i) * da * (1 - alpha)) / oa));
    };
    return packColor(mix(r, 0), mix(g, 1), mix(b, 2), static_cast<int>(std::lround(oa * 255)));
}

// Rebuild a chunk's shaded colors. Neighbors outside the chunk come from
// the cache; an unknown neighbor counts as level ground.
inline void shadeTile(TileCache const& cache, ChunkKey key, Tile& tile) {
    Tile const* north = cache.find(ChunkKey{key.x, key.z - 1});
    Tile const* west = cache.find(ChunkKey{key.x - 1, key.z});
    auto known = [](Tile const* t, int index) -> Column const* {
        if (!t || !t->loaded) return nullptr;
        auto const& c = t->columns[static_cast<size_t>(index)];
        return (c.color >> 24) ? &c : nullptr;
    };
    for (int z = 0; z < 16; ++z)
        for (int x = 0; x < 16; ++x) {
            int index = z * 16 + x;
            auto const& c = tile.columns[static_cast<size_t>(index)];
            if (!tile.loaded || !(c.color >> 24)) { tile.shaded[static_cast<size_t>(index)] = 0; continue; }
            auto const* n = z ? known(&tile, index - 16) : known(north, 15 * 16 + x);
            auto const* w = x ? known(&tile, index - 1) : known(west, z * 16 + 15);
            float k = shadeFactor(c.height, n ? n->height : c.height, w ? w->height : c.height);
            tile.shaded[static_cast<size_t>(index)] = shade(c.color, k);
        }
    tile.shadedValid = true;
}
// Shaded color lookups; neighboring pixels mostly hit the same chunk.
class ShadedReader {
    TileCache& cache;
    ChunkKey last{};
    Tile* tile = nullptr;
    bool primed = false;
public:
    explicit ShadedReader(TileCache& cache) : cache(cache) {}
    std::uint32_t at(int blockX, int blockZ) {
        auto key = chunkOf(blockX, blockZ);
        if (!primed || !(key == last)) {
            tile = cache.find(key);
            last = key;
            primed = true;
            if (tile && !tile->shadedValid) shadeTile(cache, key, *tile);
        }
        return tile ? tile->shaded[static_cast<size_t>(columnIndex(blockX, blockZ))] : 0;
    }
};

struct Frame {
    int pixels = 256;
    double centerX = 0, centerZ = 0;
    double blocks = 128;
    ViewTransform view;
    bool round = false;
    std::uint32_t unknown = 0; // Fill where nothing is known yet.
};
inline bool insideShape(int px, int py, int pixels, bool round, double inset = 0) {
    if (!round) return px >= inset && py >= inset && px < pixels - inset && py < pixels - inset;
    double r = pixels / 2.0 - inset;
    double dx = px + .5 - pixels / 2.0, dy = py + .5 - pixels / 2.0;
    return dx * dx + dy * dy <= r * r;
}
// Unknown columns get the frame's fill: the map shows only what the client has.
inline void composeTerrain(TileCache& cache, Frame const& frame, std::vector<std::uint32_t>& out) {
    int n = std::max(1, frame.pixels);
    out.assign(static_cast<size_t>(n) * n, 0);
    double perPixel = std::max(1.0, frame.blocks) / n;
    ShadedReader reader(cache);
    for (int py = 0; py < n; ++py) {
        // World position steps linearly along a row.
        double v = (py + .5 - n / 2.0) * perPixel;
        auto start = frame.view.toWorld((.5 - n / 2.0) * perPixel, v);
        auto step = frame.view.toWorld(perPixel, 0);
        for (int px = 0; px < n; ++px) {
            if (!insideShape(px, py, n, frame.round)) continue;
            int bx = blockFloor(frame.centerX + start.x + step.x * px);
            int bz = blockFloor(frame.centerZ + start.z + step.z * px);
            auto color = reader.at(bx, bz);
            out[static_cast<size_t>(py) * n + px] = color ? color : frame.unknown;
        }
    }
}

// The player arrow, pointing up at angle 0 and turning clockwise. Outline in
// black, fill white. `size` is its height in pixels.
struct Point {
    double x, y;
    bool operator==(Point const&) const = default;
};
inline constexpr std::array<Point, 4> arrowShape{{{0, -9}, {6.5, 7}, {0, 3.5}, {-6.5, 7}}};
inline double segmentDistance(Point p, Point a, Point b) {
    double vx = b.x - a.x, vy = b.y - a.y;
    double length = vx * vx + vy * vy;
    double t = length > 0 ? std::clamp(((p.x - a.x) * vx + (p.y - a.y) * vy) / length, 0.0, 1.0) : 0;
    double dx = p.x - (a.x + t * vx), dy = p.y - (a.y + t * vy);
    return std::sqrt(dx * dx + dy * dy);
}
template <class Polygon>
inline bool insidePolygon(Point p, Polygon const& polygon) {
    bool inside = false;
    size_t const n = polygon.size();
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        auto const& a = polygon[i];
        auto const& b = polygon[j];
        if ((a.y > p.y) != (b.y > p.y) && p.x < (b.x - a.x) * (p.y - a.y) / (b.y - a.y) + a.x) inside = !inside;
    }
    return inside;
}
// A footprint on the minimap: a black-edged colored outline around a faint
// fill, clipped to the map's shape. `marks` (pixels, 0 for none) thickens
// the outline along each corner for that length: schematic placements'
// corner marks (L-296). The polygon is a box's 4 corners or a round shape's
// ring.
template <class Polygon>
inline void drawPolygonOutline(std::vector<std::uint32_t>& pixels, int n, Polygon const& corners, std::uint32_t color,
                               double width, bool round, float fillAlpha = .18f, double marks = 0) {
    size_t const count = corners.size();
    if (count < 3) return;
    double reach = width / 2 + 2.5;
    double minX = corners[0].x, maxX = minX, minY = corners[0].y, maxY = minY;
    for (auto const& c : corners) {
        minX = std::min(minX, c.x); maxX = std::max(maxX, c.x);
        minY = std::min(minY, c.y); maxY = std::max(maxY, c.y);
    }
    if (!(maxX >= -reach && maxY >= -reach && minX <= n + reach && minY <= n + reach)) return;
    // Each corner's two mark segments, toward its neighbors.
    std::vector<std::pair<Point, Point>> markSegments;
    if (marks > 0)
        for (size_t i = 0; i < count; ++i)
            for (size_t other : {(i + 1) % count, (i + count - 1) % count}) {
                auto a = corners[i], b = corners[other];
                double length = std::hypot(b.x - a.x, b.y - a.y);
                double t = length > 0 ? std::min(1.0, marks / length) : 0;
                markSegments.push_back({a, {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t}});
            }
    int x0 = std::max(0, int(std::floor(minX - reach))), x1 = std::min(n - 1, int(std::ceil(maxX + reach)));
    int y0 = std::max(0, int(std::floor(minY - reach))), y1 = std::min(n - 1, int(std::ceil(maxY + reach)));
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x) {
            if (!insideShape(x, y, n, round)) continue;
            Point p{x + .5, y + .5};
            double d = std::numeric_limits<double>::infinity();
            for (size_t i = 0, j = count - 1; i < count; j = i++) d = std::min(d, segmentDistance(p, corners[j], corners[i]));
            double m = std::numeric_limits<double>::infinity();
            for (auto const& [a, b] : markSegments) m = std::min(m, segmentDistance(p, a, b));
            double thick = width / 2 + 1.5; // the marks' half-width
            auto& pixel = pixels[static_cast<size_t>(y) * n + x];
            if (insidePolygon(p, corners) && fillAlpha > 0)
                pixel = over(pixel, channel(color, 0), channel(color, 1), channel(color, 2), fillAlpha);
            float edge = static_cast<float>(std::clamp(std::max(width / 2 + 1.5 - d, thick + 1 - m), 0.0, 1.0));
            if (edge > 0) pixel = over(pixel, 0, 0, 0, edge * .8f);
            float line = static_cast<float>(std::clamp(std::max(width / 2 + .5 - d, thick - m), 0.0, 1.0));
            if (line > 0) pixel = over(pixel, channel(color, 0), channel(color, 1), channel(color, 2), line);
        }
}
// A box footprint; one smaller than a few pixels still shows as a small square.
inline void drawOutline(std::vector<std::uint32_t>& pixels, int n, std::array<Point, 4> corners, std::uint32_t color,
                        double width, bool round, float fillAlpha = .18f, double marks = 0) {
    constexpr double smallest = 3;
    double cx = 0, cy = 0;
    for (auto const& c : corners) { cx += c.x / 4; cy += c.y / 4; }
    if (std::hypot(corners[2].x - corners[0].x, corners[2].y - corners[0].y) < smallest) {
        double h = smallest / 2;
        corners = {{{cx - h, cy - h}, {cx + h, cy - h}, {cx + h, cy + h}, {cx - h, cy + h}}};
        marks = 0;
    }
    drawPolygonOutline(pixels, n, corners, color, width, round, fillAlpha, marks);
}
// Points around the ellipse inscribed in a world box, for a round shape's
// footprint (L-296); the caller maps them onto the map.
inline std::vector<Point> ellipseRing(double x0, double z0, double x1, double z1, int points = 32) {
    std::vector<Point> ring;
    double cx = (x0 + x1) / 2, cz = (z0 + z1) / 2, rx = (x1 - x0) / 2, rz = (z1 - z0) / 2;
    for (int i = 0; i < points; ++i) {
        double a = 2 * 3.14159265358979323846 * i / points;
        ring.push_back({cx + rx * std::cos(a), cz + rz * std::sin(a)});
    }
    return ring;
}
inline void drawArrow(std::vector<std::uint32_t>& pixels, int n, double cx, double cy, double angle, double size,
                      std::uint32_t fill = 0xFFFFFFFF) {
    double unit = size / 16.0, outline = 1.5 * unit;
    double c = std::cos(angle), s = std::sin(angle);
    std::array<Point, 4> shape{};
    for (size_t i = 0; i < shape.size(); ++i) {
        auto p = arrowShape[i];
        shape[i] = {cx + (p.x * c - p.y * s) * unit, cy + (p.x * s + p.y * c) * unit};
    }
    double reach = 10 * unit + outline + 1;
    int x0 = std::max(0, int(std::floor(cx - reach))), x1 = std::min(n - 1, int(std::ceil(cx + reach)));
    int y0 = std::max(0, int(std::floor(cy - reach))), y1 = std::min(n - 1, int(std::ceil(cy + reach)));
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x) {
            Point p{x + .5, y + .5};
            double edge = 1e9;
            for (size_t i = 0, j = shape.size() - 1; i < shape.size(); j = i++)
                edge = std::min(edge, segmentDistance(p, shape[j], shape[i]));
            bool inside = insidePolygon(p, shape);
            auto& pixel = pixels[static_cast<size_t>(y) * n + x];
            // Coverage over one pixel keeps the edges smooth at small sizes.
            if (inside) {
                float coverage = static_cast<float>(std::clamp(edge - outline / 2 + .5, 0.0, 1.0));
                pixel = over(over(pixel, 0, 0, 0, 1), channel(fill, 0), channel(fill, 1), channel(fill, 2),
                             coverage * channel(fill, 3) / 255.f);
            } else {
                float ring = static_cast<float>(std::clamp(outline / 2 + .5 - edge, 0.0, 1.0));
                if (ring > 0) pixel = over(pixel, 0, 0, 0, ring);
            }
        }
}
// Screen angle of the player's heading on a north-up map (0 = up, clockwise).
inline double northUpArrowAngle(float yawDegrees) {
    auto f = heading(yawDegrees);
    return std::atan2(f.x, -f.z);
}
// The green arrow that marks where FreeCamera looks. The world map and the
// minimap draw the same marker (L-228, L-286).
inline constexpr std::uint32_t cameraArrowColor = packColor(108, 195, 73);

// The thin frame: a dark line inside a faint light one, `width` pixels each.
inline void drawFrame(std::vector<std::uint32_t>& pixels, int n, bool round, double width) {
    width = std::max(1.0, width);
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x) {
            if (!insideShape(x, y, n, round)) continue;
            auto& pixel = pixels[static_cast<size_t>(y) * n + x];
            if (!insideShape(x, y, n, round, width)) pixel = over(pixel, 255, 255, 255, .35f);
            else if (!insideShape(x, y, n, round, 2 * width)) pixel = over(pixel, 20, 20, 20, .9f);
        }
}

// Compass letter centers, `inset` pixels in from the edge, in N E S W order.
// On a square map a turned letter slides along the edge.
inline std::array<Point, 4> compassPoints(ViewTransform const& view, int n, double inset, bool round) {
    constexpr std::array<std::array<double, 2>, 4> directions{{{0, -1}, {1, 0}, {0, 1}, {-1, 0}}};
    std::array<Point, 4> result{};
    double r = n / 2.0 - inset;
    for (size_t i = 0; i < 4; ++i) {
        auto m = view.toMap(directions[i][0], directions[i][1]);
        double scale = round ? std::hypot(m.x, m.z) : std::max(std::abs(m.x), std::abs(m.z));
        if (scale <= 0) scale = 1;
        result[i] = {n / 2.0 + m.x / scale * r, n / 2.0 + m.z / scale * r};
    }
    return result;
}
}
