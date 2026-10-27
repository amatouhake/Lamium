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

    using lamium::interaction::guardInput, lamium::interaction::worldMove, lamium::interaction::inputMove;
    auto [fx, fz] = worldMove(0, 1, 0);
    auto [wx, wz] = worldMove(0, 1, 90);
    auto [lx, lz] = worldMove(1, 0, 0);
    check(std::abs(fx) < 1e-9 && std::abs(fz - 1) < 1e-9 && std::abs(wx + 1) < 1e-9 && std::abs(wz) < 1e-9
          && std::abs(lx - 1) < 1e-9, "forward faces +z at yaw 0 and -x at yaw 90; strafe left is +x at yaw 0");
    auto [bs, bf] = inputMove(worldMove(.3, -.7, 37).first, worldMove(.3, -.7, 37).second, 37);
    check(std::abs(bs - .3) < 1e-9 && std::abs(bf + .7) < 1e-9, "input and world directions convert back");
    constexpr double f = .546, a = .1;
    // Simulate the server: velocity keeps `f`, input adds `a`; the feet must
    // never leave the ground while the input walks into the edge.
    auto walk = [&](double start, double vx, double input, int ticks) {
        double x = start;
        for (int t = 0; t < ticks; ++t) {
            auto at = x;
            auto [gx, gz] = guardInput(input, 0, vx, 0, a, f, [&](double dx, double) { return 0.2 + at + dx < 1.0; });
            (void)gz;
            vx = vx * f + gx * a;
            x += vx;
            if (0.2 + x >= 1.0) return x;
        }
        return x;
    };
    double end = walk(-3, .216, 1, 80);
    check(0.2 + end < 1.0 && 0.2 + end > .9, "walking into an edge stops on the ground, at the edge");
    double slow = walk(-.5, .02, 1, 120);
    check(0.2 + slow < 1.0 && 0.2 + slow > .95, "creeping toward an edge reaches it");
    // Braking: right at the limit and still sliding (as seen while bridging
    // on BDS), the brake must not push the player back.
    double furthest = -10, px = .76, v = .05;
    for (int t = 0; t < 60; ++t) {
        auto at = px;
        auto [gx, gz] = guardInput(0, 0, v, 0, a, f, [&](double dx, double) { return 0.2 + at + dx < 1.0; });
        (void)gz;
        v = v * f + gx * a;
        px += v;
        furthest = std::max(furthest, px);
    }
    check(0.2 + furthest < 1.0 && furthest - px < .02, "braking at an edge does not push the player back");
    double sprint = walk(-3, .28, 1.3, 80);
    check(0.2 + sprint < 1.0, "a faster approach still stops on the ground");
    auto [awayX, awayZ] = guardInput(-1, 0, -.2, 0, a, f, ground);
    check(awayX == -1 && awayZ == 0, "walking away from the edge keeps the input");
    auto [alongX, alongZ] = guardInput(1, 1, .2, 0, a, f, [](double dx, double) { return 0.6 + dx < 1.0; });
    check(0.6 + (.2 * f + alongX * a) / (1 - f) < 1.0 && alongX < 1 && alongZ > .5,
          "walking diagonally at an edge keeps the parallel input and only a safe share toward the edge");
    using lamium::interaction::stepBack;
    // Feet at x in [0.8, 1.4], only touching ground that ends at x = 0.8.
    auto touching = [](double dx, double) { return 0.8 + dx < 0.8 - 1e-9; };
    auto back = stepBack(touching);
    check(back && back->first < 0 && back->second == 0, "feet only touching an edge step back onto the block");
    check(!stepBack([](double, double) { return false; }), "no support nearby gives no step back");
    using lamium::interaction::checkReplayTarget, lamium::interaction::targetCheckPeriod;
    check(checkReplayTarget(false, 1) && checkReplayTarget(false, targetCheckPeriod - 1),
        "the replay target is derived on every call until the hook is attached");
    check(!checkReplayTarget(true, 1) && !checkReplayTarget(true, targetCheckPeriod - 1),
        "an attached replay hook is not re-derived on every call");
    check(checkReplayTarget(true, targetCheckPeriod) && checkReplayTarget(true, 2 * targetCheckPeriod),
        "the attached replay hook is re-derived once every period");
    using lamium::interaction::mergePushesOff, lamium::interaction::Box;
    // Feet at 67.5 on a bottom slab in cell (-84, 67, 81), overlapping it by
    // 0.03 in x (seen on BDS): a merge pushes the player off.
    Box hanging{-84.57, 67.5, 80.817, -83.97, 69.3, 81.417};
    check(mergePushesOff(hanging, -84, 67, 81), "merging the slab the player barely stands on is refused");
    Box centered{-83.8, 67.5, 81.2, -83.2, 69.3, 81.8};
    check(!mergePushesOff(centered, -84, 67, 81), "merging the slab under the player's center lifts them and is allowed");
    Box beside{-85.6, 67.5, 81.2, -85.0, 69.3, 81.8};
    check(!mergePushesOff(beside, -84, 67, 81), "a merge in a cell the player does not overlap is allowed");
    Box above{-84.57, 68.0, 80.817, -83.97, 69.8, 81.417};
    check(!mergePushesOff(above, -84, 67, 81), "standing on top of the cell does not count as inside it");
    auto [stillX, stillZ] = guardInput(0, 0, 0, 0, a, f, ground);
    check(stillX == 0 && stillZ == 0, "standing still stays still");
}
