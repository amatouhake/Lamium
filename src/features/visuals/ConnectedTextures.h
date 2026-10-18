#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lamium::visuals::connected {
// Connected Textures (BACKLOG L-96, L-164): a block face drops its border on
// the sides where the neighbor in the face's plane is the same block, so
// blocks of one kind read as one surface. The face keeps vanilla's single quad
// and shows a sprite baked from the loaded texture for its joined sides; the
// sprites are added to the terrain atlas when it is built. Splitting faces into
// several quads darkened them (L-133 panes, L-156 bookshelves under Vibrant
// Visuals); one quad per face did not (L-164 spike, 2026-10-12). Pure parts;
// ConnectedTexturesHooks.cpp and ConnectedSprites.cpp are the game glue.
enum class Face { Down, Up, North, South, West, East };
struct Offset { int x = 0, y = 0, z = 0; };
// The neighbors beside a face, seen from outside: toward the texture's left
// (u0), right (u1), top (v0) and bottom (v1). Checked in game on all six
// faces (2026-10-11).
struct Sides { Offset left, right, top, bottom; };
inline constexpr Sides sides(Face face) {
    switch (face) {
    case Face::North: return {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}};
    case Face::South: return {{-1, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, -1, 0}};
    case Face::East: return {{0, 0, 1}, {0, 0, -1}, {0, 1, 0}, {0, -1, 0}};
    case Face::West: return {{0, 0, -1}, {0, 0, 1}, {0, 1, 0}, {0, -1, 0}};
    case Face::Up: return {{-1, 0, 0}, {1, 0, 0}, {0, 0, -1}, {0, 0, 1}};
    default: return {{-1, 0, 0}, {1, 0, 0}, {0, 0, 1}, {0, 0, -1}};
    }
}
// The block a face looks out at.
inline constexpr Offset normal(Face face) {
    switch (face) {
    case Face::Down: return {0, -1, 0};
    case Face::Up: return {0, 1, 0};
    case Face::North: return {0, 0, -1};
    case Face::South: return {0, 0, 1};
    case Face::West: return {-1, 0, 0};
    default: return {1, 0, 0};
    }
}
inline constexpr Offset plus(Offset a, Offset b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
// Glass blocks: clear, stained (each color is its own block, so only the same
// color connects) and tinted. Panes take the pane path below.
inline bool connects(std::string_view identifier) {
    return identifier.ends_with("glass");
}
inline bool connectsPane(std::string_view identifier) {
    return identifier.ends_with("glass_pane");
}

// Joined sides in the texture image's own orientation (x to the right, y down).
enum Side : unsigned { Left = 1, Right = 2, Top = 4, Bottom = 8 };
using Mask = unsigned;
// Corners of a face whose two sides are joined but whose diagonal neighbor is
// not the same block: the borders of the two neighbors meet there, and the
// face keeps the frame's corner texel so the border turns without a gap
// (L-208, maintainer 2026-10-18: always, no option).
enum CornerBit : unsigned { TopLeft = 16, TopRight = 32, BottomLeft = 64, BottomRight = 128 };
inline constexpr std::array<std::pair<CornerBit, Mask>, 4> cornerSides{{
    {TopLeft, Top | Left}, {TopRight, Top | Right}, {BottomLeft, Bottom | Left}, {BottomRight, Bottom | Right}}};
// A corner bit is only meaningful with both of its sides joined.
inline bool validMask(Mask mask) {
    for (auto [bit, both] : cornerSides)
        if ((mask & bit) && (mask & both) != both) return false;
    return mask < 256;
}
// How many source texels a block drops on each joined side of a face, out of
// 16 (scaled for larger textures), and the texel each refilled strip starts
// from (-1: the texels right inside the border). Glass loses its frame on
// every face; other blocks only on their side faces (L-96 trial, 2026-10-11:
// bookshelves lose the plank column between side-by-side shelves, sandstone
// the light band on top when another sits on it).
struct Rule {
    int left = 1, right = 1, top = 1, bottom = 1;
    bool sidesOnly = false;
    int leftFrom = -1, rightFrom = -1, topFrom = -1, bottomFrom = -1;
    // L-208: the rule also gets sprites that keep the frame's corner texels.
    bool corners = false;
};
// Far away the game samples shrunk copies of the texture that average
// neighboring texels, so a strip right next to the frame let it show again at
// a distance (2026-10-11). Glass drops two texels and refills them from the
// middle. Sandstone copies rock from the middle too (chosen in game).
inline Rule glassRule() { return {2, 2, 2, 2, false, 7, 7, 7, 7, true}; }
inline Rule bookshelfRule() { return {1, 1, 0, 0, true}; }
inline Rule sandstoneRule() { return {0, 0, 4, 0, true, -1, -1, 8, -1}; }
inline std::optional<Rule> ruleFor(std::string_view identifier) {
    if (connects(identifier)) return glassRule();
    if (identifier == "minecraft:bookshelf") return bookshelfRule();
    if (identifier == "minecraft:sandstone" || identifier == "minecraft:red_sandstone") return sandstoneRule();
    return std::nullopt;
}
// The texture files those blocks draw (side textures only for side rules).
// Resource packs replace the files under the same paths.
inline std::string_view fileName(std::string_view path) {
    auto slash = path.find_last_of('/');
    return slash == std::string_view::npos ? path : path.substr(slash + 1);
}
inline std::optional<Rule> ruleForTexture(std::string_view path) {
    auto name = fileName(path);
    if (name == "glass" || name == "tinted_glass" || (name.starts_with("glass_") && name.find("pane") == std::string_view::npos))
        return glassRule();
    if (name == "bookshelf") return bookshelfRule();
    if (name == "sandstone_normal" || name == "red_sandstone_normal") return sandstoneRule();
    return std::nullopt;
}
inline Mask ruleSides(Rule const& rule) {
    return (rule.left > 0 ? Left : 0u) | (rule.right > 0 ? Right : 0u) | (rule.top > 0 ? Top : 0u) | (rule.bottom > 0 ? Bottom : 0u);
}
// Every joined combination a rule needs a sprite for (not the lone face).
inline std::vector<Mask> variantMasks(Rule const& rule) {
    std::vector<Mask> out;
    Mask all = ruleSides(rule);
    for (Mask mask = 1; mask < (rule.corners ? 256u : 16u); ++mask)
        if ((mask & 15 & all) == (mask & 15) && (mask & 15) && validMask(mask)) out.push_back(mask);
    return out;
}
// The source column (row) texel `i` of a joined face shows.
inline int sourceTexel(int i, int size, bool lowJoined, int low, int lowFrom, bool highJoined, int high, int highFrom) {
    int a = lowJoined ? size * low / 16 : 0, b = highJoined ? size * high / 16 : 0;
    if (i < a) return (lowFrom >= 0 ? size * lowFrom / 16 : a) + i;
    if (i >= size - b) return (highFrom >= 0 ? size * highFrom / 16 : size - 2 * b) + (i - (size - b));
    return i;
}
// The sprite for `mask`, from RGBA pixels in rows.
inline std::vector<std::uint32_t> bake(std::uint32_t const* pixels, int width, int height, Rule const& rule, Mask mask) {
    std::vector<std::uint32_t> out(static_cast<size_t>(width) * height);
    Mask sided = mask & 15;
    for (int y = 0; y < height; ++y) {
        int sy = sourceTexel(y, height, sided & Top, rule.top, rule.topFrom, sided & Bottom, rule.bottom, rule.bottomFrom);
        for (int x = 0; x < width; ++x) {
            int sx = sourceTexel(x, width, sided & Left, rule.left, rule.leftFrom, sided & Right, rule.right, rule.rightFrom);
            out[static_cast<size_t>(y) * width + x] = pixels[static_cast<size_t>(sy) * width + sx];
        }
    }
    // One frame texel (one 16th) per kept corner: the neighbors' borders end
    // right beside it, so a wider patch stuck out along both (2026-10-18).
    int cw = std::max(1, width / 16), ch = std::max(1, height / 16);
    for (auto [bit, both] : cornerSides) {
        if (!(mask & bit)) continue;
        int x0 = (both & Left) ? 0 : width - cw, y0 = (both & Top) ? 0 : height - ch;
        for (int y = y0; y < y0 + ch; ++y)
            for (int x = x0; x < x0 + cw; ++x)
                out[static_cast<size_t>(y) * width + x] = pixels[static_cast<size_t>(y) * width + x];
    }
    return out;
}
// Larger textures draw vanilla: their sprites would cost too much atlas space.
inline bool bakeable(int width, int height) { return width >= 16 && width == height && width <= 64; }
inline std::string spriteName(std::string_view path, Mask mask) {
    return "lamium_ct_" + std::string(fileName(path)) + "_" + std::to_string(mask);
}
// Sprites sit beside their base texture, so a texture set's relative names
// resolve the same way.
inline std::string_view directoryOf(std::string_view path) {
    auto slash = path.find_last_of('/');
    return slash == std::string_view::npos ? std::string_view{} : path.substr(0, slash);
}
inline std::string spritePath(std::string_view path, Mask mask) {
    auto directory = directoryOf(path);
    return (directory.empty() ? std::string{} : std::string(directory) + "/") + spriteName(path, mask);
}
// A face's joined sides (toward u0, u1, v0, v1) in image orientation: a face
// whose texture runs backwards shows its u0 side at the image's right.
inline Mask imageMask(bool towardU0, bool towardU1, bool towardV0, bool towardV1, bool reversedU, bool reversedV) {
    Mask mask = 0;
    if (towardU0) mask |= reversedU ? Right : Left;
    if (towardU1) mask |= reversedU ? Left : Right;
    if (towardV0) mask |= reversedV ? Bottom : Top;
    if (towardV1) mask |= reversedV ? Top : Bottom;
    return mask;
}
// The neighbor toward an image side: imageMask's inverse.
inline Offset imageOffset(Sides const& s, Mask side, bool reversedU, bool reversedV) {
    if (side == Left) return reversedU ? s.right : s.left;
    if (side == Right) return reversedU ? s.left : s.right;
    if (side == Top) return reversedV ? s.bottom : s.top;
    return reversedV ? s.top : s.bottom;
}
// The sprite a face shows: the image sides whose neighbor is the same block,
// plus the corners between two joined sides whose diagonal is not. With
// `insideCorners` (L-208 option) a side joins only while the neighbor's own
// face shows: where glass turns inward, that face is hidden behind the other
// wall, and the border stays as the edge of the turn. `same(offset)`: the
// block at that offset from this one is the same block.
template <class Same>
Mask faceMask(Face face, Rule const& rule, bool reversedU, bool reversedV, bool insideCorners, Same&& same) {
    auto s = sides(face);
    Offset out = normal(face);
    Mask mask = 0;
    for (Mask side : {Left, Right, Top, Bottom}) {
        if (!(ruleSides(rule) & side)) continue;
        Offset at = imageOffset(s, side, reversedU, reversedV);
        if (same(at) && !(insideCorners && same(plus(at, out)))) mask |= side;
    }
    if (rule.corners)
        for (auto [bit, both] : cornerSides) {
            if ((mask & both) != both) continue;
            Offset diagonal = plus(imageOffset(s, both & (Left | Right), reversedU, reversedV),
                                   imageOffset(s, both & (Top | Bottom), reversedU, reversedV));
            if (!same(diagonal)) mask |= bit;
        }
    return mask;
}
// Which image side a texture coordinate on a joined edge lies on: the tile's
// low or high end (within half a texel), else none.
inline Mask edgeSide(float value, float low, float high, float texel, Side lowSide, Side highSide) {
    float half = (texel < 0 ? -texel : texel) / 2;
    if (value - low < half && low - value < half) return lowSide;
    if (value - high < half && high - value < half) return highSide;
    return 0;
}

// ---- Glass panes (checked in game 2026-10-11) ----
// A pane shares one glass texture between its center post and its arms, so
// it is adjusted per part on the mesh vanilla built, not through the texture:
// a glass face's side border goes where its block edge touches the same pane;
// its top (bottom) border and the thin top (bottom) face go where the pane
// above (below) has the same part.
enum class Part { Center, East, West, South, North };
inline constexpr int partIndex(Part part) { return static_cast<int>(part); }
// The part a face belongs to, from its extent in block coordinates.
inline Part partOf(float x0, float x1, float z0, float z1) {
    if (x1 > 0.6f) return Part::East;
    if (x0 < 0.4f) return Part::West;
    if (z1 > 0.6f) return Part::South;
    if (z0 < 0.4f) return Part::North;
    return Part::Center;
}
// A pane's parts from its shape boxes in block coordinates: an arm box reaches
// the block edge. The center post is always drawn, though an L's boxes do not
// cover it.
struct Box { float x0 = 0, x1 = 0, z0 = 0, z1 = 0; };
struct Parts {
    bool present = false;
    bool part[5]{};
    bool has(Part p) const { return present && part[partIndex(p)]; }
};
template <class Boxes>
Parts partsOf(Boxes const& boxes) {
    Parts parts;
    parts.present = true;
    parts.part[partIndex(Part::Center)] = true;
    for (Box const& box : boxes) {
        if (box.x1 > 0.9f) parts.part[partIndex(Part::East)] = true;
        if (box.x0 < 0.1f) parts.part[partIndex(Part::West)] = true;
        if (box.z1 > 0.9f) parts.part[partIndex(Part::South)] = true;
        if (box.z0 < 0.1f) parts.part[partIndex(Part::North)] = true;
    }
    return parts;
}
// L-208: the corner an L-shaped pane turns around - one arm on each axis - in
// block coordinates, or none for straight, lone or absent panes.
inline std::optional<std::pair<float, float>> paneCornerPoint(Parts const& parts) {
    if (!parts.present) return std::nullopt;
    if (parts.has(Part::West) == parts.has(Part::East) || parts.has(Part::North) == parts.has(Part::South))
        return std::nullopt;
    return std::pair{parts.has(Part::East) ? 0.437f : 0.562f, parts.has(Part::North) ? 0.562f : 0.437f};
}
// The post faces the corner rewrite applies to: the center part's own quads.
inline bool panePostQuad(Part part, float x0, float x1, float z0, float z1) {
    return part == Part::Center && x0 > 0.4f && x1 < 0.6f && z0 > 0.4f && z1 < 0.6f;
}
// The post face with a corner end slides onto the tile border there: the whole
// face moves by one delta, so the window keeps its width (16 px stays 16 px)
// and the border shows at the corner end. Moving only the corner vertices
// stretched the post (review, 2026-10-18). Away vertices keep their distance
// to the corner ones; false when no vertex sits at the corner (then the quad
// is the mask path's). Only u moves - the border the vertical joins shift on
// v is never touched here.
inline bool paneCornerU(std::array<float, 4> const& x, std::array<float, 4> const& z, std::array<float, 4> const& u,
                        float cornerX, float cornerZ, float uLow, float uHigh, std::array<float, 4>& out) {
    bool any = false, away = false;
    float atU = 0, awayU = 0;
    for (size_t i = 0; i < u.size(); ++i) {
        if (std::abs(x[i] - cornerX) < 0.06f && std::abs(z[i] - cornerZ) < 0.06f) {
            atU = u[i];
            any = true;
        } else {
            awayU = u[i];
            away = true;
        }
    }
    if (!any) return false;
    float delta = (!away || atU > awayU ? uHigh : uLow) - atU;
    for (size_t i = 0; i < u.size(); ++i) out[i] = u[i] + delta;
    return true;
}
}
