#pragma once
// Pure pieces of the Schematics view (L-399): what its lists hold, where its
// tab strip stands and what a click on the last frame's boxes hits. No game
// types; SchematicsView.cpp draws and acts.
#include "ui/ShapesLayout.h"
#include <array>
#include <cmath>
#include <utility>
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
// The columns of the Files, Check and Materials lists between `left` (inside
// the padding) and `right` (the list's content edge). A narrow list (a large
// UI) drops the columns it can do without, so the names keep their room.
struct ListPlaces {
    // Files: size and block count on the right; size goes first when narrow.
    float blocksW, blocksX, sizeW, sizeX;
    bool showSize;
    float distW; // Check: the distance
    // Materials: needed, placed, left and carried; "placed" goes first.
    float numW, carriedX, leftX, placedX, neededX;
    bool showPlaced;
};
constexpr ListPlaces listPlaces(float left, float right) {
    ListPlaces c{};
    c.blocksW = 40;
    c.blocksX = right - ShapesLayout::pad - c.blocksW;
    c.sizeW = 54;
    c.sizeX = c.blocksX - 4 - c.sizeW;
    c.showSize = c.sizeX - left >= 80;
    c.distW = 28;
    c.numW = 34;
    c.carriedX = right - ShapesLayout::pad - c.numW;
    c.leftX = c.carriedX - c.numW - 2;
    c.showPlaced = c.leftX - 2 * (c.numW + 2) - left - 16 >= 70;
    c.placedX = c.leftX - c.numW - 2;
    c.neededX = (c.showPlaced ? c.placedX : c.leftX) - c.numW - 2;
    return c;
}
// The preview's own turn in degrees, 12 a second, from a clock in seconds
// (the time since the machine started). Worked in double: as a float that
// count steps in 1/16 s once the machine has been up six days, and the turn
// with it.
inline float autoTurn(double seconds) { return static_cast<float>(std::fmod(seconds * 12.0, 360.0)); }
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
// A box as the last draw left it; 0 wide when it was not drawn.
struct Box {
    float x = 0, y = 0, w = 0, h = 0;
    bool contains(float px, float py) const { return px >= x && px < x + w && py >= y && py < y + h; }
};
// A press on the preview: its slice line (the top 16 units while it is
// sliced, even past a shorter preview) shows it whole again; anywhere else
// on it starts a drag.
enum class PreviewPress { None, Slice, Drag };
inline PreviewPress previewPress(Box const& preview, bool sliced, float x, float y) {
    if (preview.w > 0 && sliced && x >= preview.x && x < preview.x + preview.w && y >= preview.y && y < preview.y + 16)
        return PreviewPress::Slice;
    if (preview.w > 0 && preview.contains(x, y)) return PreviewPress::Drag;
    return PreviewPress::None;
}
// The Check list's four filter chips as last drawn: their spans across the
// list's heading row, {0, 0} for one not drawn. The chip under a point, or -1.
using ChipSpans = std::array<std::pair<float, float>, 4>;
inline int chipAt(ChipSpans const& spans, float headTop, float rowsTop, float x, float y) {
    if (y < headTop || y >= rowsTop) return -1;
    for (int i = 0; i < 4; ++i)
        if (x >= spans[static_cast<size_t>(i)].first && x < spans[static_cast<size_t>(i)].second) return i;
    return -1;
}
// The Check tab's own boxes from the last frame: "Ignore settings" (taken
// only while a placement is checked), the filter chips (not in the ignore
// view) and the pane's ignore controls, in the order they were drawn.
struct CheckBoxes {
    bool shown = false; // the Check tab is the one shown
    bool checked = false, ignoreView = false;
    Box ignoreSettings;
    ChipSpans chips{};
    float headTop = 0, rowsTop = 0;
    std::vector<Box> controls;
};
enum class CheckTarget { None, IgnoreSettings, Chip, Control };
struct CheckPress {
    CheckTarget target = CheckTarget::None;
    int index = -1; // the chip, or the control in drawing order
};
// What a click on them presses: "Ignore settings" first, then a chip, then
// the first control under the point.
inline CheckPress checkPress(CheckBoxes const& c, float x, float y) {
    if (!c.shown) return {};
    if (c.checked && c.ignoreSettings.w > 0 && c.ignoreSettings.contains(x, y)) return {CheckTarget::IgnoreSettings};
    if (!c.ignoreView)
        if (int chip = chipAt(c.chips, c.headTop, c.rowsTop, x, y); chip >= 0) return {CheckTarget::Chip, chip};
    for (int i = 0; i < static_cast<int>(c.controls.size()); ++i)
        if (c.controls[static_cast<size_t>(i)].contains(x, y)) return {CheckTarget::Control, i};
    return {};
}
}
