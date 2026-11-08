#pragma once
#include "ui/SettingsTable.h"
#include "ui/ShapesLayout.h"
#include <algorithm>

namespace lamium::ui {
// Block highlight list (BACKLOG L-320 step 2). Like the game-language list it
// takes the settings table's place (header, sidebar and footer stay): a
// heading row with the path and Back, then the listed types on the left and
// the search over every block type on the right. Each column is a
// ShapesLayout list so rows and scrollbars work as in the other views.
struct HighlightLayout {
    static constexpr float titleHeight = 12, fieldHeight = 12, removeWidth = 40, addWidth = 40;
    enum class Zone { None, Back, Listed, Remove, Search, Offer, Result };
    struct Hit { Zone zone = Zone::None; int index = -1; };

    float left{}, width{}, headTop{}, bodyTop{}, footerTop{}, backX{};
    float dividerX{}, rightLeft{}, rightWidth{}, searchY{}, offerY{};
    bool offer{};
    ShapesLayout listed, results;

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
    static HighlightLayout fit(SettingsTable const& t, int listedCount, int listedFirst, int resultCount, int resultFirst,
                               bool offer) {
        HighlightLayout l;
        if (!t.usable()) return l;
        l.left = t.tableLeft; l.width = t.tableWidth;
        l.headTop = t.theadTop; l.bodyTop = t.rowsTop; l.footerTop = t.footerTop;
        l.backX = t.rowsRight() - SettingsTable::pad - ShapesLayout::closeWidth;
        float leftWidth = std::clamp(t.tableWidth * .42f, 140.0f, 220.0f);
        l.dividerX = l.left + leftWidth;
        l.listed = column(l.left, leftWidth, l.bodyTop + titleHeight, l.footerTop, listedCount, listedFirst);
        l.rightLeft = l.dividerX + 1; l.rightWidth = l.left + l.width - l.rightLeft;
        l.searchY = l.bodyTop + 2;
        l.offer = offer;
        l.offerY = l.searchY + fieldHeight + 3;
        float resultsTop = l.offerY + (offer ? ShapesLayout::rowHeight : 0) + titleHeight;
        l.results = column(l.rightLeft, l.rightWidth, resultsTop, l.footerTop, resultCount, resultFirst);
        return l;
    }
    float searchX() const { return rightLeft + SettingsTable::pad; }
    float searchWidth() const { return rightWidth - 2 * SettingsTable::pad; }
    float removeX() const { return listed.listLeft + listed.listWidth - SettingsTable::pad - removeWidth; }
    float addX() const { return rightLeft + rightWidth - SettingsTable::pad - addWidth; }
    static int rowAt(ShapesLayout const& c, float x, float y) {
        if (x < c.listLeft || x >= c.listLeft + c.listWidth || y < c.rowsTop) return -1;
        int offset = static_cast<int>((y - c.rowsTop) / ShapesLayout::rowHeight);
        if (offset >= c.listVisible || offset + c.listFirst >= c.listCount) return -1;
        return offset + c.listFirst;
    }
    Hit hit(float x, float y) const {
        if (!usable() || x < left || x >= left + width || y < headTop || y >= footerTop) return {};
        if (y < bodyTop) {
            if (x >= backX && x < backX + ShapesLayout::closeWidth) return {Zone::Back};
            return {};
        }
        if (int row = rowAt(listed, x, y); row >= 0)
            return {x >= removeX() && x < removeX() + removeWidth ? Zone::Remove : Zone::Listed, row};
        if (x >= searchX() && x < searchX() + searchWidth() && y >= searchY && y < searchY + fieldHeight) return {Zone::Search};
        if (offer && x >= addX() && x < addX() + addWidth && y >= offerY && y < offerY + ShapesLayout::rowHeight) return {Zone::Offer};
        if (int row = rowAt(results, x, y); row >= 0) return {Zone::Result, row};
        return {};
    }
};
}
