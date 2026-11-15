#pragma once
#include <algorithm>

namespace lamium::ui {
// One column shape for the Waypoints, Shapes and Placed lists, docked or not,
// and the selection pane's name lines (L-190): the mark (kind and color),
// the name, the distance, the list's own column (placements: progress) and
// the shown switch. Positions stay in the detail panes. Pure: the view passes
// the list's span and its measured heading widths.
struct ListColumns {
    static constexpr float pad = 6, markColumn = 14, distanceColumn = 48, gap = 4, minName = 48;
    float left = 0, right = 0;
    float markX = 0;                         // the mark's 9-unit column; its center is markX + 4
    float nameX = 0, nameWidth = 0;
    float distanceX = 0, distanceWidth = 0;  // 0 wide when a narrow list drops it
    float ownX = 0, ownWidth = 0;            // 0 wide for a list without one
    float shownX = 0, shownWidth = 0;        // the switch; 0 wide in the selection pane
    bool hasDistance() const { return distanceWidth > 0; }
    bool hasOwn() const { return ownWidth > 0; }
    // A click on the row's switch: from its column's left gap to the right edge.
    bool onShown(float x) const { return shownWidth > 0 && x >= shownX && x < right; }
    // The "Shown" heading, centered over the switch but kept inside the list:
    // a heading wider than its column then reaches left, not past the edge.
    float shownHeadingX(float headingWidth) const {
        return std::min(shownX + shownWidth / 2 - headingWidth / 2, right - 2 - headingWidth);
    }
    // Where the right-aligned headings left of the switch must end: short of
    // a wide "Shown" heading, so the two never meet (L-195).
    float headingLimit(float shownHeadingWidth) const {
        return shownWidth > 0 ? shownHeadingX(shownHeadingWidth) - gap : right - pad;
    }
};
// The columns of a list spanning [left, left + width), whose own column is
// `ownWidth` wide (0: none) and whose switch is `switchWidth` (0: no switch).
// A list too narrow for every column drops the distance first, then its own
// column, so the name keeps minName where it can.
inline ListColumns listColumns(float left, float width, float ownWidth, float switchWidth) {
    ListColumns c;
    c.left = left;
    c.right = left + std::max(0.f, width);
    c.markX = left + ListColumns::pad;
    c.nameX = c.markX + ListColumns::markColumn;
    float end = c.right - ListColumns::pad;
    if (switchWidth > 0) {
        c.shownWidth = switchWidth;
        c.shownX = end - switchWidth - 2;
        end = c.shownX - ListColumns::gap;
    }
    auto room = [&](float taken) { return end - taken - c.nameX; };
    bool own = ownWidth > 0, distance = true;
    float const ownTaken = own ? ownWidth + ListColumns::gap : 0, distanceTaken = ListColumns::distanceColumn + ListColumns::gap;
    if (room(ownTaken + distanceTaken) < ListColumns::minName) distance = false;
    if (!distance && room(ownTaken) < ListColumns::minName) own = false;
    if (own) {
        c.ownWidth = ownWidth;
        c.ownX = end - ownWidth;
        end = c.ownX - ListColumns::gap;
    }
    if (distance) {
        c.distanceWidth = ListColumns::distanceColumn;
        c.distanceX = end - ListColumns::distanceColumn;
        end = c.distanceX - ListColumns::gap;
    }
    c.nameWidth = std::max(0.f, end - c.nameX);
    return c;
}
}
