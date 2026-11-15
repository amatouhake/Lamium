#include "ui/ListColumns.h"
void check(bool, char const*);
void listColumnsTests() {
    using namespace lamium::ui;
    constexpr float sw = 18;
    auto ordered = [](ListColumns const& c) {
        float x = c.nameX + c.nameWidth;
        if (c.hasDistance()) { if (c.distanceX < x + ListColumns::gap - .01f) return false; x = c.distanceX + c.distanceWidth; }
        if (c.hasOwn()) { if (c.ownX < x + ListColumns::gap - .01f) return false; x = c.ownX + c.ownWidth; }
        if (c.shownWidth > 0) { if (c.shownX < x + ListColumns::gap - .01f) return false; x = c.shownX + c.shownWidth; }
        return c.markX + ListColumns::markColumn <= c.nameX && x <= c.right - ListColumns::pad;
    };
    for (float width : {240.f, 300.f, 190.f}) {
        auto waypoints = listColumns(100, width, 0, sw), placed = listColumns(100, width, 26, sw);
        check(ordered(waypoints) && ordered(placed), "mark, name, distance, own column and switch follow each other without overlap");
        check(waypoints.hasDistance() && !waypoints.hasOwn() && placed.hasDistance() && placed.hasOwn() && placed.ownWidth == 26,
              "a list from the docked width up keeps every column");
        check(waypoints.nameX == placed.nameX && waypoints.shownX == placed.shownX && waypoints.distanceX + waypoints.distanceWidth
              == waypoints.shownX - ListColumns::gap, "the lists share the mark, name and switch places; distance sits by the switch without an own column");
        check(placed.nameWidth >= ListColumns::minName, "the name keeps its room");
    }
    auto narrow = listColumns(0, 150, 26, sw);
    check(!narrow.hasDistance() && narrow.hasOwn() && ordered(narrow), "a narrow list drops the distance first");
    auto tiny = listColumns(0, 90, 26, sw);
    check(!tiny.hasDistance() && !tiny.hasOwn() && tiny.nameWidth > 0 && ordered(tiny), "a very narrow list keeps only the name and the switch");
    auto pane = listColumns(0, 200, 26, 0);
    check(pane.shownWidth == 0 && pane.ownX + pane.ownWidth == 200 - ListColumns::pad && !pane.onShown(199) && ordered(pane),
          "without a switch the own column reaches the right edge");
    auto c = listColumns(0, 240, 26, sw);
    check(c.onShown(c.shownX) && c.onShown(c.right - 1) && !c.onShown(c.shownX - 1), "a click from the switch's column to the edge takes the switch");
    float wide = 40; // a "Shown" heading wider than its column (L-195)
    float head = c.shownHeadingX(wide);
    check(head + wide <= c.right - 2 && c.headingLimit(wide) <= head - ListColumns::gap && c.headingLimit(10) >= c.ownX + c.ownWidth,
          "a wide Shown heading stays inside the list and the headings left of it stop short of it");
}
