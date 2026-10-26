#include "features/camera/FreeCameraInsideBlock.h"
#include <initializer_list>
void check(bool, char const*);

void freeCameraInsideBlockTests() {
    using namespace lamium::camera::insideBlock;
    for (bool vanilla : {false, true}) {
        check(drawsInsideCubes(vanilla, false) == vanilla,
            "every other frame keeps the game's own inside-a-block answer");
        check(!drawsInsideCubes(vanilla, true),
            "a FreeCamera frame never draws the inside-a-block effect");
    }
    check(!drawsInsideCubes(false, true) && !drawsInsideCubes(false, false),
        "suppression never invents the effect where the game did not draw it");
    check(suppress(true, true), "the primary view during a FreeCamera frame is suppressed");
    check(!suppress(false, true) && !suppress(true, false) && !suppress(false, false),
        "split screen and frames FreeCamera does not own stay vanilla");
}
