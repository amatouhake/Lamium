#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

// Which side of its cell a ghost quad lies on (BACKLOG L-93). A quad flat on
// a cell side that touches an opaque ghost is never seen from outside, and
// drawing it fought with the neighbor's face; the glue drops such quads.
namespace lamium::schematic::faces {
struct Vertex { float x, y, z; };
// Sides: 0 west (-x), 1 east (+x), 2 down (-y), 3 up (+y), 4 north (-z), 5 south (+z).
inline constexpr int offsets[6][3] = {{-1,0,0},{1,0,0},{0,-1,0},{0,1,0},{0,0,-1},{0,0,1}};
// Whether a point (the camera) lies beyond a cell's side, on its neighbor's
// side of the plane. Of two touching faces near the camera, the one whose
// cell has the camera beyond that side is the one facing it.
inline bool beyond(int side, int x, int y, int z, double px, double py, double pz) {
    int axis = side / 2;
    double c = axis == 0 ? px : axis == 1 ? py : pz;
    int low = axis == 0 ? x : axis == 1 ? y : z;
    return side % 2 ? c > low + 1 : c < low;
}
// The side every vertex of the quad lies on, or -1 (inside the cell, slanted).
inline int sideOf(std::span<Vertex const> quad, int x, int y, int z, float epsilon = 1e-3f) {
    if (quad.empty()) return -1;
    float low[3]{static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)};
    for (int side = 0; side < 6; ++side) {
        int axis = side / 2;
        float plane = low[axis] + (side % 2 ? 1.f : 0.f);
        bool flat = true;
        for (auto const& v : quad) {
            float c = axis == 0 ? v.x : axis == 1 ? v.y : v.z;
            if (std::abs(c - plane) > epsilon) { flat = false; break; }
        }
        if (flat) return side;
    }
    return -1;
}
// The area of a quad flat on `side`, measured in that side's plane. Quads on
// one side add up to 1 where a block covers the side entirely.
inline float sideArea(std::span<Vertex const> quad, int side) {
    int axis = side / 2;
    auto at = [&](Vertex const& v, int a) { return a == 0 ? v.x : a == 1 ? v.y : v.z; };
    int u = axis == 0 ? 1 : 0, w = axis == 2 ? 1 : 2;
    float twice = 0;
    for (size_t k = 0; k < quad.size(); ++k) {
        auto const& p = quad[k];
        auto const& q = quad[(k + 1) % quad.size()];
        twice += at(p, u) * at(q, w) - at(q, u) * at(p, w);
    }
    return std::abs(twice) * .5f;
}
// Whether a ghost's quad on a side shared with a neighbor is left out. A
// real opaque full block there hides it. Where the neighbor ghost has a face
// in the same plane (`shared`: an opaque full ghost, or one reaching that
// side) and either cell is next to the camera, only the face toward the
// camera stays (`cameraBeyond`: the camera lies past this side), so the
// plane keeps exactly one face; elsewhere an opaque full ghost hides it.
inline bool dropFace(bool realOpaque, bool ghostOpaque, bool shared, bool nearCamera, bool cameraBeyond) {
    if (realOpaque) return true;
    if (shared && nearCamera) return !cameraBeyond;
    return ghostOpaque;
}
// Whether a quad with normal `n` at `center` is seen from behind from `eye`;
// never for a quad without a usable normal. P: any point with x, y, z.
template <class P>
bool facesAway(P const& n, P const& center, P const& eye) {
    if (n.x * n.x + n.y * n.y + n.z * n.z < 1e-12f) return false;
    return (eye.x - center.x) * n.x + (eye.y - center.y) * n.y + (eye.z - center.z) * n.z < 0;
}
// The order to draw a quad list's quads in for blending: farthest from
// `eye` first, by their centers; equally far quads keep their order.
template <class P>
std::vector<std::uint32_t> farToNear(std::span<P const> positions, P const& eye) {
    std::vector<std::pair<float, std::uint32_t>> far;
    for (size_t q = 0; q + 4 <= positions.size(); q += 4) {
        auto const &a = positions[q], &b = positions[q + 1], &c = positions[q + 2], &d = positions[q + 3];
        float dx = (a.x + b.x + c.x + d.x) * .25f - eye.x, dy = (a.y + b.y + c.y + d.y) * .25f - eye.y,
              dz = (a.z + b.z + c.z + d.z) * .25f - eye.z;
        far.push_back({-(dx * dx + dy * dy + dz * dz), static_cast<std::uint32_t>(q / 4)});
    }
    std::stable_sort(far.begin(), far.end());
    std::vector<std::uint32_t> order;
    order.reserve(far.size());
    for (auto const& f : far) order.push_back(f.second);
    return order;
}
inline bool coveredBy(float area) { return area >= 1.f - 1e-3f; }
// Whether two quads have the same corners, in any order or winding: the
// same face emitted twice (front and back), which a material drawing both
// sides shows as two faces fighting in one plane.
inline bool sameQuad(std::span<Vertex const> a, std::span<Vertex const> b, float epsilon = 1e-4f) {
    if (a.size() != b.size()) return false;
    auto near = [&](Vertex const& p, Vertex const& q) {
        return std::abs(p.x - q.x) <= epsilon && std::abs(p.y - q.y) <= epsilon && std::abs(p.z - q.z) <= epsilon;
    };
    for (auto const* from : {&a, &b}) {
        auto const& other = from == &a ? b : a;
        for (auto const& p : *from) {
            bool found = false;
            for (auto const& q : other) found = found || near(p, q);
            if (!found) return false;
        }
    }
    return true;
}
}
