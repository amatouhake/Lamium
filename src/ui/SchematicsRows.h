#pragma once
// Pure pieces of the Schematics view (L-399): what its lists hold and where
// its tab strip stands. No game types; SchematicsView.cpp draws and acts.
#include "ui/ShapesLayout.h"
#include <vector>
namespace lamium::ui::schematics_rows {
// The Materials list over its lines (blocks first, then entities; `entity`
// says which each is): -1 a "Blocks" heading, -2 an "Entities" heading, else
// the line's index. Headings only when both kinds are present.
inline constexpr int blocksHeading = -1, entitiesHeading = -2;
inline std::vector<int> materialList(std::vector<bool> const& entity) {
    bool blocks = false, entities = false;
    for (bool e : entity) (e ? entities : blocks) = true;
    std::vector<int> rows;
    for (int i = 0; i < static_cast<int>(entity.size()); ++i) {
        bool e = entity[static_cast<size_t>(i)];
        if (blocks && entities && (i == 0 || e != entity[static_cast<size_t>(i - 1)])) rows.push_back(e ? entitiesHeading : blocksHeading);
        rows.push_back(i);
    }
    return rows;
}
// The tab strip in the list pane's toolbar: `count` tabs side by side with no
// gap (DESIGN "Controls"), across the pane inside its padding.
inline float tabWidth(ShapesLayout const& l, int count) { return (l.listWidth - 2 * ShapesLayout::pad) / static_cast<float>(count); }
inline float tabX(ShapesLayout const& l, int tab, int count) { return l.listLeft + ShapesLayout::pad + static_cast<float>(tab) * tabWidth(l, count); }
// The tab under a point, or -1.
inline int tabAt(ShapesLayout const& l, float x, float y, int count) {
    if (!l.usable() || y < l.toolbarTop || y >= l.toolbarTop + ShapesLayout::toolbarHeight) return -1;
    for (int i = 0; i < count; ++i)
        if (x >= tabX(l, i, count) && x < tabX(l, i, count) + tabWidth(l, count)) return i;
    return -1;
}
}
