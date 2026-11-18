#pragma once
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
inline constexpr float searchTop = 16; // its top, from the screen's bottom
inline ListBox searchFieldAt(float screenWidth, float screenHeight) { return {(screenWidth - 176) / 2, screenHeight - searchTop, 176, 14}; }
// The band left of the game's window, from the screen's edge: favourites in its upper half, the wanted
// items in the lower. The lower half is a bar, rows, and a last line kept free for the open container's
// control, so the rows never shift.
struct SideLayout {
    static constexpr float cell = 18, head = 14, row = 12, pad = 2;
    float left = 0, top = 0, width = 0, height = 0;
    int columns = 0;
    bool usable() const { return columns > 0 && height >= 120; }
    bool has(float x, float y) const { return usable() && x >= left && x < left + width && y >= top && y < top + height; }
    float split() const { return std::floor(height * .5f); }
    int favouriteRows() const { return std::max(0, static_cast<int>((split() - head - pad) / cell)); }
    int favourites() const { return columns * favouriteRows(); }
    float cellX(int index) const { return left + pad + static_cast<float>(index % columns) * cell; }
    float cellY(int index) const { return top + head + static_cast<float>(index / columns) * cell; }
    int hitFavourite(float x, float y) const {
        if (!usable() || x < left + pad || y < top + head) return -1;
        int const column = static_cast<int>((x - left - pad) / cell), line = static_cast<int>((y - top - head) / cell);
        return column < columns && line < favouriteRows() ? line * columns + column : -1;
    }
    float barY() const { return top + split() + 3; }
    float rowsTop() const { return top + split() + 16; }
    // The last 18 units are the open container's line: a rule, the name and switch, and room under them.
    static constexpr float foot = 18;
    float footY() const { return top + height - 16; }
    int rows() const { return std::max(0, static_cast<int>((height - (split() + 16) - foot) / row)); }
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
