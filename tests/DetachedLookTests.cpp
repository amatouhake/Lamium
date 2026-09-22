#include "features/camera/DetachedLookState.h"
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
    require(look.begin(-10, 725) && look.snapshot()->yaw == 5, "new session uses fresh pose");
    auto maximum = std::numeric_limits<float>::max();
    look.turn(maximum, maximum);
    look.turn(maximum, maximum);
    pose = look.snapshot();
    require(pose && std::isfinite(pose->yaw) && std::abs(pose->yaw) <= 180,
        "finite extreme input cannot poison view");
    require(!look.turn(std::numeric_limits<float>::quiet_NaN(), 0) && !look.snapshot(),
        "invalid input cancels override");
    require(!look.begin(0, std::numeric_limits<float>::infinity()) && !look.snapshot(),
        "invalid initial pose cannot activate");
}
