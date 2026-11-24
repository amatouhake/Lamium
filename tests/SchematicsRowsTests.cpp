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
}
