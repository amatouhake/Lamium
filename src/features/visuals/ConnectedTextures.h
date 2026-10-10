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
// color connects) and tinted. Panes are another shape and are not handled.
inline bool connects(std::string_view identifier) {
    return identifier.ends_with("glass");
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
}
