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
}
