#pragma once
#include <optional>
#include <vector>
#include <string_view>

namespace lamium::visuals::connected {
// Connected Textures (BACKLOG L-96): a block face drops its border on the
// sides where the neighbor in the face's plane is the same block, so blocks
// of one kind read as one surface. Pure parts; ConnectedTextures.cpp hooks
// the block tessellator.
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
// Glass blocks: clear, stained (each color is its own block, so only the same
// color connects) and tinted. Panes take the pane path below.
inline bool connects(std::string_view identifier) {
    return identifier.ends_with("glass");
}
inline bool connectsPane(std::string_view identifier) {
    return identifier.ends_with("glass_pane");
}
// The face's texture rectangle with one source texel cut on each connected
// side. The rectangle may run either way; the cut always moves inward.
struct Uv { float u0 = 0, v0 = 0, u1 = 0, v1 = 0; };
struct Joined { bool left = false, right = false, top = false, bottom = false; };
// How many source texels a block drops on each joined side of a face, out of
// 16 (scaled for larger textures). Glass loses its 1-texel frame on every
// face; other blocks only on their side faces (L-96 trial, 2026-10-11:
// bookshelves lose the plank column between side-by-side shelves, sandstone
// the light band on top when another sits on it).
struct Rule {
    int left = 1, right = 1, top = 1, bottom = 1;
    bool sidesOnly = false;
    // Split: draw the face as strips at the texture's own scale, the dropped
    // border replaced by the texels next to it, instead of stretching the rest
    // (trial 2026-10-11, sandstone first).
    bool split = false;
    // The texel row the top strip copies from; -1 is the rows right below
    // the band. Sandstone copies rock from the middle so the copy does not
    // sit next to its source (chosen in game 2026-10-11).
    int topFrom = -1;
};
inline std::optional<Rule> ruleFor(std::string_view identifier) {
    // Every rule draws split since 2026-10-11: cropping stretched the texture,
    // which showed while a block was being placed.
    if (connects(identifier)) return Rule{1, 1, 1, 1, false, true};
    if (identifier == "minecraft:bookshelf") return Rule{1, 1, 0, 0, true, true};
    if (identifier == "minecraft:sandstone" || identifier == "minecraft:red_sandstone") return Rule{0, 0, 4, 0, true, true, 8};
    return std::nullopt;
}
inline Uv trim(Uv uv, int imageWidth, int imageHeight, Joined joined, Rule rule = {}) {
    int width = imageWidth > 0 ? imageWidth : 16, height = imageHeight > 0 ? imageHeight : 16;
    float du = (uv.u1 - uv.u0) / static_cast<float>(width), dv = (uv.v1 - uv.v0) / static_cast<float>(height);
    float sx = width / 16.f, sy = height / 16.f;
    auto scaled = [](int texels, float scale) { return texels > 0 && scale > 1 ? texels * scale : static_cast<float>(texels); };
    Uv out = uv;
    if (joined.left) out.u0 += du * scaled(rule.left, sx);
    if (joined.right) out.u1 -= du * scaled(rule.right, sx);
    if (joined.top) out.v0 += dv * scaled(rule.top, sy);
    if (joined.bottom) out.v1 -= dv * scaled(rule.bottom, sy);
    return out;
}

// A face split into cells for a split rule. s/t run across the face from its
// u0/v0 corner (0) to its u1/v1 corner (1); su/tv are the texture fractions the
// cell shows. A joined side becomes a strip that shows the texels just inside
// the dropped border; the rest of the face keeps its own texels.
struct Cell { float s0 = 0, s1 = 1, t0 = 0, t1 = 1, su0 = 0, su1 = 1, tv0 = 0, tv1 = 1; };
struct Span { float at0, at1, from0, from1; };
inline std::vector<Span> spans(bool lowJoined, int low, bool highJoined, int high, int lowFrom = -1) {
    float a = lowJoined && low > 0 ? low / 16.f : 0, b = highJoined && high > 0 ? high / 16.f : 0;
    std::vector<Span> out;
    float from = lowFrom >= 0 ? lowFrom / 16.f : a;
    if (a > 0) out.push_back({0, a, from, from + a});
    out.push_back({a, 1 - b, a, 1 - b});
    if (b > 0) out.push_back({1 - b, 1, 1 - 2 * b, 1 - b});
    return out;
}
inline std::vector<Cell> splitCells(Joined joined, Rule rule) {
    std::vector<Cell> cells;
    for (auto const& across : spans(joined.left, rule.left, joined.right, rule.right))
        for (auto const& down : spans(joined.top, rule.top, joined.bottom, rule.bottom, rule.topFrom))
            cells.push_back({across.at0, across.at1, down.at0, down.at1, across.from0, across.from1, down.from0, down.from1});
    return cells;
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
// One texture coordinate of a quad moved one texel inward: from the quad's
// low end up, or from its high end down; values inside stay.
inline float inward(float value, float low, float high, float step) {
    float s = step < 0 ? -step : step;
    if (value - low < 1e-6f && low - value < 1e-6f) return value + s;
    if (value - high < 1e-6f && high - value < 1e-6f) return value - s;
    return value;
}
}
