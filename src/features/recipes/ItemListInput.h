#pragma once
#include "features/recipes/ItemListLayout.h"
#include "features/recipes/RecipeView.h"
#include <algorithm>
#include <vector>

// L-399: the item list's decisions about input, as pure functions with tests. The listeners run between client
// ticks and ask what the last frame showed; the frame asks them for its own press and keys.
namespace lamium::recipes::item_input {
// What the last frame showed where the pointer can be, for the listeners.
struct Boxes {
    ItemListLayout list;
    RecipeViewLayout view;
    bool viewOpen = false;
    SideLayout side;            // the plan's list
    ListBox field;              // the search row as the listeners keep it
    std::vector<ListBox> bands; // the other panels and the open container's line
    ListBox favorites;
    ListBox window;             // an open Notes window's span, wider than the recipe view's (L-369)
};
// An open window's span as drawn, from `left` to `right` (wider than the recipe view where the Recipes window is).
inline ListBox windowSpan(RecipeViewLayout const& view, float left, float right) {
    float const from = std::min(left, view.left), to = std::max(right, view.left + RecipeViewLayout::width);
    return {from, view.top, to - from, view.height};
}
// The pointer over an open window drawn from `left` to `right`, its tabs included: the game's slots under it are
// covered and name no item.
inline bool covered(RecipeViewLayout const& view, float left, float right, float x, float y) {
    return x >= left && x < right && y >= view.top - RecipeViewLayout::tabsAbove && y < view.top + view.height;
}
enum class Wheel { Wanted, View, List, Side, Favorites, Kept, Game };
// Who a wheel turn goes to. The wanted key held takes it wherever the pointer is; an open view over the game's
// window keeps it from the game's slots also where nothing of Lamium's takes it.
inline Wheel wheel(Boxes const& b, bool wantedHeld, float x, float y) {
    if (wantedHeld) return Wheel::Wanted;
    if (b.viewOpen && (b.view.has(x, y) || b.window.has(x, y))) return Wheel::View;
    if (b.list.has(x, y)) return Wheel::List;
    if (b.side.has(x, y)) return Wheel::Side;
    if (b.favorites.has(x, y)) return Wheel::Favorites;
    return b.viewOpen ? Wheel::Kept : Wheel::Game;
}
// A mouse press stays from the game: with a view open anywhere, else over the list, the search row or a panel.
inline bool ownsPress(Boxes const& b, float x, float y) {
    return b.viewOpen || b.list.has(x, y) || b.field.has(x, y)
        || std::any_of(b.bands.begin(), b.bands.end(), [&](ListBox const& box) { return box.has(x, y); });
}
// The bottom row's controls (L-382, L-398) as this frame shows them.
struct RowShown {
    bool recipesOn = false;    // the search field and its highlight switch
    bool windowButton = false; // open the Recipes window
    bool layoutButton = false; // arrange the panels
    bool editing = false;      // the panels are being arranged
};
enum class RowControl { None, Highlight, Window, Layout, Field };
// Which of the row's controls takes a press (either button), the first that has the point: the highlight switch,
// the window and layout buttons (not while arranging, where every press is the editor's), then the field with a
// few units around it.
inline RowControl rowControl(RowShown const& s, ListBox field, ListBox row, float x, float y) {
    if (s.recipesOn && searchHighlightAt(field).has(x, y)) return RowControl::Highlight;
    if (s.windowButton && !s.editing && windowButtonAt(row).has(x, y)) return RowControl::Window;
    if (s.layoutButton && !s.editing && layoutButtonAt(row).has(x, y)) return RowControl::Layout;
    if (s.recipesOn && searchFieldHit(field).has(x, y)) return RowControl::Field;
    return RowControl::None;
}
// While arranging, a left press on the layout button ends it, as Done does (L-382).
inline bool endsArranging(RowShown const& s, bool leftPress, ListBox row, float x, float y) {
    return s.editing && leftPress && s.layoutButton && layoutButtonAt(row).has(x, y);
}
// The keys button's tip (L-398): a row per key that does something in this mode with these features, in its
// order. The presses come with Recipes, the take keys in creative with it; a key without a binding has no row.
struct KeyTipOn { bool recipes = false, favorites = false, plan = false, viewOpen = false, taking = false; };
struct KeysBound { bool favorite = false, wanted = false, back = false, takeOne = false, takeStack = false; };
enum class KeyRow { Click, RightClick, ShiftClick, Favorite, Wanted, Back, TakeOne, TakeStack };
inline std::vector<KeyRow> keyRows(KeyTipOn const& on, KeysBound const& bound) {
    std::vector<KeyRow> rows;
    if (on.recipes) rows.insert(rows.end(), {KeyRow::Click, KeyRow::RightClick, KeyRow::ShiftClick});
    if (on.favorites && bound.favorite) rows.push_back(KeyRow::Favorite);
    if (on.plan && bound.wanted) rows.push_back(KeyRow::Wanted);
    if (on.viewOpen && bound.back) rows.push_back(KeyRow::Back);
    if (on.recipes && on.taking && bound.takeOne) rows.push_back(KeyRow::TakeOne);
    if (on.recipes && on.taking && bound.takeStack) rows.push_back(KeyRow::TakeStack);
    return rows;
}
// A key while a wanted item's count is typed (L-370), with the search field's keys: Esc leaves the count as it
// was, Enter and Tab keep it (both end the frame's keys); any other key goes to the digits (CountEdit.h).
enum class CountKey { Cancel, Keep, Erase, Left, Right, Home, End, SelectAll, Digits };
constexpr CountKey countKey(int code, bool ctrl) {
    if (code == 0x1B) return CountKey::Cancel;
    if (code == 0x0D || code == 0x09) return CountKey::Keep;
    if (code == 0x2E) return CountKey::Erase;
    if (code == 0x25) return CountKey::Left;
    if (code == 0x27) return CountKey::Right;
    if (code == 0x24) return CountKey::Home;
    if (code == 0x23) return CountKey::End;
    if (ctrl && code == 0x41) return CountKey::SelectAll;
    return CountKey::Digits;
}
// A key while the search field has the focus (L-377): Esc and Enter leave it, Ctrl + A or F selects it all, Up
// and Down step through earlier searches where there are any (`history`). Letters come as text, not keys.
enum class SearchKey { None, Leave, Backspace, Erase, Left, Right, Home, End, SelectAll, Earlier, Later };
constexpr SearchKey searchKey(int code, bool ctrl, bool history) {
    if (code == 0x1B || code == 0x0D) return SearchKey::Leave;
    if (code == 0x08) return SearchKey::Backspace;
    if (code == 0x2E) return SearchKey::Erase;
    if (code == 0x25) return SearchKey::Left;
    if (code == 0x27) return SearchKey::Right;
    if (code == 0x24) return SearchKey::Home;
    if (code == 0x23) return SearchKey::End;
    if (ctrl && (code == 0x41 || code == 0x46)) return SearchKey::SelectAll;
    if ((code == 0x26 || code == 0x28) && history) return code == 0x26 ? SearchKey::Earlier : SearchKey::Later;
    return SearchKey::None;
}
}
