#include "features/camera/DetachedLookState.h"
#include "features/camera/LookRotation.h"
#include <cmath>
#include <limits>
#include <stdexcept>

void detachedLookTests() {
    auto require = [](bool condition, char const* message) {
        if (!condition) throw std::runtime_error(message);
    };
    lamium::DetachedLookState look;
    require(!look.turn(1, 1) && !look.snapshot(), "inactive look must not accept input");
    require(look.begin(20, 170), "start detached look");
    require(look.turn(15, 30), "route detached rotation");
    auto pose = look.snapshot();
    require(pose && pose->pitch == 35 && pose->yaw == -160, "wrap yaw across boundary");
    require(!look.begin(0, 0) && look.snapshot()->pitch == 35, "repeat must preserve view");
    look.turn(1000, 0);
    require(look.snapshot()->pitch == 90, "upward pitch limit");
    look.turn(-2000, 0);
    require(look.snapshot()->pitch == -90, "downward pitch limit");
    look.cancel();
    require(!look.snapshot() && !look.turn(1, 1), "cancel discards pose");
    require(!look.begin(-10, 725), "held repeat cannot reactivate after cancellation");
    look.release();
    require(look.begin(-10, 725) && look.snapshot()->yaw == 5, "new session uses fresh pose");
    auto maximum = std::numeric_limits<float>::max();
    look.turn(maximum, maximum);
    look.turn(maximum, maximum);
    pose = look.snapshot();
    require(pose && std::isfinite(pose->yaw) && std::abs(pose->yaw) <= 180,
        "finite extreme input cannot poison view");
    require(!look.turn(std::numeric_limits<float>::quiet_NaN(), 0) && !look.snapshot(),
        "invalid input cancels override");
    require(!look.begin(0, 0), "invalid turn requires release before reactivation");
    look.release();
    require(!look.begin(0, std::numeric_limits<float>::infinity()) && !look.snapshot(),
        "invalid initial pose cannot activate");
    require(!look.begin(0, 0), "invalid initial input cannot recover through key repeat");
    look.release();
    require(look.begin(0, 0, 42) && look.retainOwner(42), "session retains its player identity");
    require(!look.begin(10, 10, 43), "repeat cannot replace session ownership");
    require(!look.retainOwner(43) && !look.snapshot(), "player replacement cancels detached view");
    require(!look.turn(5, 5), "replacement cannot inherit detached input");
    require(!look.begin(10, 20, 43), "owner replacement cannot reactivate through repeat");
    look.release();
    require(look.begin(10, 20, 43) && look.retainOwner(43), "fresh activation accepts replacement player");
    look.cancel();
    require(!look.retainOwner(43), "cancelled owner cannot reactivate session");
    look.release();
    look.release();
    require(look.begin(0, 0, 43), "duplicate releases keep a fresh press available");
    look.release();
    require(look.begin(80, 0), "start while already looking near a pole");
    look.turn(30, 0);
    require(look.snapshot()->pitch == 90 && look.snapshot()->initialPitch == 80,
        "pitch clamps in the initial player's frame, not relative to zero");
    look.turn(-180, 0);
    require(look.snapshot()->pitch == -90, "full pitch range remains reachable");

    for (float base : {-90.f, -60.f, 0.f, 45.f, 90.f}) {
        auto identity = lamium::lookRotation(base, base, 0);
        for (int i = 0; i < 9; ++i)
            require(std::abs(identity[i] - (i % 4 == 0 ? 1.f : 0.f)) < 1e-5f,
                "activation must not jump at a nonzero initial pitch");
        for (float pitch : {-90.f, -30.f, 0.f, 70.f, 90.f}) {
            for (float yaw : {-180.f, -90.f, 0.f, 45.f, 180.f}) {
                auto correction = lamium::lookRotation(base, pitch, yaw);
                constexpr double rad = 3.14159265358979323846 / 180;
                double cb = std::cos(base * rad), sb = std::sin(base * rad);
                double cp = std::cos(pitch * rad), sp = std::sin(pitch * rad);
                double cy = std::cos(yaw * rad), sy = std::sin(yaw * rad);
                double initial[] = {1,0,0, 0,cb,-sb, 0,sb,cb};
                double expected[] = {cy,0,sy, sp*sy,cp,-sp*cy, -cp*sy,sp,cp*cy};
                for (int row = 0; row < 3; ++row) for (int col = 0; col < 3; ++col) {
                    double actual = 0;
                    for (int k = 0; k < 3; ++k) actual += correction[row*3+k] * initial[k*3+col];
                    require(std::abs(actual - expected[row*3+col]) < 1e-5,
                        "yaw/pitch result must be independent of starting pitch");
                }
            }
        }
    }
}
