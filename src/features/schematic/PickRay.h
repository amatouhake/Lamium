#pragma once
#include <cmath>

// What the game's pick means for a ghost pick (BACKLOG L-285). When the view
// ray meets nothing the game picks the block under the player's feet, and
// that fallback is not on the ray: only a hit on the ray is something the
// player aimed at. Pure, so the rules are testable without the game.
namespace lamium::schematic {
// How far off the ray a hit point may sit and still count as on it, in
// blocks. A real hit is the ray's own intersection, off by float rounding
// only; the feet fallback is the block under the player, not a point the
// ray met.
inline constexpr double pickRayTolerance = .05;
// Whether `p` lies on the ray from `s` along `d` (any length) within
// `tolerance` blocks of it. A point behind the start is not on the ray, and
// nothing is when the direction has no length.
inline bool onRay(double sx, double sy, double sz, double dx, double dy, double dz, double px, double py, double pz,
                  double tolerance) {
    double dd = dx * dx + dy * dy + dz * dz;
    double ex = px - sx, ey = py - sy, ez = pz - sz;
    if (!(dd > 0) || !std::isfinite(dd) || !std::isfinite(ex) || !std::isfinite(ey) || !std::isfinite(ez)) return false;
    double along = ex * dx + ey * dy + ez * dz;
    if (along < 0) return false;
    // Squared, so a point exactly at the tolerance is still on the ray.
    double lateral = ex * ex + ey * ey + ez * ez - along * along / dd;
    return lateral <= tolerance * tolerance;
}
// How far along the ray the ghost search may look (L-285): a hit the player
// aimed at shortens it to just before the hit, so a real block in front of a
// ghost still wins; the engine's fallback off the ray leaves the whole pick
// range, so the ghost under the crosshair wins instead of the feet block.
inline double ghostReach(bool aimed, double hitDistance, double pickRange) {
    return aimed ? hitDistance - .05 : pickRange;
}
}
