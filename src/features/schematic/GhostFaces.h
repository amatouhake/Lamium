#pragma once
#include <array>
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
// L-186: the ghost tint over block textures by its strength in percent,
// and the offsets an outline edge is drawn again at, across the edge, by
// width (thin, normal, thick): game lines are one pixel wide.
inline constexpr std::array<float, 3> ghostTint(int percent) {
    // From white (0%) toward the full tint (100%, the look before 2026-10-15).
    float t = static_cast<float>(std::clamp(percent, 0, 100)) / 100.f;
    return {1.f - .38f * t, 1.f - .15f * t, 1.f};
}
// Half the width of an outline bar `width` 64ths of a block wide; 0 for
// game lines. Copies of a line at small offsets left visible gaps up close
// (2026-10-15), so wider outlines are square bars.
inline constexpr float barHalfWidth(int width) { return width <= 0 ? 0.f : .0078125f * static_cast<float>(width); }
// The share of its cell a ghost is drawn and aimed at (L-424), from the
// size setting in percent.
inline constexpr float ghostScale(int percent) { return percent >= 100 ? 1.f : percent <= 50 ? .5f : static_cast<float>(percent) / 100.f; }
// The other half of a bed, which the game draws as one model from its head:
// `direction` 0-3 is south, west, north, east from foot to head (L-424).
inline constexpr std::array<int, 3> bedPartner(int direction, bool head) {
    constexpr int dx[4] = {0, -1, 0, 1}, dz[4] = {1, 0, -1, 0};
    int const d = direction & 3, sign = head ? -1 : 1;
    return {dx[d] * sign, 0, dz[d] * sign};
}
// Where a ghost's cell lands at `scale`: shrunk about its centre, or for a
// model drawn over two cells (`partner` the offset to the other) about the
// middle of both, so the whole model shrinks as one (L-424).
inline std::pair<std::array<double, 3>, std::array<double, 3>> shrunkCell(std::array<int, 3> cell, std::array<int, 3> partner,
                                                                          double scale) {
    std::array<double, 3> low{}, high{};
    for (int i = 0; i < 3; ++i) {
        double const pivot = cell[i] + .5 + partner[i] * .5;
        low[i] = pivot + (cell[i] - pivot) * scale;
        high[i] = pivot + (cell[i] + 1 - pivot) * scale;
    }
    return {low, high};
}
// Whether a ray from `from` along `along` meets the box [low, high] within
// `reach` blocks: a shrunk ghost is aimed at only where it is drawn, so the
// ray may pass between ghosts (L-424).
inline bool rayMeetsBox(std::array<double, 3> from, std::array<double, 3> along, std::array<double, 3> low,
                        std::array<double, 3> high, double reach) {
    double const length = std::sqrt(along[0] * along[0] + along[1] * along[1] + along[2] * along[2]);
    if (!(length > 0) || !(reach >= 0)) return false;
    double enter = 0, leave = reach;
    for (int i = 0; i < 3; ++i) {
        double const step = along[i] / length;
        if (std::abs(step) < 1e-12) {
            if (from[i] < low[i] || from[i] > high[i]) return false;
            continue;
        }
        double a = (low[i] - from[i]) / step, b = (high[i] - from[i]) / step;
        if (a > b) std::swap(a, b);
        enter = std::max(enter, a);
        leave = std::min(leave, b);
        if (enter > leave) return false;
    }
    return true;
}
// Neighbors' bars share an edge. Coplanar bars of two colors flickered, so a
// stronger mistake's bar is a little wider and encloses the weaker one: wrong
// block (red) over unknown block (orange) over wrong state (yellow) over a
// ghost or missing block (light blue).
inline constexpr int outlineRank(float r, float g, float b) {
    if (b > .5f) return 0;
    if (r > .9f && g < .4f) return 3;
    if (r > .9f && g < .7f) return 2;
    return 1;
}
inline constexpr float rankedBarHalfWidth(int width, int rank) {
    return width <= 0 ? 0.f : barHalfWidth(width) + .002f * static_cast<float>(rank);
}
// An edge by its ends to 1/256 block, so two boxes' copies of one edge match.
// Within a section only the strongest result's copy is drawn; the wider bar
// covers the rest across section borders, where 0.0015 lost to depth precision
// a few blocks away (wrong-state stairs over a missing block, 2026-10-15).
inline std::array<int, 6> edgeKey(std::array<float, 3> p, std::array<float, 3> q) {
    auto at = [](float v) { return static_cast<int>(std::lround(v * 256.f)); };
    return {at(p[0]), at(p[1]), at(p[2]), at(q[0]), at(q[1]), at(q[2])};
}
// The four sides of a square bar around the axis-aligned segment p-q, each
// in both windings (the material may cull either): 32 corners, quads.
inline std::vector<std::array<float, 3>> barQuads(std::array<float, 3> p, std::array<float, 3> q, float half) {
    std::vector<std::array<float, 3>> out;
    float dx = std::abs(q[0] - p[0]), dy = std::abs(q[1] - p[1]), dz = std::abs(q[2] - p[2]);
    int axis = dx >= dy && dx >= dz ? 0 : dy >= dz ? 1 : 2;
    int a = (axis + 1) % 3, b = (axis + 2) % 3;
    constexpr float corner[4][2] = {{1, 1}, {1, -1}, {-1, -1}, {-1, 1}};
    auto at = [&](std::array<float, 3> base, int c) {
        base[static_cast<size_t>(a)] += corner[c][0] * half;
        base[static_cast<size_t>(b)] += corner[c][1] * half;
        return base;
    };
    out.reserve(32);
    for (int side = 0; side < 4; ++side) {
        int c1 = side, c2 = (side + 1) % 4;
        std::array<std::array<float, 3>, 4> quad{at(p, c1), at(q, c1), at(q, c2), at(p, c2)};
        out.insert(out.end(), quad.begin(), quad.end());
        out.insert(out.end(), quad.rbegin(), quad.rend());
    }
    return out;
}
// The game's face (down, up, north, south, west, east) of the neighbor on
// `side` that looks back at this cell.
inline constexpr unsigned char facingToward(int side) {
    constexpr unsigned char back[6] = {5, 4, 1, 0, 3, 2};
    return side >= 0 && side < 6 ? back[side] : 0;
}
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
// A see-through ghost's face on a side where the neighbor ghost (this
// placement's or another's) also has a face: the material draws both sides,
// so two faces there fought (L-178 follow-up). Against a solid ghost the
// see-through face goes; two of the same kind drop both, as the game draws
// no face between two glass blocks; two different see-through kinds keep
// one, the one on the positive side (`side` odd).
inline bool dropBlendedFace(bool neighborReaches, bool neighborBlended, bool sameType, int side) {
    if (!neighborReaches) return false;
    if (!neighborBlended || sameType) return true;
    return side % 2 == 0;
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
