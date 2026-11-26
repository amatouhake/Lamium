#pragma once
#include "features/map/MapTiles.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <optional>
#include <string_view>
#include <vector>

namespace lamium::map {
// Mob faces on the radar (BACKLOG L-85, docs/demos/radar-icons.html, look
// B). Built at runtime from the texture the game already has; nothing from
// the game is stored. Pure; RadarFaces.cpp reads the model and texture,
// Minimap.cpp draws the faces on screen pixels.

// A face at its texture's own resolution (at most maxFaceSide a side).
inline constexpr int maxFaceSide = 16;
struct Face {
    int width = 0, height = 0;
    std::vector<std::uint32_t> pixels; // RGBA, row-major; alpha 0 is see-through.
};

// Renderers whose default skin is not the mob's own look (an overlay, an
// armor layer or a baby's): the texture to build the face from instead.
inline std::string_view baseTexture(std::string_view renderer) {
    if (renderer == "minecraft:villager_v2") return "textures/entity/villager2/villager";
    if (renderer == "minecraft:horse") return "textures/entity/horse2/horse_white";
    if (renderer == "minecraft:donkey") return "textures/entity/horse2/donkey";
    if (renderer == "minecraft:mule") return "textures/entity/horse2/mule";
    if (renderer == "minecraft:skeleton_horse") return "textures/entity/horse2/horse_skeleton";
    if (renderer == "minecraft:zombie_horse") return "textures/entity/horse2/horse_zombie";
    if (renderer == "minecraft:llama" || renderer == "minecraft:trader_llama") return "textures/entity/llama/llama_creamy";
    if (renderer == "minecraft:fox") return "textures/entity/fox/fox";
    if (renderer == "minecraft:happy_ghast") return "textures/entity/happy_ghast/happy_ghast";
    return {};
}
// Parts drawn over a head that are not the face itself (worn layers and tack).
inline bool faceLayer(std::string_view part) {
    for (auto skip : {"hat", "helmet", "brim", "armor", "jacket", "bridle", "rein", "saddle"})
        if (part.find(skip) != std::string_view::npos) return false;
    return part != "bitl" && part != "bitr";
}

// How a kind's face is seen (L-330, maintainer 2026-11-26): from the front of its "head" with the parts
// hanging from it by default; a kind without a head shows the part where its face is; long necks are cut
// below the face; fish and parrots are seen from the side.
enum class FaceView { Front, Side };
struct FaceRecipe {
    std::vector<std::string_view> parts{"head"};
    bool children = true;             // the parts hanging from those, as a head's nose and ears
    FaceView view = FaceView::Front;
    bool crop = false;                // at most two rows more than its width, from the top
    std::vector<std::string_view> leftOut; // parts the game hides on this kind (a horse's mule ears)
};
inline FaceRecipe faceRecipe(std::string_view renderer) {
    std::string_view const kind = renderer.substr(renderer.starts_with("minecraft:") ? 10 : 0);
    FaceRecipe r;
    auto only = [&](std::initializer_list<std::string_view> parts) { r.parts = parts; r.children = false; };
    if (kind == "horse" || kind == "skeleton_horse" || kind == "zombie_horse") r.leftOut = {"muleearl", "muleearr"};
    else if (kind == "donkey" || kind == "mule") r.leftOut = {"earl", "earr"};
    else if (kind == "llama" || kind == "trader_llama" || kind == "camel" || kind == "camel_husk") r.crop = true;
    else if (kind == "squid" || kind == "glow_squid" || kind == "ghast" || kind == "happy_ghast" || kind == "bee" || kind == "strider")
        only({"body"});
    else if (kind == "slime") r.parts = {"cube"};
    else if (kind == "magma_cube") r.parts = {"insidecube"};
    else if (kind == "silverfish") only({"bodypart_0", "bodypart_1"});
    else if (kind == "endermite") only({"section_0", "section_1"});
    else if (kind == "wither") r.parts = {"head1"};
    else if (kind == "pufferfish") r.parts = {"body"};
    else if (kind == "tadpole") r.parts = {"body", "tail"}, r.view = FaceView::Side;
    else if (kind == "cod" || kind == "tropicalfish") r.parts = {"body"}, r.view = FaceView::Side;
    else if (kind == "salmon") r.parts = {"body_front"}, r.view = FaceView::Side;
    else if (kind == "parrot") r.view = FaceView::Side;
    return r;
}
inline bool drawnPart(FaceRecipe const& recipe, std::string_view part) {
    return faceLayer(part) && std::find(recipe.leftOut.begin(), recipe.leftOut.end(), part) == recipe.leftOut.end();
}

// A face picture at its image's resolution, reduced to fit maxFaceSide: a high-resolution face drops to one
// texel per model unit when that fits, else to the smallest whole factor that does. None when nothing shows.
inline std::optional<Face> reduceFace(std::vector<std::uint32_t> const& full, int fullW, int fullH, double texelsPerUnit) {
    if (fullW <= 0 || fullH <= 0 || full.size() < static_cast<size_t>(fullW) * fullH) return std::nullopt;
    int step = std::max(1, (std::max(fullW, fullH) + maxFaceSide - 1) / maxFaceSide);
    if (int unit = static_cast<int>(std::lround(texelsPerUnit)); unit > step && std::max(fullW, fullH) / unit <= maxFaceSide)
        step = unit;
    Face face{std::max(1, fullW / step), std::max(1, fullH / step), {}};
    face.pixels.assign(static_cast<size_t>(face.width) * face.height, 0);
    bool any = false;
    for (int y = 0; y < face.height; ++y)
        for (int x = 0; x < face.width; ++x) {
            auto c = full[static_cast<size_t>(y * step) * fullW + x * step];
            face.pixels[static_cast<size_t>(y * face.width + x)] = c;
            any = any || (c >> 24);
        }
    if (!any) return std::nullopt;
    return face;
}

// One cube's front: where it covers the head (model units, y up), how near
// it is (smaller z is nearer, the mob facing -z), and its front face in the
// texture (texture units).
struct FaceBox {
    double x0, y0, x1, y1, z;
    double u, v, w, h;
};
// The head seen from the front: every box's front face painted far to
// near, at `texelsPerUnit` image pixels per model unit. A finer texture (a
// high-resolution pack) is reduced by a whole factor to fit maxFaceSide.
// None when nothing is visible.
inline std::optional<Face> composeFace(std::uint8_t const* rgba, int width, int height, double texelsPerUnit,
                                       std::vector<FaceBox> boxes) {
    if (!rgba || width <= 0 || height <= 0 || !(texelsPerUnit > 0) || boxes.empty()) return std::nullopt;
    double minX = 1e9, minY = 1e9, maxX = -1e9, maxY = -1e9;
    for (auto const& b : boxes) {
        minX = std::min(minX, b.x0); maxX = std::max(maxX, b.x1);
        minY = std::min(minY, b.y0); maxY = std::max(maxY, b.y1);
    }
    int fullW = static_cast<int>(std::lround((maxX - minX) * texelsPerUnit));
    int fullH = static_cast<int>(std::lround((maxY - minY) * texelsPerUnit));
    if (fullW <= 0 || fullH <= 0 || fullW > 512 || fullH > 512) return std::nullopt;
    std::vector<std::uint32_t> full(static_cast<size_t>(fullW) * fullH, 0);
    std::stable_sort(boxes.begin(), boxes.end(), [](FaceBox const& a, FaceBox const& b) { return a.z > b.z; });
    for (auto const& b : boxes) {
        // A mob faces north (-z): seen from the front, +x is on the viewer's
        // left, as the front texture is laid out.
        int px0 = static_cast<int>(std::lround((maxX - b.x1) * texelsPerUnit));
        int px1 = static_cast<int>(std::lround((maxX - b.x0) * texelsPerUnit));
        int py0 = static_cast<int>(std::lround((maxY - b.y1) * texelsPerUnit));
        int py1 = static_cast<int>(std::lround((maxY - b.y0) * texelsPerUnit));
        if (px1 <= px0 || py1 <= py0) continue;
        for (int y = py0; y < py1; ++y)
            for (int x = px0; x < px1; ++x) {
                double fx = (x - px0 + .5) / (px1 - px0), fy = (y - py0 + .5) / (py1 - py0);
                int sx = static_cast<int>(std::floor((b.u + fx * b.w) * texelsPerUnit));
                int sy = static_cast<int>(std::floor((b.v + fy * b.h) * texelsPerUnit));
                if (sx < 0 || sy < 0 || sx >= width || sy >= height) continue;
                auto const* p = rgba + (static_cast<size_t>(sy) * width + sx) * 4;
                // Entity textures use partial alpha as a mask (tinting,
                // glow) on pixels the game draws opaque: only 0 is clear.
                if (!p[3]) continue;
                full[static_cast<size_t>(y) * fullW + x] = packColor(p[0], p[1], p[2]);
            }
    }
    return reduceFace(full, fullW, fullH, texelsPerUnit);
}

// A mob's model seen as a picture (L-330): its boxes with each face's
// texture rectangle, turned by the box's own rotation and then each part's
// up to the model's root, painted near over far from the front or the side.
struct Vec3d { double x = 0, y = 0, z = 0; };
// A rotation as the game keeps it (radians, its signs) about a pivot in model units, y up.
struct Turn { Vec3d pivot, angles; };
struct FaceRect { double u = 0, v = 0, w = 0, h = 0; bool valid = false; }; // texture units
enum FaceSide : int { sideDown, sideUp, sideNorth, sideSouth, sideWest, sideEast }; // the game's order of a cube's faces
struct ModelBox {
    Vec3d from, to;
    bool mirrored = false;
    FaceRect faces[6]{};
    std::vector<Turn> turns; // applied in order: the box's own first
};
// The models' order and signs: x, then y, then z, with x and z turned the
// other way (checked against the game's models: a hoglin's head pitched
// +50° looks down, a goat's face slopes to its mouth).
inline Vec3d turned(Vec3d p, Turn const& t) {
    double const ax = -t.angles.x, ay = t.angles.y, az = -t.angles.z;
    double x = p.x - t.pivot.x, y = p.y - t.pivot.y, z = p.z - t.pivot.z;
    double const y1 = y * std::cos(ax) - z * std::sin(ax), z1 = y * std::sin(ax) + z * std::cos(ax);
    double const x2 = x * std::cos(ay) + z1 * std::sin(ay), z2 = -x * std::sin(ay) + z1 * std::cos(ay);
    double const x3 = x2 * std::cos(az) - y1 * std::sin(az), y3 = x2 * std::sin(az) + y1 * std::cos(az);
    return {x3 + t.pivot.x, y3 + t.pivot.y, z2 + t.pivot.z};
}
// A face's corners at its texture's top left, top right and bottom left, seen from outside the box: from the
// front (-z) +x is on the left; the top's lower edge meets the front.
inline void faceCorners(FaceSide side, Vec3d a, Vec3d b, Vec3d out[3]) {
    switch (side) {
    case sideNorth: out[0] = {b.x, b.y, a.z}; out[1] = {a.x, b.y, a.z}; out[2] = {b.x, a.y, a.z}; break;
    case sideSouth: out[0] = {a.x, b.y, b.z}; out[1] = {b.x, b.y, b.z}; out[2] = {a.x, a.y, b.z}; break;
    case sideEast: out[0] = {b.x, b.y, b.z}; out[1] = {b.x, b.y, a.z}; out[2] = {b.x, a.y, b.z}; break;
    case sideWest: out[0] = {a.x, b.y, a.z}; out[1] = {a.x, b.y, b.z}; out[2] = {a.x, a.y, a.z}; break;
    case sideUp: out[0] = {b.x, b.y, b.z}; out[1] = {a.x, b.y, b.z}; out[2] = {b.x, b.y, a.z}; break;
    case sideDown: out[0] = {b.x, a.y, a.z}; out[1] = {a.x, a.y, a.z}; out[2] = {b.x, a.y, b.z}; break;
    }
}
// The picture of `boxes` at `texelsPerUnit` image pixels per model unit, cropped to what is drawn; `crop`
// keeps at most two rows more than its width from the top. Reduced as composeFace's to fit maxFaceSide.
inline std::optional<Face> projectFace(std::uint8_t const* rgba, int width, int height, double texelsPerUnit,
                                       std::vector<ModelBox> const& boxes, FaceView view, bool crop = false) {
    if (!rgba || width <= 0 || height <= 0 || !(texelsPerUnit > 0) || boxes.empty()) return std::nullopt;
    struct Quad { double ax, ay, az, e1x, e1y, e1z, e2x, e2y, e2z, det; FaceRect uv; };
    std::vector<Quad> quads;
    double minX = 1e9, minY = 1e9, maxX = -1e9, maxY = -1e9;
    for (auto const& box : boxes) {
        FaceRect faces[6];
        std::copy(std::begin(box.faces), std::end(box.faces), faces);
        if (box.mirrored) {
            // A mirrored box swaps its sides and reads every face right to left.
            std::swap(faces[sideWest], faces[sideEast]);
            for (auto& f : faces) f.u += f.w, f.w = -f.w;
        }
        for (int side = 0; side < 6; ++side) {
            if (!faces[side].valid) continue;
            Vec3d c[3];
            faceCorners(static_cast<FaceSide>(side), box.from, box.to, c);
            double p[3][3];
            for (int k = 0; k < 3; ++k) {
                for (auto const& t : box.turns) c[k] = turned(c[k], t);
                // Seen from the front: +x on the left, y down, smaller z nearer. From the side (+x): the front on the right.
                if (view == FaceView::Front) p[k][0] = -c[k].x, p[k][1] = -c[k].y, p[k][2] = c[k].z;
                else p[k][0] = -c[k].z, p[k][1] = -c[k].y, p[k][2] = -c[k].x;
            }
            Quad q{p[0][0], p[0][1], p[0][2], p[1][0] - p[0][0], p[1][1] - p[0][1], p[1][2] - p[0][2],
                   p[2][0] - p[0][0], p[2][1] - p[0][1], p[2][2] - p[0][2], 0, faces[side]};
            q.det = q.e1x * q.e2y - q.e1y * q.e2x;
            if (!(q.det > 1e-9)) continue; // facing away, or seen edge on
            for (double s : {0., 1.})
                for (double t : {0., 1.}) {
                    double const x = q.ax + s * q.e1x + t * q.e2x, y = q.ay + s * q.e1y + t * q.e2y;
                    minX = std::min(minX, x); maxX = std::max(maxX, x); minY = std::min(minY, y); maxY = std::max(maxY, y);
                }
            quads.push_back(q);
        }
    }
    if (quads.empty()) return std::nullopt;
    double const x0 = std::floor(minX), y0 = std::floor(minY);
    int const gridW = static_cast<int>(std::lround((std::ceil(maxX) - x0) * texelsPerUnit));
    int const gridH = static_cast<int>(std::lround((std::ceil(maxY) - y0) * texelsPerUnit));
    if (gridW <= 0 || gridH <= 0 || gridW > 512 || gridH > 512) return std::nullopt;
    std::vector<std::uint32_t> grid(static_cast<size_t>(gridW) * gridH, 0);
    std::vector<double> depth(grid.size(), 1e18);
    auto texel = [&](double at) { return static_cast<int>(std::floor(at * texelsPerUnit - 1e-6)); };
    for (auto const& q : quads)
        for (int gy = 0; gy < gridH; ++gy)
            for (int gx = 0; gx < gridW; ++gx) {
                double const dx = x0 + (gx + .5) / texelsPerUnit - q.ax, dy = y0 + (gy + .5) / texelsPerUnit - q.ay;
                double const s = (dx * q.e2y - dy * q.e2x) / q.det, t = (q.e1x * dy - q.e1y * dx) / q.det;
                if (s < 0 || s >= 1 || t < 0 || t >= 1) continue;
                double const z = q.az + s * q.e1z + t * q.e2z;
                size_t const at = static_cast<size_t>(gy) * gridW + gx;
                if (z >= depth[at]) continue;
                // The texel holding this point; a face read right to left (negative width) steps back.
                int const sx = q.uv.w < 0 ? texel(q.uv.u + s * q.uv.w) : static_cast<int>(std::floor((q.uv.u + s * q.uv.w) * texelsPerUnit));
                int const sy = q.uv.h < 0 ? texel(q.uv.v + t * q.uv.h) : static_cast<int>(std::floor((q.uv.v + t * q.uv.h) * texelsPerUnit));
                if (sx < 0 || sy < 0 || sx >= width || sy >= height) continue;
                auto const* px = rgba + (static_cast<size_t>(sy) * width + sx) * 4;
                if (!px[3]) continue; // only 0 is clear (composeFace)
                grid[at] = packColor(px[0], px[1], px[2]);
                depth[at] = z;
            }
    int left = gridW, right = -1, top = gridH, bottom = -1;
    for (int y = 0; y < gridH; ++y)
        for (int x = 0; x < gridW; ++x)
            if (grid[static_cast<size_t>(y) * gridW + x] >> 24) left = std::min(left, x), right = std::max(right, x), top = std::min(top, y), bottom = std::max(bottom, y);
    if (right < 0) return std::nullopt;
    int const w = right - left + 1;
    int h = bottom - top + 1;
    if (crop) h = std::min(h, w + 2 * std::max(1, static_cast<int>(std::lround(texelsPerUnit))));
    std::vector<std::uint32_t> cut(static_cast<size_t>(w) * h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) cut[static_cast<size_t>(y) * w + x] = grid[static_cast<size_t>(top + y) * gridW + left + x];
    return reduceFace(cut, w, h, texelsPerUnit);
}

// A player's head from their skin image (L-87): the front of the head in
// the classic skin layout (8x8 at (8, 8) per 64 pixels of width, 64x64 or
// the legacy 64x32 and their multiples) with the outer layer at (40, 8)
// laid over it. The base layer is drawn whole, as the game draws it; outer
// texels with any alpha cover it. None for other layouts or a clear face.
inline std::optional<Face> playerHead(std::uint8_t const* rgba, int width, int height) {
    if (!rgba || width < 64 || width % 64 || (height != width && height * 2 != width)) return std::nullopt;
    int unit = width / 64, side = 8 * unit;
    int step = (side + maxFaceSide - 1) / maxFaceSide;
    Face face{side / step, side / step, {}};
    face.pixels.assign(static_cast<size_t>(face.width) * face.height, 0);
    bool any = false;
    for (int y = 0; y < face.height; ++y)
        for (int x = 0; x < face.width; ++x) {
            auto at = [&](int u) { return rgba + (static_cast<size_t>(8 * unit + y * step) * width + u + x * step) * 4; };
            auto const* base = at(8 * unit);
            auto const* outer = at(40 * unit);
            auto const* p = outer[3] ? outer : base;
            any = any || base[3] || outer[3];
            face.pixels[static_cast<size_t>(y * face.width + x)] = packColor(p[0], p[1], p[2]);
        }
    if (!any) return std::nullopt;
    return face;
}

// Faces share one runtime texture: a grid of cells, each a face with a
// one-texel black outline around its visible pixels (look B). The outline
// is part of the image, so it moves with the face exactly.
inline constexpr int faceCellSide = maxFaceSide + 2;
inline constexpr int faceAtlasSide = 256, faceAtlasCells = faceAtlasSide / faceCellSide;
inline constexpr int faceAtlasCapacity = faceAtlasCells * faceAtlasCells;
inline constexpr std::uint32_t faceOutline = packColor(0, 0, 0, 230);
struct AtlasCell { int x, y; };
inline AtlasCell faceAtlasCell(int index) {
    return {(index % faceAtlasCells) * faceCellSide, (index / faceAtlasCells) * faceCellSide};
}
// The face's outlined image size inside its cell.
inline int outlinedWidth(Face const& face) { return face.width + 2; }
inline int outlinedHeight(Face const& face) { return face.height + 2; }
inline void writeFace(std::vector<std::uint32_t>& atlas, int index, Face const& face) {
    if (index < 0 || index >= faceAtlasCapacity) return;
    atlas.resize(static_cast<size_t>(faceAtlasSide) * faceAtlasSide, 0);
    auto cell = faceAtlasCell(index);
    auto opaque = [&](int x, int y) {
        return x >= 0 && y >= 0 && x < face.width && y < face.height
            && (face.pixels[static_cast<size_t>(y * face.width + x)] >> 24);
    };
    for (int y = -1; y <= face.height; ++y)
        for (int x = -1; x <= face.width; ++x) {
            std::uint32_t c = 0;
            if (opaque(x, y)) c = face.pixels[static_cast<size_t>(y * face.width + x)];
            else
                for (int dy = -1; dy <= 1 && !c; ++dy)
                    for (int dx = -1; dx <= 1 && !c; ++dx)
                        if (opaque(x + dx, y + dy)) c = faceOutline;
            atlas[static_cast<size_t>(cell.y + y + 1) * faceAtlasSide + cell.x + x + 1] = c;
        }
}
// Screen pixels per face texel, always whole so the texture's pixels stay
// even, and the same for every mob (maintainer 2026-11-26, L-330): an
// 8-texel face fills about `target` screen pixels, the others keep their
// models' sizes beside it, and the one-texel outline is as wide on all.
inline int faceTexelPixels(int longerSide, double target) {
    if (longerSide <= 0 || !(target > 0)) return 1;
    return std::max(1, static_cast<int>(std::lround(target / 8)));
}
}
