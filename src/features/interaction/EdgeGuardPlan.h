#pragma once
#include <cmath>
#include <utility>

namespace lamium::interaction {
// Edge Guard (BACKLOG L-40): shorten a horizontal move until the player's
// feet stay supported, the way sneaking stops at an edge. `supported(dx, dz)`
// answers whether ground lies within the step height under the feet shifted
// by (dx, dz). Each axis is tried alone first so sliding along an edge keeps
// the unaffected component.
template <class Supported>
std::pair<double, double> guardEdge(double dx, double dz, Supported&& supported, double increment = 0.05) {
    auto shrink = [increment](double v) { return std::abs(v) <= increment ? 0.0 : v - std::copysign(increment, v); };
    if (!std::isfinite(dx) || !std::isfinite(dz) || !(increment > 0)) return {dx, dz};
    while (dx != 0 && !supported(dx, 0.0)) dx = shrink(dx);
    while (dz != 0 && !supported(0.0, dz)) dz = shrink(dz);
    while (dx != 0 && dz != 0 && !supported(dx, dz)) {
        dx = shrink(dx);
        dz = shrink(dz);
    }
    return {dx, dz};
}
}
