#include "features/interaction/EdgeGuardPlan.h"
#include <cmath>
void check(bool, char const*);

void edgeGuardTests() {
    using lamium::interaction::guardEdge;
    // Feet span x in [0.2, 0.8] and z in [0.2, 0.8]; ground ends at x = 1 and
    // z = 1, so the feet stay supported while their min corner is below 1.
    auto ground = [](double dx, double dz) { return 0.2 + dx < 1.0 && 0.2 + dz < 1.0; };
    auto [x, z] = guardEdge(0.3, 0.0, ground);
    check(x > 0 && 0.2 + x < 1.0 && z == 0, "a move toward the edge stops before leaving the ground");
    auto [inX, inZ] = guardEdge(-0.3, -0.2, ground);
    check(inX == -0.3 && inZ == -0.2, "a move away from the edge is untouched");
    auto [far, farZ] = guardEdge(2.0, 0.0, ground);
    check(0.2 + far < 1.0 && far >= 0.7, "a long move is shortened to the edge, not to zero");
    auto [slideX, slideZ] = guardEdge(0.9, 0.1, ground);
    check(0.2 + slideX < 1.0 && slideZ == 0.1, "sliding along an edge keeps the parallel component");
    auto [cornerX, cornerZ] = guardEdge(0.9, 0.9, ground);
    check(0.2 + cornerX < 1.0 && 0.2 + cornerZ < 1.0, "a diagonal move stops at a corner");
    auto none = [](double, double) { return false; };
    auto [nx, nz] = guardEdge(0.3, -0.3, none);
    check(nx == 0 && nz == 0, "no support anywhere stops horizontal movement");
    auto [bad, badZ] = guardEdge(NAN, 0.1, ground);
    check(std::isnan(bad) && badZ == 0.1, "non-finite input is passed through");
}
