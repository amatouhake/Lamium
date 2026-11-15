#pragma once
#include "ui/HighlightLayout.h"
#include <algorithm>
#include <cstdio>
#include <string>

namespace lamium::ui {
// General > Saved data (BACKLOG L-343 part 3, docs/demos/server-data.html).
// Like the game-language list it takes the settings table's place (header,
// sidebar and footer stay): a heading row with the path and Back, the list of
// saved data on the left and the selected one's details on the right, with
// its switch and buttons at the bottom of the details.
struct SavedDataLayout {
    static constexpr float lineHeight = 12, buttonHeight = 12, buttonGap = 4, noteLines = 3;
    enum class Zone { None, Back, Row, AnyPort, Use, Delete };
    struct Hit { Zone zone = Zone::None; int index = -1; };

    float left{}, width{}, headTop{}, bodyTop{}, footerTop{}, backX{};
    float dividerX{}, detailLeft{}, detailWidth{};
    float actionsY{}, anyPortY{}, noteY{};
    float useX{}, useWidth{}, deleteX{}, deleteWidth{};
    bool anyPort = false, use = false, remove = false;
    ShapesLayout list;

    bool usable() const { return width > 0; }
    // `useWide` and `deleteWide`: the buttons' text widths; the armed delete names what goes and is wider.
    static SavedDataLayout fit(SettingsTable const& t, int count, int first, bool anyPort, bool use, bool remove,
                               float useWide, float deleteWide) {
        SavedDataLayout l;
        if (!t.usable()) return l;
        l.left = t.tableLeft; l.width = t.tableWidth;
        l.headTop = t.theadTop; l.bodyTop = t.rowsTop; l.footerTop = t.footerTop;
        l.backX = t.rowsRight() - SettingsTable::pad - ShapesLayout::closeWidth;
        l.detailWidth = std::clamp(t.tableWidth * .44f, 150.0f, 240.0f);
        l.dividerX = l.left + l.width - l.detailWidth;
        l.detailLeft = l.dividerX + 1;
        l.list = HighlightLayout::column(l.left, l.dividerX - l.left, l.bodyTop, l.footerTop, count, first);
        l.anyPort = anyPort; l.use = use; l.remove = remove;
        float inner = l.detailWidth - 2 * SettingsTable::pad;
        // Use on the bottom row, Delete on a row of its own above it, the switch above both. Nothing moves when
        // Delete is armed and its label grows: a second click never lands on a control that was somewhere else.
        l.actionsY = l.footerTop - buttonHeight - 3;
        l.useX = l.deleteX = l.detailLeft + SettingsTable::pad;
        l.useWidth = use ? std::min(inner, useWide) : 0;
        l.deleteWidth = remove ? std::min(inner, deleteWide) : 0;
        l.deleteY = use ? l.actionsY - buttonHeight - buttonGap : l.actionsY;
        float const top = remove ? l.deleteY : use ? l.actionsY : l.footerTop;
        l.anyPortY = top - lineHeight - 2;
        l.noteY = (anyPort ? l.anyPortY : top) - noteLines * lineHeight - 2;
        return l;
    }
    float deleteY{};
    float useY() const { return actionsY; }
    Hit hit(float x, float y) const {
        if (!usable() || x < left || x >= left + width || y < headTop || y >= footerTop) return {};
        if (y < bodyTop) {
            if (x >= backX && x < backX + ShapesLayout::closeWidth) return {Zone::Back};
            return {};
        }
        if (int row = HighlightLayout::rowAt(list, x, y); row >= 0) return {Zone::Row, row};
        if (x < detailLeft) return {};
        auto over = [&](float bx, float by, float w, float h) { return x >= bx && x < bx + w && y >= by && y < by + h; };
        if (anyPort && over(detailLeft, anyPortY, detailWidth, lineHeight)) return {Zone::AnyPort};
        if (use && over(useX, useY(), useWidth, buttonHeight)) return {Zone::Use};
        if (remove && over(deleteX, deleteY, deleteWidth, buttonHeight)) return {Zone::Delete};
        return {};
    }
};
// "3.4 MB": what a size reads as in the list and the details.
inline std::string sizeText(long long bytes, bool more = false) {
    double const megabytes = bytes / (1024.0 * 1024.0);
    char text[32];
    std::snprintf(text, sizeof text, megabytes >= 100 ? "%.0f MB" : "%.1f MB", megabytes);
    return std::string(text) + (more ? "+" : "");
}
}
