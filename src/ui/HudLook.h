#pragma once
#include "ui/HudEditorLayout.h"
#include <algorithm>
#include <vector>

namespace lamium::ui::hud_editor {
// The HUD layout editor's "Look" popover (L-357): the selected element's
// background (a choice stepper), text shadow and, for an element with
// graphics beside its text, whether the text keeps the standard size. Each
// row is the settings option the toolbar's buttons used to flip. Rows are
// as tall as the Contents popover's, so the two read as one family.
enum class LookRow { Background, Shadow, StandardText };
struct LookItem {
    LookRow kind = LookRow::Background;
    Box row;                // the hover band
    Box toggle;             // a switch row: flips it
    Box stepper, less, more; // the background row: its stepper and end cells
    Point switchAt, labelAt;
    float labelWidth = 0;
};
struct LookLayout { Box box; std::vector<LookItem> items; };
inline constexpr float lookWidth = 168, lookStepperWidth = 64, lookCell = 10;
inline LookLayout layoutLook(Spot toolbar, float toolbarH, bool background, bool shadow, bool standardText, float screenW,
                             float screenH, float switchW, float switchH) {
    LookLayout out;
    std::vector<LookRow> kinds;
    if (background) kinds.push_back(LookRow::Background);
    if (shadow) kinds.push_back(LookRow::Shadow);
    if (standardText) kinds.push_back(LookRow::StandardText);
    if (kinds.empty()) return out;
    constexpr float w = lookWidth, rowH = contentsRowHeight;
    float const wanted = static_cast<float>(kinds.size()) * rowH + 6;
    auto at = popoverSpot(toolbar, toolbarH, w, wanted, screenW, screenH);
    // Three rows at most: too little room shows what fits rather than scrolling.
    int const shown = std::clamp(static_cast<int>((at.h - 6) / rowH), 0, static_cast<int>(kinds.size()));
    float const h = static_cast<float>(shown) * rowH + 6;
    if (!toolbar.below || at.y < toolbar.y) at.y = at.y + at.h - h; // Hug the toolbar when opening upward.
    out.box = {at.x, at.y, w, h};
    for (int i = 0; i < shown; ++i) {
        LookItem item;
        item.kind = kinds[static_cast<size_t>(i)];
        float const ry = at.y + 3 + static_cast<float>(i) * rowH;
        item.row = {at.x + 2, ry, w - 4, rowH};
        if (item.kind == LookRow::Background) {
            // Name on the left, the `< value >` stepper on the right.
            item.stepper = {at.x + w - 4 - lookStepperWidth, ry + 1, lookStepperWidth, rowH - 2};
            item.less = {item.stepper.x, item.stepper.y, lookCell, item.stepper.h};
            item.more = {item.stepper.x + item.stepper.w - lookCell, item.stepper.y, lookCell, item.stepper.h};
            item.labelAt = {at.x + 5, ry + 1};
            item.labelWidth = item.stepper.x - item.labelAt.x - 3;
        } else {
            // As a settings row: the name, then the switch at the right end, where the stepper ends.
            item.switchAt = {at.x + w - 4 - switchW, ry + (rowH - switchH) / 2};
            item.labelAt = {at.x + 5, ry + 1};
            item.labelWidth = item.switchAt.x - item.labelAt.x - 3;
            item.toggle = item.row;
        }
        out.items.push_back(item);
    }
    return out;
}
}
