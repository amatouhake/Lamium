#pragma once
#include "ui/SettingsTable.h"
#include "ui/ShapesLayout.h"
#include <algorithm>
#include <cmath>

namespace lamium::ui {
// Enchantment order (BACKLOG L-170). Like the block highlight list it takes
// the settings table's place: a heading row with the path and Back, then on
// the left the item, the kinds as an icon grid with the main-hand item's cell
// last, the set buttons and the enchantments; on the right the goal, the totals and
// the steps (two list rows each). The left column alone is also the panel left
// of the anvil screen (fitPanel), where the anvil's left slot gets a cell too.
struct EnchantLayout {
    static constexpr float titleHeight = 12, barHeight = 14, cell = 20, cellGap = 2, buttonHeight = 11, stepWidth = 11;
    static constexpr float summaryLine = 11, goalWidth = 120, levelWidth = 22;
    static constexpr int summaryLines = 3, stepRows = 2;
    enum class Zone { None, Back, Held, Kind, SetAll, SetLast, SetClear, Row, RowMinus, RowPlus, Goal, Slot };
    static constexpr float statusHeight = 23; // the panel's two lines of status under the rows
    struct Hit { Zone zone = Zone::None; int index = -1; };

    float left{}, width{}, headTop{}, bodyTop{}, footerTop{}, backX{};
    float dividerX{}, rightLeft{}, rightWidth{};
    float itemY{}, kindsTop{}, setsY{}, rowsTitleY{};
    float setAllX{}, setLastX{}, setClearX{}, setWide{}, setNarrow{};
    int kindColumns{}, kindCount{};
    bool heldCell{}, slotPlace{}, slotCell{};
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
    static EnchantLayout fit(SettingsTable const& t, int kinds, bool held, int rowCount, int rowFirst, int stepCount, int stepFirst) {
        EnchantLayout l;
        if (!t.usable()) return l;
        float const pad = SettingsTable::pad;
        l.left = t.tableLeft; l.width = t.tableWidth;
        l.headTop = t.theadTop; l.bodyTop = t.rowsTop; l.footerTop = t.footerTop;
        l.backX = t.rowsRight() - pad - ShapesLayout::closeWidth;
        float const leftWidth = std::clamp(t.tableWidth * .46f, 150.0f, 230.0f);
        l.layColumn(leftWidth, kinds, held, false, false, rowCount, rowFirst);
        l.rightLeft = l.dividerX + 1; l.rightWidth = l.left + l.width - l.rightLeft;

        // The totals sit right under the goal, where they are read first.
        l.goalY = l.bodyTop + 1;
        l.summaryTop = l.goalY + barHeight + 1;
        l.stepsTitleY = l.summaryTop + summaryLines * summaryLine + 6;
        l.steps = column(l.rightLeft, l.rightWidth, l.stepsTitleY + titleHeight, l.footerTop, stepCount * stepRows, stepFirst);
        return l;
    }
    // The choosing column from `left`, `bodyTop` down to `footerTop`: the item's line, the cells, the set
    // buttons, the rows.
    void layColumn(float leftWidth, int kinds, bool held, bool slotHasPlace, bool slot, int rowCount, int rowFirst) {
        float const pad = SettingsTable::pad;
        dividerX = left + leftWidth;
        kindCount = std::max(0, kinds);
        // The main-hand item's cell comes after the last kind, then the anvil
        // slot's where there is an anvil. Their places are kept whether or
        // not there is an item, so nothing below moves.
        heldCell = held;
        slotPlace = slotHasPlace;
        slotCell = slotHasPlace && slot;
        kindColumns = std::max(1, static_cast<int>((leftWidth - 2 * pad + cellGap) / (cell + cellGap)));
        int const kindRows = (kindCount + 1 + (slotPlace ? 1 : 0) + kindColumns - 1) / kindColumns;
        itemY = bodyTop + 1;
        kindsTop = itemY + barHeight + 1;
        float y = kindsTop + kindRows * (cell + cellGap);
        setsY = y + 1;
        float const room = leftWidth - 2 * pad - 2 * 3;
        setWide = std::floor(room * .38f); setNarrow = room - 2 * setWide;
        setAllX = left + pad; setLastX = setAllX + setWide + 3; setClearX = setLastX + setWide + 3;
        y = setsY + buttonHeight + 3;
        rowsTitleY = y;
        rows = column(left, leftWidth, y + titleHeight, footerTop, rowCount, rowFirst);
    }
    // The choosing column on its own, as a panel: its status lines take the last `statusHeight` units.
    static EnchantLayout fitPanel(float left, float top, float width, float height, int kinds, bool held, bool slot, int rowCount, int rowFirst) {
        EnchantLayout l;
        if (width < 120 || height < 150) return l;
        l.left = left; l.width = width;
        l.headTop = l.bodyTop = top; l.footerTop = top + height - statusHeight;
        l.backX = left + width;
        l.layColumn(width, kinds, held, true, slot, rowCount, rowFirst);
        l.rightLeft = l.dividerX; l.rightWidth = 0;
        return l;
    }
    float kindX(int index) const { return left + SettingsTable::pad + (index % kindColumns) * (cell + cellGap); }
    int heldIndex() const { return kindCount; }
    int slotIndex() const { return kindCount + 1; }
    float kindY(int index) const { return kindsTop + (index / kindColumns) * (cell + cellGap); }
    // A row reads name, then the level as a stepper (- level +), then the
    // switch at the right edge, so nothing is left empty beside it.
    float switchX() const { return rows.listLeft + rows.listWidth - SettingsTable::pad - 18; }
    float plusX() const { return switchX() - 6 - stepWidth; }
    float levelX() const { return plusX() - levelWidth; }
    float minusX() const { return levelX() - stepWidth; }
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
            if (heldCell && x >= kindX(heldIndex()) && x < kindX(heldIndex()) + cell && y >= kindY(heldIndex()) && y < kindY(heldIndex()) + cell)
                return {Zone::Held};
            if (slotCell && x >= kindX(slotIndex()) && x < kindX(slotIndex()) + cell && y >= kindY(slotIndex()) && y < kindY(slotIndex()) + cell)
                return {Zone::Slot};
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
