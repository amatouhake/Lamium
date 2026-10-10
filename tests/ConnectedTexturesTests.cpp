#include "features/visuals/ConnectedTextures.h"
#include <cmath>
void check(bool, char const*);
void connectedTexturesTests() {
    using namespace lamium::visuals::connected;
    check(connects("minecraft:glass") && connects("minecraft:red_stained_glass") && connects("minecraft:tinted_glass"),
          "glass blocks connect");
    check(!connects("minecraft:glass_pane") && !connects("minecraft:red_stained_glass_pane") && !connects("minecraft:stone"),
          "panes and other blocks do not");
    for (auto face : {Face::Down, Face::Up, Face::North, Face::South, Face::West, Face::East}) {
        auto s = sides(face);
        bool opposite = s.left.x == -s.right.x && s.left.y == -s.right.y && s.left.z == -s.right.z
            && s.top.x == -s.bottom.x && s.top.y == -s.bottom.y && s.top.z == -s.bottom.z;
        check(opposite, "left/right and top/bottom are opposite neighbors");
    }
    check(sides(Face::North).top.y == 1 && sides(Face::East).top.y == 1 && sides(Face::Up).top.y == 0,
          "side faces have the block above at their top; horizontal faces stay in their plane");
    auto near = [](float a, float b) { return std::abs(a - b) < 1e-6f; };
    Uv glass{0.25f, 0.5f, 0.5f, 0.75f};
    auto all = trim(glass, 16, 16, {true, true, true, true});
    check(near(all.u0, 0.25f + 0.25f / 16) && near(all.u1, 0.5f - 0.25f / 16) && near(all.v0, 0.5f + 0.25f / 16)
              && near(all.v1, 0.75f - 0.25f / 16),
          "a fully joined face loses one texel on each side");
    auto none = trim(glass, 16, 16, {});
    check(none.u0 == glass.u0 && none.u1 == glass.u1 && none.v0 == glass.v0 && none.v1 == glass.v1, "a lone face is unchanged");
    auto mirrored = trim({0.5f, 0.75f, 0.25f, 0.5f}, 16, 16, {true, false, true, false});
    check(near(mirrored.u0, 0.5f - 0.25f / 16) && near(mirrored.v0, 0.75f - 0.25f / 16), "a reversed rectangle is cut inward");
    auto big = trim(glass, 32, 32, {true, false, false, false});
    check(near(big.u0, 0.25f + 0.25f / 32), "a 32-pixel texture loses one of its own texels");
}
