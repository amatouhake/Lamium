#pragma once
#include "ui/ScrollBar.h"
#include <algorithm>
#include <cmath>

namespace lamium::ui {
// Geometry of the settings table in layout units, shared by drawing and hit
// testing. There is one layout: sidebar, columns and a two-line footer. A GUI
// too small for it draws the whole screen shrunk (scaleFor, ui::DrawScale)
// instead of a second, cramped layout. Narrower layouts narrow the key column.
// Scrolling is explicit: callers own the first row.
struct SettingsTable {
    static constexpr float headerHeight = 20, theadHeight = 12, rowHeight = 14, footerHeight = 44;
    static constexpr float navItemHeight = 14, sidebarWidth = 112, pad = 6, gap = 6;
    static constexpr float stateWidth = 24, closeWidth = 58;
    static constexpr float descriptionLineHeight = 12;
    static constexpr int descriptionLines = 2; // the footer's; longer text scrolls
    enum class Zone { None, Search, Close, Nav, Row, Footer, Version };
    enum class Column { Name, State, Key };
    struct Hit { Zone zone = Zone::None; int index = -1; Column column = Column::Name; float x = 0, y = 0; };

    float left{}, top{}, width{}, height{};
    float searchX{}, searchWidth{}, closeX{};
    float navTop{}, navBottom{};
    float navStep = navItemHeight; // a sidebar item's height: less when a short window must fit them all
    float tableLeft{}, tableWidth{}, theadTop{}, rowsTop{}, footerTop{};
    float nameX{}, stateX{}, keyX{}, keyWidth{};
    // The version after the title (L-101); placed by placeVersion from measured text.
    float versionX{}, versionWidth{};
    int first{}, visible{};
    int count{};

    // `navItems` counts the sidebar's items; each keeps at least this height.
    static constexpr float minNavStep = 11, minWidth = 440, margin = 16, marginY = 12;
    static float minHeight(int navItems) {
        // Top items, the pinned ones and the rule between them.
        return std::max(230.0f, footerHeight + headerHeight + 12 + std::max(navItems, 0) * minNavStep);
    }
    // The draw scale that keeps this layout whole on a screen of this size:
    // 1 when it fits, below 1 to shrink it. Never enlarges.
    static float scaleFor(float screenWidth, float screenHeight, int navItems) {
        if (!(screenWidth > 0) || !(screenHeight > 0) || !std::isfinite(screenWidth) || !std::isfinite(screenHeight)) return 1;
        float scale = std::min({1.0f, screenWidth / (minWidth + margin), screenHeight / (minHeight(navItems) + marginY)});
        // Leave rounding room so the scaled-up size never falls just short.
        return scale < 1 ? scale * .999f : 1;
    }
    static SettingsTable fit(float screenWidth, float screenHeight, int count, int first, int navItems = 0) {
        SettingsTable t;
        if (!std::isfinite(screenWidth) || !std::isfinite(screenHeight) || screenWidth < minWidth + margin
            || screenHeight < minHeight(navItems) + marginY)
            return t;
        t.count = std::max(count, 0);
        t.width = std::min(640.0f, screenWidth - margin);
        t.height = std::min(380.0f, screenHeight - marginY);
        t.left = std::round((screenWidth - t.width) * .5f);
        t.top = std::round((screenHeight - t.height) * .5f);
        if (navItems > 0) {
            float room = t.height - footerHeight - headerHeight - 8 - 4;
            t.navStep = std::min(navItemHeight, std::floor(room / static_cast<float>(navItems)));
        }
        t.closeX = t.left + t.width - pad - closeWidth;
        t.searchWidth = std::min(170.0f, t.width * .4f);
        t.searchX = t.closeX - gap - t.searchWidth;
        t.footerTop = t.top + t.height - footerHeight;
        float body = t.top + headerHeight;
        t.navTop = body;
        t.navBottom = t.footerTop;
        t.tableLeft = t.left + sidebarWidth;
        t.tableWidth = t.width - sidebarWidth;
        t.theadTop = body;
        t.rowsTop = body + theadHeight;
        // The key column grows with a wide table (L-372), so long chords fit where there is room.
        t.keyWidth = t.tableWidth < 380 ? 70 : std::clamp(t.tableWidth * .28f, 88.f, 150.f);
        t.nameX = t.tableLeft + pad;
        t.visible = std::max(0, static_cast<int>((t.footerTop - t.rowsTop - 2) / rowHeight));
        t.keyX = t.tableLeft + t.contentWidth() - pad - t.keyWidth;
        t.stateX = t.keyX - gap - stateWidth;
        t.first = clampFirst(first, t.count, t.visible);
        return t;
    }
    static int clampFirst(int first, int count, int visible) {
        return std::clamp(first, 0, std::max(0, count - visible));
    }
    // Minimal scroll that keeps a row visible, used for keyboard navigation.
    static int reveal(int first, int row, int visible) {
        if (visible <= 0) return first;
        if (row < first) return row;
        if (row >= first + visible) return row - visible + 1;
        return first;
    }
    bool usable() const { return visible > 0; }
    // Scrollbars in the right padding of the list and of the footer (the
    // press zones; the bar and its drag are the shared ones, ScrollBar.h).
    bool onListScrollbar(float x, float y) const {
        return count > visible && x >= rowsRight() - 3 - barReach && x < rowsRight() && y >= rowsTop && y < rowsTop + visible * rowHeight;
    }
    bool onDescriptionScrollbar(float x, float y, int lines) const {
        float right = left + width;
        return lines > descriptionLines && x >= right - pad - 2 && x < right
            && y >= footerTop + 3 && y < footerTop + 3 + descriptionLines * descriptionLineHeight;
    }
    // Puts the version after the title; hidden when it would reach the search field.
    void placeVersion(float titleWidth, float textWidth) {
        versionX = left + pad + titleWidth + 4;
        versionWidth = usable() && textWidth > 0 && versionX + textWidth <= searchX - gap ? textWidth : 0;
    }
    // Reset button on the column-heading line (General: all settings,
    // Hotkeys: key bindings), right-aligned before the state heading; in
    // Hotkeys, Find by key sits between it and the key heading (L-159).
    static constexpr float headActionWidth = 84, keyFindWidth = 64;
    float keyFindX() const { return keyX - gap - keyFindWidth; }
    float headActionX(bool hotkeys) const { return (hotkeys ? keyFindX() : stateX - 6) - gap - headActionWidth; }
    bool headAction(float x, float y, bool hotkeys) const {
        float ax = headActionX(hotkeys);
        return usable() && y >= theadTop && y < rowsTop && x >= ax && x < ax + headActionWidth;
    }
    bool keyFindShown() const { return usable() && headActionX(true) > nameX + 40; }
    bool keyFind(float x, float y) const {
        return keyFindShown() && y >= theadTop && y < rowsTop && x >= keyFindX() && x < keyFindX() + keyFindWidth;
    }
    float rowY(int index) const { return rowsTop + (index - first) * rowHeight; }
    float rowsRight() const { return tableLeft + tableWidth; }
    // Rows and their columns stop short of the list's bar while it shows.
    float contentWidth() const { return tableWidth - (count > visible ? rowBarGutter : 0); }
    float rowWidth() const { return contentWidth() - 2; }
    // Values without a switch span the state and key columns.
    float controlX() const { return stateX; }
    float controlWidth() const { return keyX + keyWidth - stateX; }
    float navItemY(int index) const { return navTop + 4 + index * navStep; }
    static constexpr int pinnedItems = 6; // Hotkeys, Shapes, Waypoints, Schematics, World map, HUD layout.
    float pinnedItemY(int k) const { return navBottom - 4 - (pinnedItems - k) * navStep; }
    // Binding-editor buttons (Clear / Reset / Cancel) on the edited row, ending
    // just before its key cell (L-159).
    static constexpr float captureButtonWidth = 40, captureButtonGap = 3;
    float captureButtonX(int index, int count) const {
        return keyX - gap - (count - index) * (captureButtonWidth + captureButtonGap) + captureButtonGap;
    }
    int captureButton(float x, float y, float rowTop, int count) const {
        if (y < rowTop + 1.5f || y >= rowTop + 1.5f + footerButtonHeight) return -1;
        for (int i = 0; i < count; ++i)
            if (x >= captureButtonX(i, count) && x < captureButtonX(i, count) + captureButtonWidth) return i;
        return -1;
    }
    // Find by key's Cancel on the footer's first line.
    static constexpr float footerButtonWidth = 50, footerButtonHeight = 11;
    float descriptionHintY() const { return footerTop + 30; }
    float footerButtonX(int index) const { return left + pad + index * (footerButtonWidth + 4); }
    float footerButtonY() const { return footerTop + 3; }
    int footerButton(float x, float y) const {
        if (y < footerButtonY() || y >= footerButtonY() + footerButtonHeight) return -1;
        for (int i = 0; i < 3; ++i)
            if (x >= footerButtonX(i) && x < footerButtonX(i) + footerButtonWidth) return i;
        return -1;
    }
    // Numeric and choice steppers: one width on every row, ending before the
    // key column whether or not the row has a key (L-349), so values line up
    // and never run into a key cell. A long value is cut with "...".
    static constexpr float stepperColumnWidth = 96;
    float stepperWidth() const { return stepperColumnWidth; }
    float stepperX() const { return keyX - gap - stepperColumnWidth; }
    static constexpr float arrowWidth = 11;
    // Sliders span the state and key columns: track left, value right.
    static constexpr float sliderValueWidth = 50;
    float sliderX() const { return controlX(); }
    float sliderWidth() const { return std::max(0.0f, controlWidth() - sliderValueWidth - 4); }
    float sliderValueX() const { return controlX() + controlWidth() - sliderValueWidth; }
    // Fraction of the track under x (0-1), or -1 when x is on the value text.
    float sliderFraction(float x) const {
        if (x >= sliderValueX()) return -1;
        float usable = sliderWidth() - 6; // knob width
        if (!(usable > 0)) return 0;
        return std::clamp((x - sliderX() - 3) / usable, 0.0f, 1.0f);
    }
    static float sliderValue(float fraction, float minimum, float maximum, float step) {
        float value = minimum + (maximum - minimum) * std::clamp(fraction, 0.0f, 1.0f);
        if (step > 0) value = minimum + std::round((value - minimum) / step) * step;
        return std::clamp(value, minimum, maximum);
    }
    static float sliderPosition(float value, float minimum, float maximum) {
        if (!(maximum > minimum) || !std::isfinite(value)) return 0;
        return std::clamp((value - minimum) / (maximum - minimum), 0.0f, 1.0f);
    }
    // -1 decrease, 1 increase, 0 value, 2 outside.
    int stepperPart(float x) const {
        float sx = stepperX(), sw = stepperWidth();
        if (x < sx || x >= sx + sw) return 2;
        if (x < sx + arrowWidth) return -1;
        if (x >= sx + sw - arrowWidth) return 1;
        return 0;
    }

    Hit hit(float x, float y, int navItems) const {
        auto result = hitZone(x, y, navItems);
        result.x = x; result.y = y;
        return result;
    }
    Hit hitZone(float x, float y, int navItems) const {
        if (!usable() || !std::isfinite(x) || !std::isfinite(y)) return {};
        if (x < left || x >= left + width || y < top || y >= top + height) return {};
        if (y < top + headerHeight) {
            if (x >= closeX && x < closeX + closeWidth) return {Zone::Close};
            if (x >= searchX && x < searchX + searchWidth) return {Zone::Search};
            if (versionWidth > 0 && x >= versionX && x < versionX + versionWidth) return {Zone::Version};
            return {};
        }
        if (y >= footerTop) return {Zone::Footer};
        if (x < tableLeft) {
            // The last items are tools pinned to the sidebar bottom (Hotkeys, Shapes, HUD layout).
            for (int k = 0; k < pinnedItems && k < navItems; ++k) {
                int index = navItems - pinnedItems + k;
                float itemTop = pinnedItemY(k);
                if (y >= itemTop && y < itemTop + navStep) return {Zone::Nav, index};
            }
            int index = static_cast<int>(std::floor((y - navTop - 4) / navStep));
            return index >= 0 && index < navItems - pinnedItems ? Hit{Zone::Nav, index} : Hit{};
        }
        if (y < rowsTop || onListScrollbar(x, y)) return {};
        int offset = static_cast<int>((y - rowsTop) / rowHeight);
        int row = first + offset;
        if (offset >= visible || row >= count) return {};
        Column column = x >= keyX ? Column::Key : x >= stateX ? Column::State : Column::Name;
        return {Zone::Row, row, column};
    }
};
}
