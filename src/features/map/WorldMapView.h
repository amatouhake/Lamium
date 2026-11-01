#pragma once
#include "features/map/MapRegion.h"
#include "features/map/MapView.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <set>
#include <tuple>
#include <vector>

namespace lamium::map {
// The world map screen's geometry (BACKLOG L-60 world map, look in
// docs/demos/worldmap.html). Pure; WorldMap.cpp draws.

// GUI units per block, from far to close. Steps past 1/8 open only as far
// as the explored map needs to fit the screen (L-187).
inline constexpr std::array<double, 25> worldScales{
    1 / 256., 3 / 512., 1 / 128., 3 / 256., 1 / 64., 3 / 128., 1 / 32., 3 / 64., 1 / 16., 3 / 32.,
    1 / 8., 3 / 16., 1 / 4., 3 / 8., 1 / 2., 3 / 4., 1., 1.5, 2., 3., 4., 6., 8., 12., 16.};
inline constexpr int alwaysFarthestScale = 10; // 1/8: open even with little explored.
inline constexpr int defaultWorldScale = 18;
inline constexpr int clampWorldScale(int index) { return std::clamp(index, 0, static_cast<int>(worldScales.size()) - 1); }
// The farthest step the view may zoom out to: far enough that blocks
// `spanX` x `spanZ` fit in a `width` x `height` area, at least 1/8.
inline int farthestScaleFor(double spanX, double spanZ, double width, double height) {
    int farthest = alwaysFarthestScale;
    auto fits = [&](int i) { return spanX * worldScales[static_cast<size_t>(i)] <= width && spanZ * worldScales[static_cast<size_t>(i)] <= height; };
    while (farthest > 0 && !fits(farthest)) --farthest;
    return farthest;
}
// The Nether's saved layers: y 0-127 in 16-block bands.
inline constexpr int netherBands = 8;

// An image of 256x256 pixels covering 256 << lod blocks: level 0 is one
// region, each level above averages four of the level below.
struct TileKey {
    int lod = 0, x = 0, z = 0;
    bool operator==(TileKey const&) const = default;
};
// The tile at `tile.lod` that contains a region. Tile coordinates are already
// at that LOD, so only the region is shifted.
inline bool tileCoversRegion(TileKey tile, RegionKey region) {
    return tile.x == (region.x >> tile.lod) && tile.z == (region.z >> tile.lod);
}
inline constexpr int maxLod = 8;
inline constexpr int tileBlocks(int lod) { return regionBlocks << lod; }
inline constexpr TileKey parentTile(TileKey tile) { return {tile.lod + 1, floorDiv(tile.x, 2), floorDiv(tile.z, 2)}; }
// The tile of level `lod` (not finer than `tile`) holding `tile`.
inline constexpr TileKey ancestorAt(TileKey tile, int lod) {
    int up = std::max(0, lod - tile.lod);
    return {tile.lod + up, tile.x >> up, tile.z >> up};
}
// Zooming out (L-211): finer images already on the GPU stand in for tiles of
// level `lod` not shown yet. Indexes into `onGpu`, coarser first so finer
// ones draw on top.
inline std::vector<size_t> finerStandIns(int lod, std::vector<TileKey> const& missing, std::vector<TileKey> const& onGpu) {
    std::vector<size_t> result;
    for (size_t i = 0; i < onGpu.size(); ++i)
        if (onGpu[i].lod < lod && std::find(missing.begin(), missing.end(), ancestorAt(onGpu[i], lod)) != missing.end())
            result.push_back(i);
    std::stable_sort(result.begin(), result.end(), [&](size_t a, size_t b) { return onGpu[a].lod > onGpu[b].lod; });
    return result;
}
// Which saved regions exist: coarse images skip empty space without
// touching the disk, and the bounds say how far the explored map reaches.
class ExploredIndex {
    std::set<std::tuple<int, int, int>> occupied; // lod, x, z of every tile with a region under it.
    RegionKey low{}, high{};
    bool any = false;
public:
    // False when it was known already.
    bool add(RegionKey key) {
        if (!occupied.insert({0, key.x, key.z}).second) return false;
        for (int lod = 1; lod <= maxLod; ++lod) occupied.insert({lod, key.x >> lod, key.z >> lod});
        low = any ? RegionKey{std::min(low.x, key.x), std::min(low.z, key.z)} : key;
        high = any ? RegionKey{std::max(high.x, key.x), std::max(high.z, key.z)} : key;
        any = true;
        return true;
    }
    bool has(TileKey tile) const { return occupied.contains({tile.lod, tile.x, tile.z}); }
    bool empty() const { return !any; }
    // Blocks covered by the known regions: x0, z0 inclusive, x1, z1 exclusive.
    struct Bounds { int x0 = 0, z0 = 0, x1 = 0, z1 = 0; };
    std::optional<Bounds> bounds() const {
        if (empty()) return std::nullopt;
        return Bounds{low.x * regionBlocks, low.z * regionBlocks, (high.x + 1) * regionBlocks, (high.z + 1) * regionBlocks};
    }
};
// The coarsest level whose pixel still covers a screen pixel or more, so no
// more pixels are built and uploaded than the screen can show.
inline int lodFor(double unitsPerBlock, double screenPixelsPerUnit) {
    int lod = 0;
    while (lod < maxLod && unitsPerBlock * double(1 << lod) * screenPixelsPerUnit < 1) ++lod;
    return lod;
}

struct WorldView {
    double centerX = 0, centerZ = 0;
    int zoom = defaultWorldScale;
    double width = 0, height = 0; // GUI units.
    int farthest = alwaysFarthestScale;
    double scale() const { return worldScales[static_cast<size_t>(clampWorldScale(zoom))]; }
    double screenX(double worldX) const { return width / 2 + (worldX - centerX) * scale(); }
    double screenY(double worldZ) const { return height / 2 + (worldZ - centerZ) * scale(); }
    double worldX(double x) const { return centerX + (x - width / 2) / scale(); }
    double worldZ(double y) const { return centerZ + (y - height / 2) / scale(); }
    // Keeps the world point under (x, y) in place.
    void zoomAt(double x, double y, int step) {
        double wx = worldX(x), wz = worldZ(y);
        // A view already farther out (the map was cleared) is not pushed in.
        zoom = std::clamp(zoom + step, std::min(clampWorldScale(farthest), zoom), static_cast<int>(worldScales.size()) - 1);
        centerX = wx - (x - width / 2) / scale();
        centerZ = wz - (y - height / 2) / scale();
    }
    void pan(double dx, double dy) {
        centerX = std::clamp(centerX - dx / scale(), -3.0e7, 3.0e7);
        centerZ = std::clamp(centerZ - dy / scale(), -3.0e7, 3.0e7);
    }
};
// Tiles of one level covering the view, nearest the center first.
inline std::vector<TileKey> visibleTiles(WorldView const& view, int lod) {
    double blocks = tileBlocks(lod);
    int x0 = static_cast<int>(std::floor(view.worldX(0) / blocks)), x1 = static_cast<int>(std::floor(view.worldX(view.width) / blocks));
    int z0 = static_cast<int>(std::floor(view.worldZ(0) / blocks)), z1 = static_cast<int>(std::floor(view.worldZ(view.height) / blocks));
    std::vector<TileKey> tiles;
    if (x1 - x0 > 64 || z1 - z0 > 64) return tiles;
    for (int z = z0; z <= z1; ++z)
        for (int x = x0; x <= x1; ++x) tiles.push_back({lod, x, z});
    auto distance = [&](TileKey t) {
        double dx = (t.x + .5) * blocks - view.centerX, dz = (t.z + .5) * blocks - view.centerZ;
        return dx * dx + dz * dz;
    };
    std::sort(tiles.begin(), tiles.end(), [&](TileKey a, TileKey b) { return distance(a) < distance(b); });
    return tiles;
}
// A GUI coordinate moved onto the screen's pixel grid; tiles drawn edge to
// edge then meet without seams or overlaps.
inline double snapToPixel(double units, double screenPixelsPerUnit) {
    if (!(screenPixelsPerUnit > 0)) return units;
    return std::round(units * screenPixelsPerUnit) / screenPixelsPerUnit;
}
// A round length for the scale bar at least `minUnits` long.
inline int scaleBarBlocks(double unitsPerBlock, double minUnits) {
    for (int blocks : {1, 2, 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000})
        if (blocks * unitsPerBlock >= minUnits) return blocks;
    return 50000;
}
// The world map's radar (L-297): players at any zoom; mobs and dropped items
// only from 1 GUI unit per block in, since farther out they pile up around
// the player.
inline constexpr double radarMobsMinScale = 1;
inline constexpr bool radarDotShown(bool player, double unitsPerBlock) { return player || unitsPerBlock >= radarMobsMinScale; }
// The outline of the ellipse inscribed in a screen box [x0, x1) by [y0, y1),
// in whole GUI units, as rectangles (the UI draws rectangles only): a round
// shape's footprint (L-296). Up to 160 units tall it is drawn row by row,
// mirrored on both axes, so a small one (an icon, a far-out shape) is a
// clean pixel circle; taller ones join at most 96 points around it by a
// horizontal and a vertical run each.
struct UnitRect { int x, y, w, h; };
inline std::vector<UnitRect> ellipseOutline(int x0, int y0, int x1, int y1) {
    std::vector<UnitRect> rects;
    int const w = x1 - x0, h = y1 - y0;
    if (w < 2 || h < 2) {
        rects.push_back({x0, y0, std::max(1, w), std::max(1, h)});
        return rects;
    }
    if (h <= 160) {
        // Each row's inset from the left; the right side mirrors it, and the
        // lower half mirrors the upper.
        std::vector<int> inset(static_cast<size_t>(h));
        for (int j = 0; j < (h + 1) / 2; ++j) {
            double v = (j + .5 - h / 2.0) / (h / 2.0);
            int in = static_cast<int>(std::lround(w / 2.0 * (1 - std::sqrt(std::max(0.0, 1 - v * v)))));
            inset[static_cast<size_t>(j)] = inset[static_cast<size_t>(h - 1 - j)] = std::min(in, (w - 1) / 2);
        }
        for (int j = 0; j < h; ++j) {
            int in = inset[static_cast<size_t>(j)];
            bool edge = j == 0 || j == h - 1;
            // Down to the next row's inset, so the line stays closed.
            int to = edge ? w - in : std::max({in + 1, inset[static_cast<size_t>(j - 1)], inset[static_cast<size_t>(j + 1)]});
            if (edge || 2 * to >= w) rects.push_back({x0 + in, y0 + j, w - 2 * in, 1});
            else {
                rects.push_back({x0 + in, y0 + j, to - in, 1});
                rects.push_back({x1 - to, y0 + j, to - in, 1});
            }
        }
        return rects;
    }
    double cx = (x0 + x1 - 1) / 2.0, cy = (y0 + y1 - 1) / 2.0, rx = (w - 1) / 2.0, ry = (h - 1) / 2.0;
    int count = std::clamp(static_cast<int>((rx + ry) * 2), 12, 96);
    std::vector<std::array<int, 2>> points;
    for (int i = 0; i < count; ++i) {
        double a = 2 * 3.14159265358979323846 * i / count;
        std::array<int, 2> p{static_cast<int>(std::lround(cx + rx * std::cos(a))), static_cast<int>(std::lround(cy + ry * std::sin(a)))};
        if (points.empty() || points.back() != p) points.push_back(p);
    }
    for (size_t i = 0; i < points.size(); ++i) {
        auto p = points[i], q = points[(i + 1) % points.size()];
        rects.push_back({std::min(p[0], q[0]), p[1], std::abs(q[0] - p[0]) + 1, 1});
        if (q[1] != p[1]) rects.push_back({q[0], std::min(p[1], q[1]), 1, std::abs(q[1] - p[1]) + 1});
    }
    return rects;
}
}
