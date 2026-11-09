#include "features/visuals/BlockHighlight.h"
#include "ui/HighlightLayout.h"
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
    {
        auto length = [](lamium::overlay::Point p) { return std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z); };
        lamium::overlay::Point far{300, -40, 400}, near{.5, 1, -2};
        auto f = throughWall(far), n = throughWall(near);
        check(length(f) < 2.5 && length(n) < 2.5 && length(n) > .25, "through-wall vertices land within a few blocks of the eye");
        check(std::abs(f.x / far.x - f.y / far.y) < 1e-12 && std::abs(f.x / far.x - f.z / far.z) < 1e-12 && f.x / far.x > 0,
              "a through-wall vertex stays on its own ray");
        check(length(n) < length(f), "a nearer face stays nearer through walls");
        check(throughWall({0, 0, 0}) == lamium::overlay::Point{}, "the eye itself does not move");
    }
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
    std::vector<Mark> many{{{10, 0, 0}, 0b111111}, {{1, 0, 0}, 0b000011}, {{5, 0, 0}, 0b000001}, {{-2, 0, 0}, 0b000111}};
    auto all = chooseNearest(many, {0, 0, 0}, 12);
    check(all.count == 4 && !all.capped && many[0].cell.x == 10, "marks that fit are all drawn, order untouched");
    auto near = chooseNearest(many, {0, 0, 0}, 6);
    check(near.capped && near.count == 3 && many[0].cell.x == 1 && many[1].cell.x == -2 && many[2].cell.x == 5,
          "over the limit the nearest marks are kept until their faces reach it");
    lamium::overlay::Cell origin{0, 0, 0};
    float h0 = rainbowHue(0, origin), h1 = rainbowHue(rainbowPeriod / 4, origin);
    check(h0 == 0 && std::abs(h1 - .25f) < 1e-5f, "the hue turns once per period");
    check(rainbowHue(0, {1, 0, 0}) != rainbowHue(0, origin) && rainbowHue(0, {1, 0, 0}) == rainbowHue(0, {0, 0, 1}),
          "neighbouring blocks differ, along x + y + z");
    check(rainbowHue(0, {-1000, 5, 7}) >= 0 && rainbowHue(0, {-1000, 5, 7}) < 1, "the hue stays in 0..1 far from the origin");

    check(shortName("minecraft:diamond_ore") == "diamond_ore" && shortName("mod:ruby_ore") == "mod:ruby_ore",
          "only the vanilla namespace is left out of the shown name");
    check(siblingName("minecraft:diamond_ore") == "minecraft:deepslate_diamond_ore"
              && siblingName("minecraft:deepslate_diamond_ore") == "minecraft:diamond_ore",
          "a type's deepslate sibling goes both ways");
    check(siblingName("mod:ruby_ore") == "mod:deepslate_ruby_ore" && siblingName("minecraft:deepslate_").empty()
              && siblingName("minecraft:").empty(), "siblings keep the namespace and refuse empty names");
    check(queryMatches("diamond ore", "minecraft:deepslate_diamond_ore", "Deepslate Diamond Ore")
              && queryMatches("DIAMOND_ORE", "minecraft:diamond_ore", "x"),
          "words match the name in any case, with spaces or underscores");
    check(queryMatches("ãã¤ã¤", "minecraft:diamond_ore",
                       "ãã¤ã¤ã¢ã³ãé±ç³"),
          "the display name in the current language matches");
    check(!queryMatches("diamond emerald", "minecraft:diamond_ore", "Diamond Ore") && !queryMatches("  ", "minecraft:a", "A")
              && !queryMatches("minecraft", "minecraft:stone", "Stone"),
          "every word must match, a blank query matches nothing, and the namespace is not searched");

    using lamium::ui::HighlightLayout;
    using lamium::ui::SettingsTable;
    using lamium::ui::ShapesLayout;
    auto table = SettingsTable::fit(640, 360, 60, 0, 13);
    auto layout = HighlightLayout::fit(table, 3, 0, 100, 0, false);
    check(layout.usable() && layout.dividerX > layout.left && layout.dividerX < layout.left + layout.width,
          "the listed column sits left of the search");
    check(layout.results.listVisible > 5 && layout.results.listVisible < 100 && layout.results.listCount == 100,
          "results scroll inside the space under the search");
    float rowY = layout.listed.listRowY(1) + 2;
    check(layout.hit(layout.listed.listLeft + 20, rowY).zone == HighlightLayout::Zone::Listed
              && layout.hit(layout.listed.listLeft + 20, rowY).index == 1, "a listed row is hit by its index");
    check(layout.hit(layout.removeX() + 1, rowY).zone == HighlightLayout::Zone::Remove, "the remove button sits on the row");
    check(layout.hit(layout.listed.listLeft + 20, layout.listed.listRowY(3) + 2).zone == HighlightLayout::Zone::None,
          "below the last listed row nothing is hit");
    check(layout.hit(layout.searchX() + 4, layout.searchY + 4).zone == HighlightLayout::Zone::Search, "the field takes clicks");
    check(layout.hit(layout.backX + 2, layout.headTop + 2).zone == HighlightLayout::Zone::Back, "Back is in the heading row");
    auto offered = HighlightLayout::fit(table, 3, 0, 100, 0, true);
    check(offered.results.rowsTop == layout.results.rowsTop + ShapesLayout::rowHeight
              && offered.hit(offered.addX() + 1, offered.offerY + 2).zone == HighlightLayout::Zone::Offer,
          "the sibling offer takes a row above the results");
    check(HighlightLayout::fit(table, 3, 0, 100, 1000, false).results.listFirst
              == 100 - layout.results.listVisible, "the first result row is clamped");
    check(!HighlightLayout::fit(SettingsTable::fit(100, 100, 29, 0), 0, 0, 0, 0, false).usable(),
          "too-small windows draw nothing");
}
