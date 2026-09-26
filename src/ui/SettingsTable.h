#pragma once
#include <algorithm>
#include <cmath>

namespace lamium::ui {
// Geometry of the settings table in GUI units, shared by drawing and hit
// testing. A 16:9 window gets a sidebar and full columns; narrower windows
// narrow the key column, then move categories into tabs; short windows keep a
// one-line description. Scrolling is explicit: callers own the first row.
struct SettingsTable {
    static constexpr float headerHeight = 20, tabsHeight = 14, theadHeight = 12, rowHeight = 14;
    static constexpr float navItemHeight = 14, sidebarWidth = 112, pad = 6, gap = 6;
    static constexpr float stateWidth = 24, closeWidth = 58;
    enum class Zone { None, Search, Close, Nav, Row, Footer };
    enum class Column { Name, State, Key };
    struct Hit { Zone zone = Zone::None; int index = -1; Column column = Column::Name; float x = 0, y = 0; };

    float left{}, top{}, width{}, height{};
    bool compact{}, shortFooter{};
    float searchX{}, searchWidth{}, closeX{};
    float navTop{}, navBottom{};
    float tableLeft{}, tableWidth{}, theadTop{}, rowsTop{}, footerTop{};
    float nameX{}, stateX{}, keyX{}, keyWidth{};
    int first{}, visible{};
    int count{};

    static SettingsTable fit(float screenWidth, float screenHeight, int count, int first) {
        SettingsTable t;
        if (!std::isfinite(screenWidth) || !std::isfinite(screenHeight) || screenWidth < 240 || screenHeight < 150)
            return t;
        t.count = std::max(count, 0);
        t.width = std::min(640.0f, screenWidth - 16);
        t.height = std::min(380.0f, screenHeight - 12);
        t.left = std::round((screenWidth - t.width) * .5f);
        t.top = std::round((screenHeight - t.height) * .5f);
        t.compact = t.width < 440;
        t.shortFooter = t.height < 230;
        t.closeX = t.left + t.width - pad - closeWidth;
        t.searchWidth = std::min(170.0f, t.width * .4f);
        t.searchX = t.closeX - gap - t.searchWidth;
        t.footerTop = t.top + t.height - (t.shortFooter ? 14 : 44);
        float body = t.top + headerHeight;
        t.navTop = body;
        if (t.compact) {
            t.navBottom = body + tabsHeight;
            t.tableLeft = t.left;
            t.tableWidth = t.width;
            body = t.navBottom;
        } else {
            t.navBottom = t.footerTop;
            t.tableLeft = t.left + sidebarWidth;
            t.tableWidth = t.width - sidebarWidth;
        }
        t.theadTop = body;
        t.rowsTop = body + theadHeight;
        t.keyWidth = t.tableWidth < 380 ? 70 : 88;
        t.nameX = t.tableLeft + pad;
        t.keyX = t.tableLeft + t.tableWidth - pad - t.keyWidth;
        t.stateX = t.keyX - gap - stateWidth;
        t.visible = std::max(0, static_cast<int>((t.footerTop - t.rowsTop - 2) / rowHeight));
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
    // Reset button on the column-heading line (General: all settings,
    // Hotkeys: key bindings), right-aligned before the state/key heading.
    static constexpr float headActionWidth = 84;
    float headActionX(bool hotkeys) const { return (hotkeys ? keyX : stateX - 6) - gap - headActionWidth; }
    bool headAction(float x, float y, bool hotkeys) const {
        float ax = headActionX(hotkeys);
        return usable() && y >= theadTop && y < rowsTop && x >= ax && x < ax + headActionWidth;
    }
    float rowY(int index) const { return rowsTop + (index - first) * rowHeight; }
    float rowsRight() const { return tableLeft + tableWidth; }
    // Values without a switch span the state and key columns.
    float controlX() const { return stateX; }
    float controlWidth() const { return keyX + keyWidth - stateX; }
    float navItemY(int index) const { return navTop + 4 + index * navItemHeight; }
    static constexpr int pinnedItems = 3;
    float pinnedItemY(int k) const { return navBottom - 4 - (pinnedItems - k) * navItemHeight; }
    // Binding-editor buttons (Clear / Reset / Cancel) on the footer's first line.
    static constexpr float footerButtonWidth = 50, footerButtonHeight = 11;
    float footerButtonX(int index) const { return left + pad + index * (footerButtonWidth + 4); }
    float footerButtonY() const { return footerTop + 3; }
    int footerButton(float x, float y) const {
        if (y < footerButtonY() || y >= footerButtonY() + footerButtonHeight) return -1;
        for (int i = 0; i < 3; ++i)
            if (x >= footerButtonX(i) && x < footerButtonX(i) + footerButtonWidth) return i;
        return -1;
    }
    // Numeric and choice steppers, right-aligned in the value columns. A
    // keyed row keeps its key cell, so its stepper ends before the key column.
    float stepperWidth(bool keyed = false) const { return keyed ? 96.0f : std::min(96.0f, controlWidth()); }
    float stepperX(bool keyed = false) const {
        return keyed ? keyX - gap - stepperWidth(true) : keyX + keyWidth - stepperWidth();
    }
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
    int stepperPart(float x, bool keyed = false) const {
        float sx = stepperX(keyed), sw = stepperWidth(keyed);
        if (x < sx || x >= sx + sw) return 2;
        if (x < sx + arrowWidth) return -1;
        if (x >= sx + sw - arrowWidth) return 1;
        return 0;
    }

    Hit hit(float x, float y, int navItems, float tabWidth = 0) const {
        auto result = hitZone(x, y, navItems, tabWidth);
        result.x = x; result.y = y;
        return result;
    }
    Hit hitZone(float x, float y, int navItems, float tabWidth) const {
        if (!usable() || !std::isfinite(x) || !std::isfinite(y)) return {};
        if (x < left || x >= left + width || y < top || y >= top + height) return {};
        if (y < top + headerHeight) {
            if (x >= closeX && x < closeX + closeWidth) return {Zone::Close};
            if (x >= searchX && x < searchX + searchWidth) return {Zone::Search};
            return {};
        }
        if (y >= footerTop) return {Zone::Footer};
        if (compact) {
            if (y < navBottom) {
                if (tabWidth <= 0) return {};
                int index = static_cast<int>((x - left - 2) / tabWidth);
                return index >= 0 && index < navItems ? Hit{Zone::Nav, index} : Hit{};
            }
        } else if (x < tableLeft) {
            // The last items are tools pinned to the sidebar bottom (Hotkeys, Shapes, HUD layout).
            for (int k = 0; k < pinnedItems && k < navItems; ++k) {
                int index = navItems - pinnedItems + k;
                float itemTop = pinnedItemY(k);
                if (y >= itemTop && y < itemTop + navItemHeight) return {Zone::Nav, index};
            }
            int index = static_cast<int>(std::floor((y - navTop - 4) / navItemHeight));
            return index >= 0 && index < navItems - pinnedItems ? Hit{Zone::Nav, index} : Hit{};
        }
        if (y < rowsTop) return {};
        int offset = static_cast<int>((y - rowsTop) / rowHeight);
        int row = first + offset;
        if (offset >= visible || row >= count) return {};
        Column column = x >= keyX ? Column::Key : x >= stateX ? Column::State : Column::Name;
        return {Zone::Row, row, column};
    }
};
}
