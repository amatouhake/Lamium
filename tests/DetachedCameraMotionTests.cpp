#include "features/camera/DetachedCameraMotion.h"
#include <limits>
void check(bool, char const*);
void detachedCameraMotionTests() {
    using Motion = lamium::DetachedCameraMotion;
    constexpr Motion::Vector right{1,0,0}, up{0,1,0}, forward{0,0,1};
    Motion motion;
    check(!motion.begin(0), "detached movement requires an owner");
    check(motion.begin(1) && !motion.begin(1), "repeated activation retains the same session");
    check(motion.advance(1,{1,1,1},right,up,forward,10,.1), "diagonal camera input accepted");
    auto offset = *motion.snapshot();
    check(std::abs(std::hypot(offset[0],offset[1],offset[2])-1) < 1e-9,
          "three-axis movement has the configured speed");
    motion.cancel();
    check(!motion.snapshot(), "cancellation discards camera displacement");
    motion.begin(1);
    motion.advance(1,{0,0,1},right,up,forward,10,60);
    check((*motion.snapshot())[2] == 1, "stalls cannot cause a delayed movement burst");
    check(!motion.advance(2,{1,0,0},right,up,forward,10,.1) && !motion.snapshot(),
          "player replacement cancels camera movement");
    motion.begin(2);
    motion.advance(2,{.5,0,0},right,up,forward,10,.1);
    check((*motion.snapshot())[0] == .5, "analog movement retains subunit magnitude");
    check(!motion.advance(2,{0,0,0},right,up,forward,10,std::numeric_limits<double>::infinity())
          && !motion.snapshot(), "invalid timing discards the session");
    motion.begin(2);
    motion.advance(2,{0,0,1},forward,up,Motion::Vector{-1,0,0},10,.1);
    check((*motion.snapshot())[0] == -1 && (*motion.snapshot())[2] == 0,
          "movement follows the supplied camera orientation");
    motion.cancel();
    motion.begin(3);
    check(motion.shift(3, Motion::Vector{0, 0, -4}), "third-person eye seeds the session");
    check((*motion.snapshot())[2] == -4, "seeded offset persists without input");
    check(!motion.shift(4, Motion::Vector{1, 0, 0}) && !motion.snapshot(),
          "seeding rejects a replaced owner");
}
