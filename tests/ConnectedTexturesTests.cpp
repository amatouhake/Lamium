#include "features/visuals/ConnectedTextures.h"
#include <cmath>
#include <vector>
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
    check(near(big.u0, 0.25f + 0.25f / 32 * 2), "a 32-pixel texture loses the same share of the face");
    check(ruleFor("minecraft:glass") && !ruleFor("minecraft:glass")->sidesOnly && ruleFor("minecraft:bookshelf")
              && ruleFor("minecraft:bookshelf")->sidesOnly && ruleFor("minecraft:bookshelf")->top == 0
              && ruleFor("minecraft:sandstone")->top == 4 && ruleFor("minecraft:sandstone")->left == 0
              && !ruleFor("minecraft:stone") && !ruleFor("minecraft:chiseled_sandstone"),
          "rules per block");
    auto shelf = trim(glass, 16, 16, {true, true, true, true}, *ruleFor("minecraft:bookshelf"));
    check(near(shelf.u0, 0.25f + 0.25f / 16) && near(shelf.v0, 0.5f) && near(shelf.v1, 0.75f), "bookshelves only lose side columns");
    auto rock = trim(glass, 16, 16, {false, false, true, true}, *ruleFor("minecraft:sandstone"));
    check(near(rock.v0, 0.5f + 0.25f / 4) && near(rock.v1, 0.75f), "sandstone drops its top band under another"); 
    // Panes (geometry dumped in game 2026-10-11).
    check(connectsPane("minecraft:glass_pane") && connectsPane("minecraft:lime_stained_glass_pane") && !connectsPane("minecraft:glass"),
          "panes take the pane path");
    check(partOf(0.437f, 0.563f, 0.437f, 0.563f) == Part::Center && partOf(0.563f, 1.f, 0.437f, 0.563f) == Part::East
              && partOf(0.f, 0.437f, 0.437f, 0.563f) == Part::West && partOf(0.437f, 0.563f, 0.563f, 1.f) == Part::South
              && partOf(0.563f, 0.563f, 0.f, 0.437f) == Part::North && partOf(0.437f, 0.563f, 0.563f, 0.563f) == Part::Center,
          "faces belong to the center post or the arm they reach into");
    auto corner = partsOf(std::vector<Box>{{0.5f, 1.f, 0.438f, 0.562f}, {0.438f, 0.562f, 0.f, 0.5f}});
    check(corner.has(Part::Center) && corner.has(Part::East) && corner.has(Part::North) && !corner.has(Part::West)
              && !corner.has(Part::South),
          "an L corner has its two arms and the center its boxes leave out");
    auto post = partsOf(std::vector<Box>{{0.438f, 0.562f, 0.438f, 0.562f}});
    check(post.has(Part::Center) && !post.has(Part::East) && !post.has(Part::North), "a lone pane is only its post");
    auto straight = partsOf(std::vector<Box>{{0.f, 1.f, 0.438f, 0.562f}});
    check(straight.has(Part::East) && straight.has(Part::West), "a straight pane has both arms");
    check(!Parts{}.has(Part::Center), "no pane, no parts");
    check(near(inward(0.5f, 0.5f, 0.6f, 0.01f), 0.51f) && near(inward(0.6f, 0.5f, 0.6f, -0.01f), 0.59f)
              && near(inward(0.55f, 0.5f, 0.6f, 0.01f), 0.55f),
          "border coordinates move inward, inner ones stay");
}
