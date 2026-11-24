#include "ui/SchematicsRows.h"
#include "ui/SettingsTable.h"
#include <cmath>
void check(bool, char const*);
void schematicsRowsTests() {
    using namespace lamium::ui;
    using namespace lamium::ui::schematics_rows;
    check(materialList({}).empty(), "no lines, no rows");
    check(materialList({false, false}) == std::vector<int>{0, 1} && materialList({true}) == std::vector<int>{0},
          "one kind alone is listed without a heading");
    check(materialList({false, false, true}) == std::vector<int>{blocksHeading, 0, 1, entitiesHeading, 2},
          "blocks and entities each stand under their heading");
    check(materialList({false, true, true}) == std::vector<int>{blocksHeading, 0, entitiesHeading, 1, 2},
          "a heading starts where the kind changes");

    auto table = SettingsTable::fit(640, 360, 20, 0);
    auto l = ShapesLayout::fit(table, 640, 360, false, 80, 0, 16, 0, false, false);
    constexpr int tabs = 4;
    float w = tabWidth(l, tabs), y = l.toolbarTop + 1;
    check(l.usable() && std::abs(w * tabs - (l.listWidth - 2 * ShapesLayout::pad)) < 1e-3f && tabX(l, 0, tabs) == l.listLeft + ShapesLayout::pad,
          "the tabs fill the list pane inside its padding, side by side");
    check(tabAt(l, tabX(l, 0, tabs) + 1, y, tabs) == 0 && tabAt(l, tabX(l, 2, tabs) + 1, y, tabs) == 2
              && tabAt(l, tabX(l, 3, tabs) + w - .5f, y, tabs) == 3, "a point on a tab names it");
    check(tabAt(l, tabX(l, 0, tabs) - 1, y, tabs) == -1 && tabAt(l, tabX(l, 3, tabs) + w + 1, y, tabs) == -1,
          "the padding beside the strip is no tab");
    check(tabAt(l, tabX(l, 1, tabs) + 1, l.toolbarTop - 1, tabs) == -1
              && tabAt(l, tabX(l, 1, tabs) + 1, l.toolbarTop + ShapesLayout::toolbarHeight, tabs) == -1, "above and below the toolbar is no tab");
    check(tabAt(ShapesLayout{}, 10, 10, tabs) == -1, "a layout too small to use has no tabs");

    // The lists' columns, as the view placed them before they moved here.
    auto wide = listPlaces(100, 500);
    check(wide.blocksX == 500 - ShapesLayout::pad - 40 && wide.sizeX == wide.blocksX - 4 - 54 && wide.showSize,
          "a wide Files list shows the size left of the block count");
    check(wide.carriedX == 500 - ShapesLayout::pad - 34 && wide.leftX == wide.carriedX - 36 && wide.showPlaced
              && wide.placedX == wide.leftX - 36 && wide.neededX == wide.placedX - 36 && wide.distW == 28,
          "a wide Materials list shows needed, placed, left and carried from the right");
    auto middle = listPlaces(100, 300);
    check(middle.showSize && !middle.showPlaced && middle.neededX == middle.leftX - 36,
          "the placed count goes first; needed then stands next to left");
    auto narrow = listPlaces(100, 260);
    check(!narrow.showSize && !narrow.showPlaced, "a narrow list drops the size too");
    auto edge = listPlaces(0, 80 + 4 + 54 + 40 + ShapesLayout::pad);
    check(edge.showSize && !listPlaces(1, 80 + 4 + 54 + 40 + ShapesLayout::pad).showSize, "the size shows from 80 units of name room");

    // The preview's own turn stays smooth however long the machine has been up.
    check(autoTurn(0) == 0 && std::abs(autoTurn(1) - 12) < 1e-4f && std::abs(autoTurn(30.5) - 6) < 1e-3f, "12 degrees a second, once round in 30 s");
    double const upNineDays = 9 * 86400.0 + .25;
    float const step = autoTurn(upNineDays + 1.0 / 60) - autoTurn(upNineDays);
    check(std::abs(step - .2f) < 1e-3f, "after nine days of uptime one 60 Hz frame still turns it a fifth of a degree");
    float const asFloat = std::fmod(static_cast<float>(upNineDays + 1.0 / 60) * 12.f, 360.f) - std::fmod(static_cast<float>(upNineDays) * 12.f, 360.f);
    check(asFloat == 0, "as a float second count the same frame does not turn it at all");
}
