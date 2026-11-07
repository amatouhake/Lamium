#include "features/visuals/BlockHighlight.h"
#include <array>
#include <cmath>
#include <string>
#include <vector>
void check(bool, char const*);
void blockHighlightTests() {
    using namespace lamium::visuals::highlight;
    std::vector<Entry> list;
    check(toggle(list, "minecraft:diamond_ore") == Change::Added && list.size() == 1 && list[0].color == 0,
          "the first designated type takes the first color");
    check(toggle(list, "minecraft:ancient_debris") == Change::Added && list[1].color == 1,
          "the next type takes the color after the last one");
    check(toggle(list, "minecraft:diamond_ore") == Change::Removed && list.size() == 1
              && list[0].name == "minecraft:ancient_debris",
          "designating a listed type again removes it");
    check(toggle(list, "") == Change::Invalid && list.size() == 1, "an empty name is refused");
    list.back().color = paletteSize - 1;
    toggle(list, "minecraft:emerald_ore");
    check(list.back().color == 0, "colors wrap around the palette");
    std::vector<Entry> full;
    for (size_t i = 0; i < maxEntries; ++i) toggle(full, "minecraft:block_" + std::to_string(i));
    check(toggle(full, "minecraft:one_more") == Change::Full && full.size() == maxEntries, "the list is bounded");
    check(toggle(full, "minecraft:block_0") == Change::Removed, "a full list still removes");

    std::vector<Entry> loaded{{"minecraft:a", 40}, {"", 1}, {"minecraft:a", 2}, {"minecraft:b", -3}};
    normalize(loaded);
    check(loaded.size() == 2 && loaded[0] == Entry{"minecraft:a", paletteSize - 1} && loaded[1] == Entry{"minecraft:b", 0},
          "loading drops empty and repeated names and clamps colors");

    std::vector<lamium::overlay::CellFace> faces;
    appendFaces(faces, Mark{{1, 2, 3}, 0b000011});
    check(faces.size() == 2 && faces[0].face == lamium::overlay::Face::Down && faces[1].face == lamium::overlay::Face::Up,
          "facing bits 0 and 1 are the bottom and top faces");
    faces.clear();
    appendFaces(faces, Mark{{0, 0, 0}, 0b110100});
    check(faces.size() == 3 && faces[0].face == lamium::overlay::Face::West && faces[1].face == lamium::overlay::Face::East
              && faces[2].face == lamium::overlay::Face::North,
          "facing bits 2, 4 and 5 are north, west and east");
    check(pulse(0) < .01f && pulse(pulsePeriod / 2) > .99f && pulse(pulsePeriod) < .01f && pulse(-pulsePeriod / 2) > .99f,
          "the pulse rises from nothing to full and back each period");
    auto hueRed = hueColor(0, 1), green = hueColor(1.f / 3, 1), blue = hueColor(2.f / 3, 1);
    check(hueRed == std::array<float, 3>{1, 0, 0} && green == std::array<float, 3>{0, 1, 0} && blue == std::array<float, 3>{0, 0, 1},
          "hues 0, 1/3 and 2/3 are red, green and blue");
    check(hueColor(1, 1) == hueRed && hueColor(.5f, .5f)[0] == .5f, "the hue wraps and saturation lifts the low channel");
    lamium::overlay::Cell origin{0, 0, 0};
    float h0 = rainbowHue(0, origin), h1 = rainbowHue(rainbowPeriod / 4, origin);
    check(h0 == 0 && std::abs(h1 - .25f) < 1e-5f, "the hue turns once per period");
    check(rainbowHue(0, {1, 0, 0}) != rainbowHue(0, origin) && rainbowHue(0, {1, 0, 0}) == rainbowHue(0, {0, 0, 1}),
          "neighbouring blocks differ, along x + y + z");
    check(rainbowHue(0, {-1000, 5, 7}) >= 0 && rainbowHue(0, {-1000, 5, 7}) < 1, "the hue stays in 0..1 far from the origin");

}
