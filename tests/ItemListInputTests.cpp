#include "features/recipes/ItemListInput.h"
void check(bool, char const*);
void itemListInputTests() {
    using namespace lamium::recipes;
    using namespace lamium::recipes::item_input;
    Boxes b;
    b.list = itemListIn({400, 20, 100, 200});
    b.view = recipeViewAt(500, 300);
    b.side = sidePanelIn({0, 120, 80, 100}, false);
    b.favorites = {0, 20, 80, 100};
    b.field = {160, 280, 180, 16};
    b.bands = {{0, 20, 80, 100}, {0, 120, 80, 100}};
    float const viewX = b.view.left + 10, viewY = b.view.top + 10;
    check(wheel(b, false, viewX, viewY) == Wheel::Game, "a closed view takes no wheel: the game's window under it scrolls");
    b.viewOpen = true;
    check(wheel(b, false, viewX, viewY) == Wheel::View, "the wheel over an open view turns the view");
    check(wheel(b, true, viewX, viewY) == Wheel::Wanted && wheel(b, true, 410, 40) == Wheel::Wanted && wheel(b, true, 1, 1) == Wheel::Wanted,
          "the wanted key held takes the wheel wherever the pointer is");
    check(wheel(b, false, 410, 40) == Wheel::List, "beside an open view the list turns its pages");
    check(wheel(b, false, 10, 150) == Wheel::Side, "the plan's list scrolls");
    check(wheel(b, false, 10, 40) == Wheel::Favorites, "the favorites scroll");
    check(wheel(b, false, 499, 1) == Wheel::Kept, "an open view keeps the wheel from the game's slots where nothing takes it");
    b.window = {100, b.view.top, 300, b.view.height};
    check(wheel(b, false, 101, viewY) == Wheel::View, "over the Notes window's wider span, the window's");
    b.viewOpen = false;
    check(wheel(b, false, 101, viewY) == Wheel::Game, "the window's span takes nothing once it is closed");
    check(wheel(b, false, 499, 1) == Wheel::Game, "with no view open the wheel elsewhere is the game's");
    check(!ownsPress(b, 499, 1), "a press on the game's window is the game's");
    check(ownsPress(b, 410, 40) && ownsPress(b, 170, 285) && ownsPress(b, 10, 150), "the list, the search row and a panel keep a press");
    b.viewOpen = true;
    check(ownsPress(b, 499, 1), "an open view keeps every press from the game");
}
