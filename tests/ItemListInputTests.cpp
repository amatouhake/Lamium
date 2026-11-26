#include "features/recipes/ItemListInput.h"
#include <cmath>
#include <utility>
#include <vector>
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

    auto const row = bottomRowAt(500, 300), field = searchFieldAt(500, 300);
    auto centre = [](ListBox box) { return std::pair{box.x + box.w / 2, box.y + box.h / 2}; };
    auto const [hx, hy] = centre(searchHighlightAt(field));
    auto const [wx, wy] = centre(windowButtonAt(row));
    auto const [lx, ly] = centre(layoutButtonAt(row));
    auto const [fx, fy] = centre(field);
    RowShown const all{true, true, true, false};
    check(rowControl(all, field, row, hx, hy) == RowControl::Highlight && rowControl(all, field, row, wx, wy) == RowControl::Window
              && rowControl(all, field, row, lx, ly) == RowControl::Layout && rowControl(all, field, row, fx, fy) == RowControl::Field,
          "each of the row's controls takes a press on it");
    check(rowControl(all, field, row, field.x - 3, fy) == RowControl::Field, "the field takes a press a few units beside it");
    check(rowControl(all, field, row, 1, 1) == RowControl::None, "a press off the row is not the row's");
    check(rowControl({false, true, true, false}, field, row, hx, hy) == RowControl::None
              && rowControl({false, true, true, false}, field, row, fx, fy) == RowControl::None,
          "without Recipes neither the field nor its highlight switch takes a press");
    check(rowControl({true, false, false, false}, field, row, wx, wy) == RowControl::None, "a button not shown takes nothing");
    RowShown const arranging{true, true, true, true};
    check(rowControl(arranging, field, row, wx, wy) == RowControl::None && rowControl(arranging, field, row, lx, ly) == RowControl::None,
          "while arranging, the window and layout buttons take no press from the editor");
    check(endsArranging(arranging, true, row, lx, ly), "a left press on the layout button ends arranging");
    check(!endsArranging(arranging, false, row, lx, ly) && !endsArranging(all, true, row, lx, ly) && !endsArranging(arranging, true, row, wx, wy),
          "only a left press, only while arranging, only on the layout button");

    check(countKey(0x1B, false) == CountKey::Cancel, "Esc leaves a count as it was");
    check(countKey(0x0D, false) == CountKey::Keep && countKey(0x09, false) == CountKey::Keep, "Enter and Tab keep a count");
    check(countKey(0x41, true) == CountKey::SelectAll && countKey(0x41, false) == CountKey::Digits, "Ctrl + A selects a count, A alone is no edit");
    check(countKey(0x46, true) == CountKey::Digits, "Ctrl + F selects only the search field");
    check(countKey(0x35, false) == CountKey::Digits && countKey(0x08, false) == CountKey::Digits, "digits and Backspace go to the digits");
    check(countKey(0x25, false) == CountKey::Left && countKey(0x27, false) == CountKey::Right && countKey(0x24, false) == CountKey::Home
              && countKey(0x23, false) == CountKey::End && countKey(0x2E, false) == CountKey::Erase,
          "the caret's keys and Delete");
    check(searchKey(0x1B, false, false) == SearchKey::Leave && searchKey(0x0D, false, false) == SearchKey::Leave,
          "Esc and Enter leave the search field");
    check(searchKey(0x09, false, true) == SearchKey::None, "Tab does nothing in the search field");
    check(searchKey(0x41, true, false) == SearchKey::SelectAll && searchKey(0x46, true, false) == SearchKey::SelectAll
              && searchKey(0x46, false, false) == SearchKey::None, "Ctrl + A and Ctrl + F select the search");
    check(searchKey(0x26, false, true) == SearchKey::Earlier && searchKey(0x28, false, true) == SearchKey::Later,
          "Up steps to an earlier search, Down back");
    check(searchKey(0x26, false, false) == SearchKey::None && searchKey(0x28, false, false) == SearchKey::None,
          "without earlier searches Up and Down do nothing");
    check(searchKey(0x08, false, false) == SearchKey::Backspace && searchKey(0x2E, true, false) == SearchKey::Erase
              && searchKey(0x25, true, false) == SearchKey::Left && searchKey(0x27, true, false) == SearchKey::Right,
          "Backspace, Delete and the caret's keys, with Ctrl too");

    auto const view = recipeViewAt(500, 300);
    float const viewIn = view.top + 5;
    check(covered(view, 100, 400, 101, viewIn) && !covered(view, 100, 400, 99, viewIn) && !covered(view, 100, 400, 400, viewIn),
          "an open window covers the game's slots across its drawn width");
    check(covered(view, 100, 400, 200, view.top - RecipeViewLayout::tabsAbove) && !covered(view, 100, 400, 200, view.top - RecipeViewLayout::tabsAbove - 1)
              && !covered(view, 100, 400, 200, view.top + view.height),
          "its tabs above it count, the line under it does not");
    auto const span = windowSpan(view, 100, 400);
    check(span.x == 100 && span.w == 300 && span.y == view.top && span.h == view.height, "the Notes window's span as drawn");
    auto const narrow = windowSpan(view, view.left + 20, view.left + 40);
    check(narrow.x == view.left && narrow.w == RecipeViewLayout::width, "never narrower than the recipe view");

    using Rows = std::vector<KeyRow>;
    KeysBound const bound{true, true, true, true, true};
    check(keyRows({}, bound).empty(), "with every feature off the keys tip has no rows");
    check(keyRows({.recipes = true}, {}) == Rows{KeyRow::Click, KeyRow::RightClick, KeyRow::ShiftClick},
          "with Recipes the presses show, with no key bound");
    check(keyRows({.recipes = true, .favorites = true, .plan = true, .viewOpen = true, .taking = true}, bound)
              == Rows{KeyRow::Click, KeyRow::RightClick, KeyRow::ShiftClick, KeyRow::Favorite, KeyRow::Wanted, KeyRow::Back, KeyRow::TakeOne,
                      KeyRow::TakeStack},
          "everything on and bound, in the tip's order");
    check(keyRows({.recipes = true, .favorites = true, .plan = true, .viewOpen = true, .taking = true}, {})
              == Rows{KeyRow::Click, KeyRow::RightClick, KeyRow::ShiftClick},
          "a key without a binding has no row");
    check(keyRows({.plan = true, .taking = true}, bound) == Rows{KeyRow::Wanted}, "the take keys only with Recipes");
    check(keyRows({.recipes = true, .favorites = true}, bound) == Rows{KeyRow::Click, KeyRow::RightClick, KeyRow::ShiftClick, KeyRow::Favorite},
          "Back only with the recipe view open, the take keys only in creative");
}
void itemListLayoutTests() {
    using namespace lamium::recipes;
    using lamium::recipes::band::Placed;
    float const rowTop = 300 - searchTop;
    check(rowWidthBeside(500, 300, {}) == 212, "with no panels beside it the row takes a button more each side");
    std::vector<Placed> beside{{band::Page::Favorites, {0, rowTop - 50, 160, 60}}, {band::Page::Items, {330, 0, 170, 300}}};
    check(rowWidthBeside(500, 300, beside) == 176, "panels close beside the row keep it at the inventory's width");
    std::vector<Placed> above{{band::Page::Favorites, {0, 0, 240, rowTop}}, {band::Page::Items, {260, rowTop + 16, 240, 10}}};
    check(rowWidthBeside(500, 300, above) == 212, "panels that end above the row or start below it leave it room");
    std::vector<Placed> mid{{band::Page::Favorites, {245, rowTop, 20, 16}}};
    check(rowWidthBeside(500, 300, mid) == 212, "a panel across the middle is neither side");
    auto const row = bottomRowAt(500, 300), field = searchFieldAt(500, 300);
    auto const kept = rowKeptArea(field, row, true), bare = rowKeptArea(field, row, false), keys = keysButtonAt(field);
    check(kept.x == row.x - 2 && kept.x + kept.w == keys.x + keys.w + 2, "with the window button the row is kept from its start to past the keys button");
    check(bare.x == searchFieldHit(field).x && bare.x + bare.w == kept.x + kept.w, "without it from the field's hit box");
    check(hintPlace(2, 300, hintTop(300, 2), false) == HintPlace::Lines, "hints that end at what stands over them wrap above the row");
    check(hintPlace(2, 300, hintTop(300, 2) + 1, true) == HintPlace::OneLine, "too low with a window open: one line in its last line");
    check(hintPlace(2, 300, hintTop(300, 2) + 1, false) == HintPlace::None, "too low with none open: no hints");
    check(hintPlace(0, 300, 0, true) == HintPlace::None, "no hints, no place");
    check(hintsAbove(true, 200, 250) == 201 && hintsAbove(false, 200, 250) == 252, "under an open window, else under the game's");
    check(hintLineScale(50, 100) == hintScale && std::abs(hintLineScale(200, 100) - hintScale / 2) < 1e-5f, "a wrapped line too wide shrinks to its room");
    check(hintOneLineScale(50, 100) == 1 && hintOneLineScale(125, 100) == .8f && hintOneLineScale(500, 100) == .6f,
          "the one line shrinks to fit, no smaller than .6");
}
