#pragma once
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
inline Uv trim(Uv uv, int imageWidth, int imageHeight, Joined joined) {
    float du = (uv.u1 - uv.u0) / static_cast<float>(imageWidth > 0 ? imageWidth : 16);
    float dv = (uv.v1 - uv.v0) / static_cast<float>(imageHeight > 0 ? imageHeight : 16);
    Uv out = uv;
    if (joined.left) out.u0 += du;
    if (joined.right) out.u1 -= du;
    if (joined.top) out.v0 += dv;
    if (joined.bottom) out.v1 -= dv;
    return out;
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
