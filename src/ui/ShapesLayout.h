#pragma once
#include "ui/RowSelection.h"
#include "ui/ScrollBar.h"
#include "ui/SettingsTable.h"
#include <algorithm>
#include <cmath>

namespace lamium::ui {
// Geometry of the Shapes view in GUI units, shared by drawing and hit testing.
// Inside the settings panel it replaces the table area with a list and an
// editor side by side. Docked, it is a narrow panel on the right so the world
// stays visible: list above, editor below.
struct ShapesLayout {
    static constexpr float rowHeight = 14, toolbarHeight = 16, theadHeight = 12, headerHeight = 20;
    static constexpr float pad = 6, previewSize = 60, actionsHeight = 16, pickHeight = 22;
    static constexpr float closeWidth = 58, dockWidth = 62, keysWidth = 58, drawAllWidth = 78, newWidth = 86;
    static constexpr int dockedListRows = 5;
    enum class Zone { None, Close, Dock, Keys, DrawAll, NewShape, ListRow, Name, LayerDown, LayerUp, Field, Action, Pick, Footer, Bulk };
    struct Hit { Zone zone = Zone::None; int index = -1; int part = 0; };

    bool docked{}, draft{}, picking{};
    float firstActionWidth = actionWidth; // Wider when the first action has a long label.
    float left{}, top{}, width{}, height{};
    float closeX{}, dockX{}, keysX{}, drawAllX{}, drawAllY{};
    float listLeft{}, listWidth{}, toolbarTop{}, theadTop{}, rowsTop{};
    float noticeTop{}, noticeHeight{};
    int listVisible{}, listFirst{}, listCount{};
    float detailLeft{}, detailWidth{}, detailTop{}, nameY{}, previewY{}, fieldsTop{}, actionsY{}, footerTop{};
    int fieldVisible{}, fieldFirst{}, fieldCount{};
    // Several rows selected (L-190): the selection pane's actions replace the
    // editor, set by the view after fit(). A list without an irreversible
    // action (the Check list) has no summary line.
    std::vector<BulkAction> selecting;

    // fullToolbar: the toolbar is taken across its width (the Schematics tabs),
    // so a docked draw-all switch gets a row of its own below it.
    static ShapesLayout fit(SettingsTable const& table, float screenWidth, float screenHeight, bool docked,
                            int listCount, int listFirst, int fieldCount, int fieldFirst, bool draft, bool picking,
                            bool fullToolbar = false, float notice = 0) {
        ShapesLayout l;
        l.docked = docked; l.draft = draft; l.picking = picking;
        l.listCount = std::max(0, listCount); l.fieldCount = std::max(0, fieldCount);
        float bodyTop, bodyBottom;
        if (docked) {
            if (!std::isfinite(screenWidth) || !std::isfinite(screenHeight) || screenWidth < 240 || screenHeight < 150) return l;
            l.width = std::clamp(screenWidth * .42f, 240.0f, 300.0f);
            l.left = screenWidth - l.width - 6;
            l.top = 6;
            l.height = screenHeight - 12;
            l.footerTop = l.top + l.height - 14;
            l.listLeft = l.left; l.listWidth = l.width;
            bodyTop = l.top + headerHeight;
            bodyBottom = l.footerTop;
        } else {
            if (!table.usable()) return l;
            l.left = table.left; l.top = table.top; l.width = table.width; l.height = table.height;
            l.footerTop = table.footerTop;
            float detail = std::clamp(table.tableWidth * .5f, 190.0f, 280.0f);
            l.listLeft = table.tableLeft; l.listWidth = table.tableWidth - detail;
            l.detailLeft = l.listLeft + l.listWidth; l.detailWidth = detail;
            bodyTop = table.theadTop;
            bodyBottom = table.footerTop;
        }
        l.closeX = l.left + l.width - pad - closeWidth;
        l.dockX = l.closeX - 4 - dockWidth;
        l.keysX = l.dockX - 4 - keysWidth;
        l.toolbarTop = bodyTop;
        // Docked panels are narrow: the draw-all switch moves to the toolbar.
        l.drawAllX = docked ? l.listLeft + l.listWidth - pad - drawAllWidth : l.keysX - 4 - drawAllWidth;
        bool ownRow = docked && fullToolbar;
        l.drawAllY = !docked ? l.top + 4 : ownRow ? l.toolbarTop + toolbarHeight + 2 : l.toolbarTop + 2;
        // A notice above the list (L-343) takes its height from the list's rows.
        l.noticeTop = bodyTop + (ownRow ? 2 : 1) * toolbarHeight;
        l.noticeHeight = std::max(0.0f, notice);
        l.theadTop = l.noticeTop + l.noticeHeight;
        l.rowsTop = l.theadTop + theadHeight;
        l.actionsY = bodyBottom - actionsHeight - 2;
        float pitch = picking ? pickHeight : rowHeight;
        auto placeDetail = [&](float detailTop) {
            l.detailTop = detailTop;
            l.nameY = l.detailTop + 4 + (draft ? 12 : 0);
            l.previewY = l.nameY + rowHeight + 4;
            l.fieldsTop = picking ? l.nameY + rowHeight : l.previewY + previewSize + 4;
            l.fieldVisible = std::max(0, static_cast<int>((l.actionsY - 2 - l.fieldsTop) / pitch));
        };
        if (docked) {
            l.detailLeft = l.left; l.detailWidth = l.width;
            // On a short screen the list gives up rows (down to two) so the
            // editor below keeps at least two of its own.
            auto giveRows = [&] {
                for (l.listVisible = dockedListRows; ; --l.listVisible) {
                    placeDetail(l.rowsTop + l.listVisible * rowHeight + 2);
                    if (l.fieldVisible >= 2 || l.listVisible <= 2) break;
                }
            };
            giveRows();
            // Too short for the notice as well: the editor stays and the notice
            // is left to the expanded screen.
            if (l.fieldVisible < 2 && l.noticeHeight > 0) {
                l.noticeHeight = 0;
                l.theadTop = l.noticeTop;
                l.rowsTop = l.theadTop + theadHeight;
                giveRows();
            }
        } else {
            l.listVisible = std::max(0, static_cast<int>((bodyBottom - l.rowsTop - 2) / rowHeight));
            placeDetail(bodyTop);
        }
        l.listFirst = std::clamp(listFirst, 0, std::max(0, l.listCount - l.listVisible));
        l.fieldFirst = std::clamp(fieldFirst, 0, std::max(0, l.fieldCount - l.fieldVisible));
        return l;
    }
    bool usable() const { return width > 0 && fieldVisible > 0; }
    float listRowY(int index) const { return rowsTop + (index - listFirst) * rowHeight; }
    // The list's scrollbar, along its right edge while the rows overflow
    // (the press zone; the drag is the shared one, ScrollDrag.h).
    // A list without a detail pane (Game language) has one too; a docked
    // panel too short for its body has none.
    bool onScrollbar(float x, float y) const {
        float right = listLeft + listWidth;
        return (usable() || (detailWidth <= 0 && listWidth > 0)) && listCount > listVisible && x >= right - 3 - barReach && x < right && y >= rowsTop && y < rowsTop + listVisible * rowHeight;
    }
    // A row's background and hover stop short of the bar while it shows, so
    // the selection frame never covers it and pointing at it lights no row.
    // The columns end inside it: `contentWidth` is the span they lay out in.
    static constexpr float barGutter = rowBarGutter;
    float contentWidth() const { return listWidth - (listCount > listVisible ? barGutter : 0); }
    float rowWidth() const { return contentWidth() - 2; }
    // The list's bar as drawn (drawListScrollbar) and dragged.
    ScrollBar scrollBar() const {
        return rowScrollBar(listLeft + listWidth - 3, rowsTop, listVisible * rowHeight, listCount, listVisible, listFirst);
    }
    float fieldY(int index) const { return fieldsTop + (index - fieldFirst) * (picking ? pickHeight : rowHeight); }
    // Values are right-aligned steppers in the editor.
    float stepperWidth() const { return std::min(118.0f, detailWidth - 2 * pad - 70); }
    float stepperX() const { return detailLeft + detailWidth - pad - stepperWidth(); }
    static constexpr float arrowWidth = 11;
    // Color rows show every color as a swatch in the value column (L-83: one
    // chooser everywhere); a click picks one, arrows still step.
    static constexpr float swatchGap = 2;
    float swatchSize(int count) const {
        if (count <= 0) return 0;
        return std::clamp((stepperWidth() - (count - 1) * swatchGap) / count, 6.0f, 11.0f);
    }
    float swatchX(int index, int count) const { return stepperX() + index * (swatchSize(count) + swatchGap); }
    int swatchAt(float x, int count) const {
        for (int i = 0; i < count; ++i)
            if (x >= swatchX(i, count) - swatchGap / 2 && x < swatchX(i, count) + swatchSize(count) + swatchGap / 2) return i;
        return -1;
    }
    // A docked header's title stops before the key settings link; the draw-all
    // switch is in the toolbar then, so it cannot bound the title.
    float dockedTitleWidth() const { return std::max(0.f, keysX - left - pad - 4); }
    float layerStepperX() const { return detailLeft + pad + previewSize + 8; }
    float layerY() const { return previewY + 30; }
    static constexpr float actionWidth = 56, deleteWidth = 96;
    float actionX(int index) const { return detailLeft + pad + index * (actionWidth + 4); }
    // The delete action sits at the right edge.
    float deleteX() const { return detailLeft + detailWidth - pad - deleteWidth; }
    // The selection pane in the detail pane: the count where the name sits,
    // the summary and the names below, the buttons where the editor's actions are.
    // `below`: room kept under the names for the view's own lines;
    // `namesTop`: where the names start instead, for a view that keeps
    // something above them (the Check preview, L-183).
    SelectionPaneLayout selectionPane(int count, float below = 0, float namesTop = 0) const {
        SelectionPaneLayout p;
        p.x = detailLeft + pad; p.w = detailWidth - 2 * pad; p.y = nameY;
        bool destructive = std::any_of(selecting.begin(), selecting.end(), irreversible);
        p.summary = destructive;
        p.summaryY = nameY + rowHeight;
        p.linesTop = std::max(namesTop, destructive ? p.summaryY + p.lineHeight : p.summaryY);
        p.fit = namesFit(count, actionsY - 4 - below - p.linesTop, p.lineHeight);
        p.buttonsY = actionsY + 2; p.buttonH = 12;
        if (destructive) placeSelectionButtons(p, selecting, actionX(0), actionWidth, 4, deleteX(), deleteWidth);
        else {
            placeSelectionButtons(p, selecting, actionX(0), 96, 4, detailLeft + detailWidth - pad + 4, 0);
        }
        return p;
    }

    // Hits follow what is drawn. A view that is not usable() draws no list or
    // editor; docked, it still draws Close and Expand (drawDockedTooShort), so
    // those two answer and the panel can always be left.
    Hit hit(float x, float y) const {
        if (!(width > 0) || !std::isfinite(x) || !std::isfinite(y)) return {};
        if (x < left || x >= left + width || y < top || y >= top + height) return {};
        if (!usable()) {
            if (!docked || y >= top + headerHeight) return {};
            if (x >= closeX && x < closeX + closeWidth) return {Zone::Close};
            if (x >= dockX && x < dockX + dockWidth) return {Zone::Dock};
            return {};
        }
        if (y < top + headerHeight) {
            if (x >= closeX && x < closeX + closeWidth) return {Zone::Close};
            if (x >= dockX && x < dockX + dockWidth) return {Zone::Dock};
            if (x >= keysX && x < keysX + keysWidth) return {Zone::Keys};
            if (!docked && x >= drawAllX && x < drawAllX + drawAllWidth) return {Zone::DrawAll};
            return {};
        }
        if (y >= footerTop) return {Zone::Footer};
        bool inList = x >= listLeft && x < listLeft + listWidth && (!docked || y < detailTop);
        if (inList) {
            if (docked && x >= drawAllX && x < drawAllX + drawAllWidth && y >= drawAllY - 2 && y < drawAllY - 2 + toolbarHeight)
                return {Zone::DrawAll};
            if (y >= toolbarTop && y < toolbarTop + toolbarHeight)
                return x < listLeft + pad + newWidth ? Hit{Zone::NewShape} : Hit{};
            if (y < rowsTop || onScrollbar(x, y)) return {};
            int offset = static_cast<int>((y - rowsTop) / rowHeight);
            int row = listFirst + offset;
            return offset < listVisible && row < listCount ? Hit{Zone::ListRow, row} : Hit{};
        }
        if (x < detailLeft || x >= detailLeft + detailWidth || y < detailTop) return {};
        if (!selecting.empty()) {
            int at = selectionPane(0).at(x, y);
            return at >= 0 ? Hit{Zone::Bulk, at} : Hit{};
        }
        if (y >= actionsY && y < actionsY + actionsHeight) {
            // Draft: Create, Cancel. Existing shape: Duplicate, then Delete at the right edge.
            if (x >= actionX(0) && x < actionX(0) + firstActionWidth) return {Zone::Action, 0};
            float second = draft ? actionX(1) : deleteX();
            if (x >= second && x < second + (draft ? actionWidth : deleteWidth)) return {Zone::Action, 1};
            return {};
        }
        if (picking) {
            if (y < fieldsTop) return {};
            int offset = static_cast<int>((y - fieldsTop) / pickHeight);
            int row = fieldFirst + offset;
            return offset < fieldVisible && row < fieldCount ? Hit{Zone::Pick, row} : Hit{};
        }
        if (y >= nameY && y < nameY + rowHeight) return {Zone::Name};
        if (y >= layerY() && y < layerY() + rowHeight) {
            float lx = layerStepperX();
            if (x >= lx && x < lx + arrowWidth) return {Zone::LayerDown};
            if (x >= lx + 64 - arrowWidth && x < lx + 64) return {Zone::LayerUp};
        }
        if (y < fieldsTop) return {};
        int offset = static_cast<int>((y - fieldsTop) / rowHeight);
        int row = fieldFirst + offset;
        if (offset >= fieldVisible || row >= fieldCount) return {};
        float sx = stepperX();
        int part = x < sx ? 2 : x < sx + arrowWidth ? -1 : x >= sx + stepperWidth() - arrowWidth ? 1 : 0;
        return {Zone::Field, row, part};
    }
};
}
