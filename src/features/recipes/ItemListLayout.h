#pragma once
#include "features/recipes/BandLayout.h"
#include "input/KeptPresses.h"
#include <algorithm>
#include <cmath>
#include <vector>

// L-359: where the item list sits beside a game window, in GUI units. Pure.
namespace lamium::recipes {
struct ItemListLayout {
    static constexpr float cell = 18, bar = 14, gap = 4, pad = 2;
    float left = 0, top = 0, width = 0, height = 0;
    int columns = 0, rows = 0;
    bool usable() const { return columns > 0 && rows > 0; }
    int perPage() const { return columns * rows; }
    int pages(int count) const { return usable() ? std::max(1, (count + perPage() - 1) / perPage()) : 1; }
    bool has(float x, float y) const { return usable() && x >= left && x < left + width && y >= top && y < top + height; }
    float cellX(int index) const { return left + pad + static_cast<float>(index % columns) * cell; }
    float cellY(int index) const { return top + bar + static_cast<float>(index / columns) * cell; }
    // The cell under a point, counted from the page's first; -1 outside the cells.
    int hit(float x, float y) const {
        if (!usable() || x < left + pad || y < top + bar) return -1;
        int const column = static_cast<int>((x - left - pad) / cell), row = static_cast<int>((y - top - bar) / cell);
        return column < columns && row < rows ? row * columns + column : -1;
    }
};
using input::KeptPresses;
// A rectangle of the screen.
struct ListBox {
    float x = 0, y = 0, w = 0, h = 0;
    bool has(float px, float py) const { return w > 0 && px >= x && px < x + w && py >= y && py < y + h; }
};
// The search field: at the bottom centre, as wide as the game's inventory window and low enough that
// nothing above it ever has to move. High enough for Japanese and for letters that reach under the line.
inline constexpr float searchTop = 18; // its top, from the screen's bottom
inline ListBox searchFieldAt(float screenWidth, float screenHeight) { return {(screenWidth - 176) / 2, screenHeight - searchTop, 176, 16}; }
// The highlight switch (L-378): a square of the field's height right of it.
inline ListBox searchHighlightAt(ListBox field) { return {field.x + field.w + 2, field.y, field.h, field.h}; }
// Where a press takes the field: a few units around it too, the screen's edge being close (L-377).
inline ListBox searchFieldHit(ListBox field) { return {field.x - 4, field.y - 4, field.w + 8, field.h + 6}; }

// The band left of the game's window, from the screen's edge: favorites in its upper half, the wanted
// items in the lower. The lower half is a bar, rows, and a last line kept free for the open container's
// control, so the rows never shift.
struct SideLayout {
    static constexpr float cell = 18, head = 14, row = 12, pad = 2;
    float left = 0, top = 0, width = 0, height = 0;
    int columns = 0;
    // Which halves are in use; one alone has the whole band.
    bool favoritesOn = true, planOn = true;
    bool footOn = false; // an open container's line takes the band's last units
    bool usable() const { return columns > 0 && height >= band::minLength(band::Left); } // one row under its heading
    bool has(float x, float y) const { return usable() && x >= left && x < left + width && y >= top && y < top + height; }
    float split() const { return !favoritesOn ? 0 : !planOn ? height : std::floor(height * .5f); }
    int favoriteRows() const { return std::max(0, static_cast<int>((split() - head - pad) / cell)); }
    int favorites() const { return columns * favoriteRows(); }
    float cellX(int index) const { return left + pad + static_cast<float>(index % columns) * cell; }
    float cellY(int index) const { return top + head + static_cast<float>(index / columns) * cell; }
    int hitFavorite(float x, float y) const {
        if (!usable() || x < left + pad || y < top + head) return -1;
        int const column = static_cast<int>((x - left - pad) / cell), line = static_cast<int>((y - top - head) / cell);
        return column < columns && line < favoriteRows() ? line * columns + column : -1;
    }
    // The lists' tabs: 14 high, room for an item's picture at 14 units where the names do not fit (L-367).
    static constexpr float bar = 14;
    float barY() const { return top + split() + 3; }
    float rowsTop() const { return top + split() + 5 + bar; }
    // With a container open the last 16 units are its line: a rule, the name and switch, a little room under
    // them. Without one the rows reach the band's end.
    static constexpr float foot = 16;
    float footY() const { return top + height - 12; }
    int rows() const { return std::max(0, static_cast<int>((height - (split() + 5 + bar) - (footOn ? foot : 2)) / row)); }
    float rowY(int index) const { return rowsTop() + static_cast<float>(index) * row; }
    int hitRow(float x, float y) const {
        if (!has(x, y) || y < rowsTop()) return -1;
        int const line = static_cast<int>((y - rowsTop()) / row);
        return line < rows() ? line : -1;
    }
    // Names fit beside the icons only where the band is wide.
    bool wide() const { return width >= 75; }
};
// `right`: where the game's window, or the room the recipe view keeps, begins.
inline SideLayout itemListLeft(float screenHeight, float right) {
    SideLayout out;
    out.width = right - 4;
    out.height = screenHeight;
    out.columns = std::max(0, static_cast<int>((out.width - 2 * SideLayout::pad) / SideLayout::cell));
    return out;
}
// A panel of the bands (BandLayout.h) as the item list: a bar for the pager, then as many whole cells as fit.
inline ItemListLayout itemListIn(band::Box box) {
    ItemListLayout out;
    out.left = box.x, out.top = box.y, out.width = box.w, out.height = box.h;
    out.columns = std::max(0, static_cast<int>((out.width - 2 * ItemListLayout::pad) / ItemListLayout::cell));
    out.rows = std::max(0, static_cast<int>((out.height - ItemListLayout::bar - ItemListLayout::pad) / ItemListLayout::cell));
    return out;
}
// A panel as the favorites (cells under a heading) or as the plan (its tabs and rows): a SideLayout with
// that part alone.
inline SideLayout sidePanelIn(band::Box box, bool favorites) {
    SideLayout out;
    out.left = box.x, out.top = box.y, out.width = box.w, out.height = box.h;
    out.columns = std::max(0, static_cast<int>((out.width - 2 * SideLayout::pad) / SideLayout::cell));
    out.favoritesOn = favorites, out.planOn = !favorites;
    return out;
}
// The band from the window's right side to the screen's edge, top to bottom:
// a bar for the pager, then as many whole cells as fit.
inline ItemListLayout itemListRight(float screenWidth, float screenHeight, float windowLeft, float windowWidth) {
    ItemListLayout out;
    out.left = windowLeft + windowWidth + ItemListLayout::gap;
    out.width = screenWidth - out.left;
    out.height = screenHeight;
    out.columns = std::max(0, static_cast<int>((out.width - 2 * ItemListLayout::pad) / ItemListLayout::cell));
    out.rows = std::max(0, static_cast<int>((out.height - ItemListLayout::bar - ItemListLayout::pad) / ItemListLayout::cell));
    return out;
}
}
