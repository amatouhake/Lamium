#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

// The schematic screen's 3D preview (BACKLOG L-114): how a structure is seen
// and in which order its blocks are drawn. The UI pass has no usable depth
// for meshes, so blocks are drawn far to near and every block keeps only the
// faces turned to the viewer (orthographic view, unit cells: walking each
// axis from its far end is a correct painter's order).
namespace lamium::schematic::preview {
struct View {
    float yaw = 35;   // degrees around the vertical axis; 0 looks from the south (+z)
    float pitch = 30; // degrees above the horizon
    float zoom = 1;   // 1 fits the whole structure; draw order does not depend on it
    int peel = 0;     // layers taken off from the viewer's side
    int peelAxis = -1, peelSign = 1; // the side peeled, fixed when peeling starts; -1 follows the view
};
struct Eye {
    float x = 0, y = 0, z = 0; // unit vector from the structure toward the viewer
};
inline Eye eye(View view) {
    constexpr float toRadians = 3.14159265f / 180;
    float yaw = view.yaw * toRadians, pitch = view.pitch * toRadians;
    return {std::sin(yaw) * std::cos(pitch), std::sin(pitch), std::cos(yaw) * std::cos(pitch)};
}
// +1 walks an axis upward, -1 downward; the far end comes first.
struct Order {
    int x = 1, y = 1, z = 1;
    bool operator==(Order const&) const = default;
};
inline Order drawOrder(View view) {
    auto e = eye(view);
    return {e.x >= 0 ? 1 : -1, e.y >= 0 ? 1 : -1, e.z >= 0 ? 1 : -1};
}
// A face with this outward axis normal is seen from the viewer.
inline bool facesViewer(Order order, int nx, int ny, int nz) {
    return nx * order.x + ny * order.y + nz * order.z > 0;
}
// The cells a viewer can see some face of: occupied, with at least one
// neighbor (or the outside) that does not cover it. In draw order.
template <class Occupied, class Covers>
std::vector<std::uint32_t> visibleCells(int sx, int sy, int sz, Order order, Occupied&& occupied, Covers&& covers) {
    std::vector<std::uint32_t> out;
    auto at = [&](int x, int y, int z) {
        return x >= 0 && y >= 0 && z >= 0 && x < sx && y < sy && z < sz && covers(x, y, z);
    };
    for (int i = 0; i < sx; ++i) {
        int x = order.x > 0 ? i : sx - 1 - i;
        for (int j = 0; j < sy; ++j) {
            int y = order.y > 0 ? j : sy - 1 - j;
            for (int k = 0; k < sz; ++k) {
                int z = order.z > 0 ? k : sz - 1 - k;
                if (!occupied(x, y, z)) continue;
                if (at(x + 1, y, z) && at(x - 1, y, z) && at(x, y + 1, z) && at(x, y - 1, z) && at(x, y, z + 1) && at(x, y, z - 1)) continue;
                out.push_back(static_cast<std::uint32_t>((x * sy + y) * sz + z));
            }
        }
    }
    return out;
}
// Where a model point lands: right, down (UI y) and toward the viewer, for a
// structure point relative to its center.
struct Projected {
    float right = 0, down = 0, toward = 0;
};
inline Projected project(View view, float x, float y, float z) {
    auto e = eye(view);
    // Right is the horizontal axis perpendicular to the eye; up completes it.
    float rx = e.z, rz = -e.x, length = std::sqrt(rx * rx + rz * rz);
    if (length > 0) { rx /= length; rz /= length; }
    float ux = e.y * rz, uy = e.z * rx - e.x * rz, uz = -e.y * rx; // eye x right
    return {x * rx + z * rz, -(x * ux + y * uy + z * uz), x * e.x + y * e.y + z * e.z};
}
// Layers peeled off from the side the view looks down on: from the top when
// looking down steeply, else from the horizontal side nearest the viewer.
// A cell stays when its coordinate on `axis` is <= limit (sign +1) or >=
// limit (sign -1).
struct Cut {
    int axis = -1; // none
    int sign = 1, limit = 0;
    bool operator==(Cut const&) const = default;
    bool keeps(int x, int y, int z) const {
        if (axis < 0) return true;
        int c = axis == 0 ? x : axis == 1 ? y : z;
        return sign > 0 ? c <= limit : c >= limit;
    }
};
inline Cut cutFor(View view, int sx, int sy, int sz, int peel) {
    if (peel <= 0) return {};
    auto e = eye(view);
    int axis = std::abs(e.y) >= std::max(std::abs(e.x), std::abs(e.z)) * .9f ? 1 : std::abs(e.x) >= std::abs(e.z) ? 0 : 2;
    float toward = axis == 0 ? e.x : axis == 1 ? e.y : e.z;
    // Fixed once peeling started, so turning to look at the cut keeps it.
    if (view.peelAxis >= 0) { axis = view.peelAxis; toward = static_cast<float>(view.peelSign); }
    int size = axis == 0 ? sx : axis == 1 ? sy : sz;
    peel = std::min(peel, size - 1);
    // The viewer is on the + side: keep the low end.
    return toward >= 0 ? Cut{axis, 1, size - 1 - peel} : Cut{axis, -1, peel};
}
inline int layersAlong(Cut cut, int sx, int sy, int sz) { return cut.axis == 0 ? sx : cut.axis == 1 ? sy : cut.axis == 2 ? sz : 0; }
// Scale (UI units per block) fitting the structure's bounding sphere in a box.
inline float fitScale(int sx, int sy, int sz, float width, float height) {
    float diagonal = std::sqrt(float(sx * sx + sy * sy + sz * sz));
    return diagonal > 0 ? std::min(width, height) * .92f / diagonal : 1.f;
}
// The largest zoom at which the structure, seen from `view`, still fits the
// box: the preview cannot be clipped to its box, so it never grows past it.
inline float maxZoom(View view, int sx, int sy, int sz, float width, float height) {
    float right = 0, down = 0;
    for (int k = 0; k < 8; ++k) {
        auto p = project(view, (k & 1 ? sx : -sx) / 2.f, (k & 2 ? sy : -sy) / 2.f, (k & 4 ? sz : -sz) / 2.f);
        right = std::max(right, std::abs(p.right));
        down = std::max(down, std::abs(p.down));
    }
    float fit = fitScale(sx, sy, sz, width, height);
    if (right <= 0 || down <= 0 || fit <= 0) return 1.f;
    return std::max(1.f, std::min(width / (2 * right), height / (2 * down)) * .98f / fit);
}
} // namespace lamium::schematic::preview
