#include "features/visuals/BlockHighlight.h"
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

    std::uint32_t white = 0xffffffffu;
    check(tintVertex(white, 255, 255, 255) == white, "tinting white toward white changes nothing");
    auto red = tintVertex(white, 255, 0, 0, 1.f);
    check(red == 0xff0000ffu, "a full share replaces the color and keeps alpha (0xAABBGGRR)");
    auto half = tintVertex(0x80000000u, 200, 100, 0, .5f);
    check((half >> 24) == 0x80 && (half & 0xff) == 100 && ((half >> 8) & 0xff) == 50 && ((half >> 16) & 0xff) == 0,
          "a share mixes each channel and keeps alpha");
}
