#pragma once
#include "features/recipes/BandLayout.h"
#include "input/KeptPresses.h"
#include <algorithm>
#include <cmath>
#include <vector>

// L-359: where the item list sits beside a game window, in GUI units. Pure.
namespace lamium::recipes {
struct ItemListLayout {
    static constexpr float cell = 18, bar = 14, pad = 2;
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
// The row at the bottom centre, as wide as the game's inventory window and low enough that nothing above it ever
// has to move (L-382): "open the window, arrange the panels | search field, highlight, keys" (L-398). It stays between the
// panels on every screen; the field is high enough for Japanese and letters that reach under the line.
inline constexpr float searchTop = 18; // its top, from the screen's bottom
// The key hints stand right above the row (maintainer 2026-11-24, L-360): small and faint (`hintScale`), wrapped
// onto as many lines as they need, up to `hintMostLines`, `hintLine` apart, in the room the windows above leave;
// nothing moves for them. The top of the first of `lines`.
inline constexpr int hintMostLines = 4;
inline constexpr float hintScale = .7f, hintLine = 8;
inline float hintTop(float screenHeight, int lines) { return screenHeight - searchTop - 1 - static_cast<float>(lines) * hintLine; }
// `width`: 176, or up to a button more each side where the panels leave the room (rowWidth).
inline ListBox bottomRowAt(float screenWidth, float screenHeight, float width = 176) {
    return {std::floor((screenWidth - width) / 2), screenHeight - searchTop, width, 16};
}
// The key hints (L-360): parts joined by a separator `sep` wide, wrapped onto lines of `room`, each as full as
// whole parts make it (a part wider than a line stands alone). The index of each line's first part; the last
// line takes all that is left once `most` lines are used.
inline std::vector<size_t> hintRows(std::vector<float> const& widths, float sep, float room, int most = hintMostLines) {
    std::vector<size_t> starts;
    float line = 0;
    for (size_t i = 0; i < widths.size(); ++i) {
        bool const first = starts.empty() || starts.back() == i;
        float const next = line + (first ? 0 : sep) + widths[i];
        if (starts.empty() || (next > room && !first && static_cast<int>(starts.size()) < most)) {
            starts.push_back(i);
            line = widths[i];
        } else line = next;
    }
    return starts;
}
// The row's width: as wide as the free span between the panels beside it allows, from the inventory's 176 to a
// button (18) more each side. `leftEnd`/`rightStart`: where the panels next to the row end and begin.
inline float rowWidth(float screenWidth, float leftEnd, float rightStart) {
    float const half = std::min(screenWidth / 2 - leftEnd, rightStart - screenWidth / 2) - 2;
    return std::clamp(std::floor(half) * 2, 176.f, 212.f);
}
// The row's width beside the panels placed now: those that reach into the row's height, ending left of the
// screen's middle or starting right of it.
inline float rowWidthBeside(float screenWidth, float screenHeight, std::vector<band::Placed> const& placed) {
    float leftEnd = 0, rightStart = screenWidth, top = screenHeight - searchTop, bottom = top + 16, middle = screenWidth / 2;
    for (auto const& panel : placed) {
        auto const& b = panel.box;
        if (b.y + b.h <= top || b.y >= bottom) continue;
        if (b.x + b.w <= middle) leftEnd = std::max(leftEnd, b.x + b.w);
        else if (b.x >= middle) rightStart = std::min(rightStart, b.x);
    }
    return rowWidth(screenWidth, leftEnd, rightStart);
}
inline ListBox windowButtonAt(ListBox row) { return {row.x, row.y, row.h, row.h}; }
inline ListBox layoutButtonAt(ListBox row) { return {row.x + row.h + 2, row.y, row.h, row.h}; }
inline ListBox searchFieldAt(float screenWidth, float screenHeight, float width = 176) {
    auto const row = bottomRowAt(screenWidth, screenHeight, width);
    return {row.x + 2 * row.h + 6, row.y, row.w - 4 * row.h - 10, row.h};
}
// The highlight switch (L-378): a square of the field's height right of it.
inline ListBox searchHighlightAt(ListBox field) { return {field.x + field.w + 2, field.y, field.h, field.h}; }
// The keys button (L-398) at the row's end, right of the highlight switch: the key hints as its tip.
inline ListBox keysButtonAt(ListBox field) { return {field.x + field.w + field.h + 4, field.y, field.h, field.h}; }
// Where a press takes the field: a few units around it too, the screen's edge being close (L-377).
inline ListBox searchFieldHit(ListBox field) { return {field.x - 4, field.y - 4, field.w + 8, field.h + 6}; }
// What the listeners keep from the game for the row: the field and a few units around it, its two switches, and
// from the row's start where the window button shows (the layout button never shows without it).
inline ListBox rowKeptArea(ListBox field, ListBox row, bool windowButton) {
    auto const hit = searchFieldHit(field), end = keysButtonAt(field);
    float const from = windowButton ? row.x - 2 : hit.x;
    return ListBox{from, hit.y, end.x + end.w + 2 - from, hit.h};
}
// Where the key hints go: wrapped right above the row while their `lines` stay below `above` (what stands over
// them ends there); else, with a window open, in its last line as one line; else not at all.
enum class HintPlace { None, Lines, OneLine };
inline HintPlace hintPlace(int lines, float screenHeight, float above, bool open) {
    if (lines && hintTop(screenHeight, lines) >= above) return HintPlace::Lines;
    if (lines && open) return HintPlace::OneLine;
    return HintPlace::None;
}
// What stands over the hints: an open window (its bottom), else the game's.
inline float hintsAbove(bool open, float viewBottom, float windowBottom) { return open ? viewBottom + 1 : windowBottom + 2; }
// A wrapped line `wide` at hintScale, shrunk to fit `room`; the one line in a window's last line, shrunk from full
// size, no smaller than .6.
inline float hintLineScale(float wide, float room) { return wide > room ? hintScale * room / wide : hintScale; }
inline float hintOneLineScale(float wide, float room) { return wide > room ? std::max(.6f, room / wide) : 1.f; }

// A panel of the bands as the favorites (cells under a heading) or as the plan (its tabs, then rows); one
// alone has the whole panel (sidePanelIn).
struct SideLayout {
    static constexpr float cell = 18, head = 14, row = 12, pad = 2;
    float left = 0, top = 0, width = 0, height = 0;
    int columns = 0;
    // Which halves are in use; one alone has the whole band.
    bool favoritesOn = true, planOn = true;
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
    int rows() const { return std::max(0, static_cast<int>((height - (split() + 5 + bar) - 2) / row)); }
    float rowY(int index) const { return rowsTop() + static_cast<float>(index) * row; }
    int hitRow(float x, float y) const {
        if (!has(x, y) || y < rowsTop()) return -1;
        int const line = static_cast<int>((y - rowsTop()) / row);
        return line < rows() ? line : -1;
    }
    // Names fit beside the icons only where the band is wide.
    bool wide() const { return width >= 75; }
};
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
}
