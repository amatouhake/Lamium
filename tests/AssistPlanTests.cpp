#include "features/schematic/AssistPlan.h"
#include <cmath>
void check(bool, char const*);
void assistPlanTests() {
    using namespace lamium::schematic::assist;
    auto near = [](float a, float b) { return std::fabs(a - b) < 1e-3f; };

    for (unsigned char d = 0; d < 6; ++d) {
        auto const& o = offsets[d];
        auto const& back = offsets[opposite(d)];
        check(o[0] == -back[0] && o[1] == -back[1] && o[2] == -back[2], "L-265: a face and its opposite point apart");
    }
    auto side = clickOn(2, 0.75f);
    check(near(side[1], 0.75f) && near(side[2], 0.0f), "L-265: a north face click sits on z = 0 at the given height");
    auto top = clickOn(1, 0.25f);
    check(near(top[1], 1.0f) && near(top[0], 0.5f), "L-265: a top face click is the middle of the top");

    auto ls = looks(30, 45);
    check(!ls.empty() && !ls[0].faked && near(ls[0].pitch, 30) && near(ls[0].yaw, 45), "L-265: the player's own look comes first");
    bool restFaked = true;
    for (size_t i = 1; i < ls.size(); ++i) restFaked = restFaked && ls[i].faked;
    check(restFaked && ls.size() == 13, "L-265: then twelve faked looks (four facings, level, down, up)");
    check(near(wrapYaw(270), -90) && near(wrapYaw(-190), 170) && near(wrapYaw(180), -180), "L-265: yaws wrap into [-180, 180)");

    std::array<bool, 6> onlyBelow{true, false, false, false, false, false};
    auto below = candidates(onlyBelow, 30, 45);
    check(below.size() == ls.size() && below[0].dir == 0 && below[0].face == 1 && !below[0].look.faked,
        "L-265: with only the ground, one top-face click per look, the unfaked one first");
    std::array<bool, 6> wallNorth{false, false, true, false, false, false};
    auto wall = candidates(wallNorth, 0, 0);
    check(wall.size() == 2 * ls.size() && wall[0].face == 3 && near(wall[0].click[1], 0.25f) && near(wall[1].click[1], 0.75f),
        "L-265: a side neighbor is clicked low and high on the face toward the cell");
    std::array<bool, 6> all{true, true, true, true, true, true};
    auto every = candidates(all, 0, 0);
    size_t firstFaked = 0;
    while (firstFaked < every.size() && !every[firstFaked].look.faked) ++firstFaked;
    bool afterAllFaked = true;
    for (size_t i = firstFaked; i < every.size(); ++i) afterAllFaked = afterAllFaked && every[i].look.faked;
    check(firstFaked == 10 && afterAllFaked, "L-265: every unfaked click is tried before any faked look");
    check(candidates({}, 0, 0).empty(), "L-265: no clickable neighbor, no candidate");

    States stair{{"weirdo_direction", "2"}, {"upside_down_bit", "0"}, {"minecraft:corner", "none"}};
    States ghostStair{{"weirdo_direction", "2"}, {"upside_down_bit", "0"}, {"minecraft:corner", "outer_left"}};
    check(placesAs("minecraft:oak_stairs", stair, "minecraft:oak_stairs", ghostStair),
        "L-265: a stair's corner shape is left to the neighbors");
    ghostStair["upside_down_bit"] = "1";
    check(!placesAs("minecraft:oak_stairs", stair, "minecraft:oak_stairs", ghostStair), "L-265: upside down is the placement's");
    check(!placesAs("minecraft:oak_stairs", stair, "minecraft:spruce_stairs", stair), "L-265: another block never counts");
    States door{{"direction", "1"}, {"open_bit", "0"}, {"door_hinge_bit", "0"}, {"upper_block_bit", "0"}};
    States openDoor = door;
    openDoor["open_bit"] = "1";
    check(placesAs("minecraft:wooden_door", door, "minecraft:wooden_door", openDoor), "L-265: an open ghost door is placed closed");
    openDoor["door_hinge_bit"] = "1";
    check(!placesAs("minecraft:wooden_door", door, "minecraft:wooden_door", openDoor), "L-265: the hinge side is the placement's");
    States slab{{"minecraft:vertical_half", "bottom"}};
    check(!placesAs("minecraft:oak_slab", slab, "minecraft:oak_slab", {}), "L-265: a state the ghost lacks is a difference");

    check(firstDifference(stair, ghostStair) == "upside_down_bit 0 not 1" && firstDifference(stair, stair).empty(),
        "L-265: the log names the first state the placement could not make");
    Move forward{0, 1};
    auto back = turnedMove(forward, 0, 180);
    check(near(back.forward, -1) && near(back.strafe, 0), "L-265: turned around, walking on needs the back key");
    auto quarter = turnedMove(forward, 0, 90);
    // Facing +z at yaw 0; at yaw 90 the player faces -x, and +z is on the left.
    check(near(quarter.forward, 0) && near(quarter.strafe, 1), "L-265: a quarter turn makes forward a strafe");
    auto round = turnedMove(turnedMove(Move{0.6f, -0.8f}, 30, 200), 200, 30);
    check(near(round.strafe, 0.6f) && near(round.forward, -0.8f), "L-265: turning there and back gives the move back");
    auto keys = keysOf(Move{0.71f, -0.71f});
    check(keys.left && keys.down && !keys.up && !keys.right, "L-265: keys follow the move vector's signs");
    auto view = viewOf(0, 90);
    check(near(view[0], -1) && near(view[1], 0) && near(view[2], 0), "L-265: yaw 90 looks toward -x");
    check(near(viewOf(90, 0)[1], -1), "L-265: pitch 90 looks straight down");
}
