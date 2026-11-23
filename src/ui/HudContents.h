#pragma once
#include "ui/ScrollBar.h"
#include "ui/HudEditorLayout.h"
#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lamium::ui::hud_editor {
// The HUD layout editor's "Contents" popover (L-336): a switch per thing an
// element shows, each the settings row's own option. Only the Info HUD's
// lines can also be reordered; other elements keep the order their layout
// gives. Look choices and behavior settings stay in the settings table, and
// a content setting without an off state (Target's health, armor and growth
// meters) has no switch here.
struct ContentsEntry {
    std::string option; // the settings option id the switch flips
    std::string line;   // Info line id, for reordering; empty elsewhere
};
struct Contents { std::vector<ContentsEntry> entries; bool reorder = false; };
inline Contents contentsOf(HudElementId id, std::vector<std::string> const& lineOrder) {
    Contents out;
    auto add = [&](std::initializer_list<std::string_view> options) {
        for (auto option : options) out.entries.push_back({std::string(option), {}});
    };
    switch (id) {
    case HudElementId::Info:
        out.reorder = true;
        for (auto const& line : lineOrder) out.entries.push_back({"information." + line, line});
        break;
    case HudElementId::Target:
        add({"information.targetIcon", "information.targetIdentifier", "information.targetStates", "information.targetBlockData",
             "information.targetCoordinates", "information.targetMeasure"});
        break;
    case HudElementId::Durability:
        add({"information.durabilityMainHand", "information.durabilityOffhand", "information.durabilityArmor", "information.durabilityElytra",
             "information.durabilityAmmo"});
        break;
    case HudElementId::Inventory: add({"information.inventoryHotbar"}); break;
    // Map text (GitHub #80): in the order it is drawn, under the map.
    case HudElementId::Minimap: add({"map.compass", "map.coordinates", "map.scaledCoordinates", "map.chunk", "map.biome"}); break;
    case HudElementId::UsedSlots: add({"information.usedSlotsHotbar"}); break;
    default: break;
    }
    return out;
}

// Rows are 12 units in a 136-unit popover; the scroll bar has its own strip
// on the right. As a settings row: the name on the left, the switch at the
// right end; reorderable rows put up and down arrows before the switch, the
// first row's up and the last row's down drawn off.
inline constexpr float contentsWidth = 136, contentsRowHeight = 12;
struct ContentsRow {
    int index = 0;           // into Contents::entries
    Box row;                 // the hover band
    Box toggle;              // flips the switch
    std::optional<Box> up, down;
    bool canUp = false, canDown = false;
    Point switchAt, labelAt, upAt, downAt; // top-left corners to draw at
    float labelWidth = 0;
};
struct ContentsTrack { float x = 0, top = 0, track = 0; };
struct ContentsLayout {
    Box box;
    int first = 0, visible = 0, count = 0;
    std::optional<ContentsTrack> track;
    std::vector<ContentsRow> rows;
};
inline ContentsLayout layoutContents(Spot toolbar, float toolbarH, int count, bool reorder, int first, float screenW,
                                     float screenH, float switchW, float switchH) {
    ContentsLayout out;
    constexpr float w = contentsWidth, rowH = contentsRowHeight;
    count = std::max(0, count);
    out.count = count;
    auto at = popoverSpot(toolbar, toolbarH, w, count * rowH + 6, screenW, screenH);
    out.visible = std::clamp(static_cast<int>((at.h - 6) / rowH), 0, count);
    float h = out.visible * rowH + 6;
    if (!toolbar.below || at.y < toolbar.y) at.y = at.y + at.h - h; // Hug the toolbar when opening upward.
    out.box = {at.x, at.y, w, h};
    float listTop = at.y + 3, listH = out.visible * rowH;
    out.first = std::clamp(first, 0, std::max(0, count - out.visible));
    if (out.visible < count && out.visible > 0) out.track = ContentsTrack{at.x + w - 5, listTop, listH};
    // Rows and their columns end short of the bar while it shows.
    float gutter = out.track ? rowBarGutter : 0.f, right = at.x + w - 4 - gutter;
    for (int i = 0; i < out.visible; ++i) {
        ContentsRow row;
        row.index = out.first + i;
        float ry = listTop + i * rowH;
        row.row = {at.x + 2, ry, w - 4 - gutter, rowH};
        // The whole row flips the switch; the arrows, tested first, take their own cells.
        row.toggle = {at.x + 2, ry, right - at.x - 2, rowH}; // to the switch's end, short of the scroll bar
        row.switchAt = {right - switchW, ry + (rowH - switchH) / 2};
        row.labelAt = {at.x + 5, ry + 1};
        float nameEnd = row.switchAt.x;
        if (reorder) {
            row.up = Box{row.switchAt.x - 28, ry, 11, rowH};
            row.down = Box{row.switchAt.x - 17, ry, 13, rowH};
            row.canUp = row.index > 0;
            row.canDown = row.index + 1 < count;
            row.upAt = {row.up->x + 4, ry + 4};
            row.downAt = {row.down->x + 4, ry + 4};
            nameEnd = row.up->x;
        }
        row.labelWidth = nameEnd - row.labelAt.x - 3;
        out.rows.push_back(row);
    }
    return out;
}
// The scroll bar takes a press a little wider than it is drawn.
inline bool onContentsTrack(ContentsLayout const& layout, float px, float py) {
    return layout.track && px >= layout.track->x + 1 - barReach && px < layout.track->x + 5 && py >= layout.track->top
        && py < layout.track->top + layout.track->track;
}
}
