#pragma once
#include "ui/SettingsTable.h"
#include "ui/ShapesLayout.h"
#include <algorithm>
#include <cmath>

namespace lamium::ui {
// Enchantment order (BACKLOG L-170). Like the block highlight list it takes
// the settings table's place: a heading row with the path and Back, then on
// the left the item, the kinds as an icon grid, the set buttons, the
// enchantments and under them the held-state switch; on the right the goal, the totals and
// the steps (two list rows each).
struct EnchantLayout {
    static constexpr float titleHeight = 12, barHeight = 14, cell = 20, cellGap = 2, buttonHeight = 11, stepWidth = 11;
    static constexpr float summaryLine = 11, goalWidth = 120;
    static constexpr int summaryLines = 3, stepRows = 2;
    enum class Zone { None, Back, Kind, UseHeld, SetAll, SetLast, SetClear, Row, RowMinus, RowPlus, Goal };
    struct Hit { Zone zone = Zone::None; int index = -1; };

    float left{}, width{}, headTop{}, bodyTop{}, footerTop{}, backX{};
    float dividerX{}, rightLeft{}, rightWidth{};
    float itemY{}, kindsTop{}, useHeldY{}, setsY{}, rowsTitleY{};
    float setAllX{}, setLastX{}, setClearX{}, setWide{}, setNarrow{};
    int kindColumns{}, kindCount{};
    bool useHeldShown{};
    float goalY{}, stepsTitleY{}, summaryTop{};
    ShapesLayout rows, steps;

    bool usable() const { return width > 0; }
    static ShapesLayout column(float left, float width, float rowsTop, float bottom, int count, int first) {
        ShapesLayout l;
        l.left = l.listLeft = left; l.width = l.listWidth = width;
        l.rowsTop = rowsTop;
        l.fieldVisible = 1; // ShapesLayout::usable(), which its scrollbar asks
        l.listCount = std::max(0, count);
        l.listVisible = std::max(0, static_cast<int>((bottom - rowsTop - 2) / ShapesLayout::rowHeight));
        l.listFirst = SettingsTable::clampFirst(first, l.listCount, l.listVisible);
        return l;
    }
    static EnchantLayout fit(SettingsTable const& t, int kinds, bool useHeld, int rowCount, int rowFirst, int stepCount, int stepFirst) {
        EnchantLayout l;
        if (!t.usable()) return l;
        float const pad = SettingsTable::pad;
        l.left = t.tableLeft; l.width = t.tableWidth;
        l.headTop = t.theadTop; l.bodyTop = t.rowsTop; l.footerTop = t.footerTop;
        l.backX = t.rowsRight() - pad - ShapesLayout::closeWidth;
        float const leftWidth = std::clamp(t.tableWidth * .46f, 150.0f, 230.0f);
        l.dividerX = l.left + leftWidth;
        l.rightLeft = l.dividerX + 1; l.rightWidth = l.left + l.width - l.rightLeft;

        l.kindCount = std::max(0, kinds);
        l.kindColumns = std::max(1, static_cast<int>((leftWidth - 2 * pad + cellGap) / (cell + cellGap)));
        int const kindRows = (l.kindCount + l.kindColumns - 1) / l.kindColumns;
        l.itemY = l.bodyTop + 1;
        l.kindsTop = l.itemY + barHeight + 1;
        float y = l.kindsTop + kindRows * (cell + cellGap);
        // The held-state row sits under the list, where showing it moves nothing.
        l.useHeldShown = useHeld;
        l.useHeldY = l.footerTop - barHeight;
        l.setsY = y + 1;
        float const room = leftWidth - 2 * pad - 2 * 3;
        l.setWide = std::floor(room * .38f); l.setNarrow = room - 2 * l.setWide;
        l.setAllX = l.left + pad; l.setLastX = l.setAllX + l.setWide + 3; l.setClearX = l.setLastX + l.setWide + 3;
        y = l.setsY + buttonHeight + 3;
        l.rowsTitleY = y;
        l.rows = column(l.left, leftWidth, y + titleHeight, useHeld ? l.useHeldY - 1 : l.footerTop, rowCount, rowFirst);

        // The totals sit right under the goal, where they are read first.
        l.goalY = l.bodyTop + 1;
        l.summaryTop = l.goalY + barHeight + 1;
        l.stepsTitleY = l.summaryTop + summaryLines * summaryLine + 6;
        l.steps = column(l.rightLeft, l.rightWidth, l.stepsTitleY + titleHeight, l.footerTop, stepCount * stepRows, stepFirst);
        return l;
    }
    float kindX(int index) const { return left + SettingsTable::pad + (index % kindColumns) * (cell + cellGap); }
    float kindY(int index) const { return kindsTop + (index / kindColumns) * (cell + cellGap); }
    float plusX() const { return rows.listLeft + rows.listWidth - SettingsTable::pad - stepWidth; }
    float minusX() const { return plusX() - stepWidth - 1; }
    float goalX() const { return rightLeft + rightWidth - SettingsTable::pad - goalWidth; }
    static int rowAt(ShapesLayout const& c, float x, float y) {
        if (x < c.listLeft || x >= c.listLeft + c.listWidth || y < c.rowsTop) return -1;
        int offset = static_cast<int>((y - c.rowsTop) / ShapesLayout::rowHeight);
        if (offset >= c.listVisible || offset + c.listFirst >= c.listCount) return -1;
        return offset + c.listFirst;
    }
    Hit hit(float x, float y) const {
        if (!usable() || x < left || x >= left + width || y < headTop || y >= footerTop) return {};
        if (y < bodyTop) return x >= backX && x < backX + ShapesLayout::closeWidth ? Hit{Zone::Back} : Hit{};
        if (x < dividerX) {
            for (int i = 0; i < kindCount; ++i)
                if (x >= kindX(i) && x < kindX(i) + cell && y >= kindY(i) && y < kindY(i) + cell) return {Zone::Kind, i};
            if (useHeldShown && y >= useHeldY && y < useHeldY + barHeight) return {Zone::UseHeld};
            if (y >= setsY && y < setsY + buttonHeight) {
                if (x >= setAllX && x < setAllX + setWide) return {Zone::SetAll};
                if (x >= setLastX && x < setLastX + setWide) return {Zone::SetLast};
                if (x >= setClearX && x < setClearX + setNarrow) return {Zone::SetClear};
                return {};
            }
            if (int row = rowAt(rows, x, y); row >= 0) {
                if (x >= minusX() && x < minusX() + stepWidth) return {Zone::RowMinus, row};
                if (x >= plusX() && x < plusX() + stepWidth) return {Zone::RowPlus, row};
                return {Zone::Row, row};
            }
            return {};
        }
        if (y >= goalY && y < goalY + barHeight && x >= goalX() && x < goalX() + goalWidth) return {Zone::Goal};
        return {};
    }
};
}
