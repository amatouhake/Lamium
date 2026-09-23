#pragma once
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
    enum class Zone { None, Close, Dock, Keys, DrawAll, NewShape, ListRow, Name, LayerDown, LayerUp, Field, Action, Pick, Footer };
    struct Hit { Zone zone = Zone::None; int index = -1; int part = 0; };

    bool docked{}, draft{}, picking{};
    float left{}, top{}, width{}, height{};
    float closeX{}, dockX{}, keysX{}, drawAllX{}, drawAllY{};
    float listLeft{}, listWidth{}, toolbarTop{}, theadTop{}, rowsTop{};
    int listVisible{}, listFirst{}, listCount{};
    float detailLeft{}, detailWidth{}, detailTop{}, nameY{}, previewY{}, fieldsTop{}, actionsY{}, footerTop{};
    int fieldVisible{}, fieldFirst{}, fieldCount{};

    static ShapesLayout fit(SettingsTable const& table, float screenWidth, float screenHeight, bool docked,
                            int listCount, int listFirst, int fieldCount, int fieldFirst, bool draft, bool picking) {
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
        l.drawAllY = docked ? l.toolbarTop + 2 : l.top + 4;
        l.theadTop = bodyTop + toolbarHeight;
        l.rowsTop = l.theadTop + theadHeight;
        if (docked) {
            l.listVisible = dockedListRows;
            l.detailLeft = l.left; l.detailWidth = l.width;
            l.detailTop = l.rowsTop + dockedListRows * rowHeight + 2;
        } else {
            l.listVisible = std::max(0, static_cast<int>((bodyBottom - l.rowsTop - 2) / rowHeight));
            l.detailTop = bodyTop;
        }
        l.listFirst = std::clamp(listFirst, 0, std::max(0, l.listCount - l.listVisible));
        l.nameY = l.detailTop + 4 + (draft ? 12 : 0);
        l.previewY = l.nameY + rowHeight + 4;
        l.fieldsTop = picking ? l.nameY + rowHeight : l.previewY + previewSize + 4;
        l.actionsY = bodyBottom - actionsHeight - 2;
        float pitch = picking ? pickHeight : rowHeight;
        l.fieldVisible = std::max(0, static_cast<int>((l.actionsY - 2 - l.fieldsTop) / pitch));
        l.fieldFirst = std::clamp(fieldFirst, 0, std::max(0, l.fieldCount - l.fieldVisible));
        return l;
    }
    bool usable() const { return width > 0 && fieldVisible > 0; }
    float listRowY(int index) const { return rowsTop + (index - listFirst) * rowHeight; }
    float fieldY(int index) const { return fieldsTop + (index - fieldFirst) * (picking ? pickHeight : rowHeight); }
    // Values are right-aligned steppers in the editor.
    float stepperWidth() const { return std::min(118.0f, detailWidth - 2 * pad - 70); }
    float stepperX() const { return detailLeft + detailWidth - pad - stepperWidth(); }
    static constexpr float arrowWidth = 11;
    float layerStepperX() const { return detailLeft + pad + previewSize + 8; }
    float layerY() const { return previewY + 30; }
    static constexpr float actionWidth = 56, deleteWidth = 96;
    float actionX(int index) const { return detailLeft + pad + index * (actionWidth + 4); }
    // The delete action sits at the right edge.
    float deleteX() const { return detailLeft + detailWidth - pad - deleteWidth; }

    Hit hit(float x, float y) const {
        if (!usable() || !std::isfinite(x) || !std::isfinite(y)) return {};
        if (x < left || x >= left + width || y < top || y >= top + height) return {};
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
            if (y >= toolbarTop && y < toolbarTop + toolbarHeight) {
                if (docked && x >= drawAllX && x < drawAllX + drawAllWidth) return {Zone::DrawAll};
                return x < listLeft + pad + newWidth ? Hit{Zone::NewShape} : Hit{};
            }
            if (y < rowsTop) return {};
            int offset = static_cast<int>((y - rowsTop) / rowHeight);
            int row = listFirst + offset;
            return offset < listVisible && row < listCount ? Hit{Zone::ListRow, row} : Hit{};
        }
        if (x < detailLeft || x >= detailLeft + detailWidth || y < detailTop) return {};
        if (y >= actionsY && y < actionsY + actionsHeight) {
            // Draft: Create, Cancel. Existing shape: Duplicate, then Delete at the right edge.
            if (x >= actionX(0) && x < actionX(0) + actionWidth) return {Zone::Action, 0};
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
