#include "features/recipes/ItemListLayout.h"
#include "features/recipes/TakePress.h"
#include <vector>
void check(bool, char const*);
void takePressTests() {
    using namespace lamium::recipes::take;
    check(shiftPress(List::CanMake, true) == ShiftPress::PlaceMany && shiftPress(List::CanMake, false) == ShiftPress::PlaceMany,
          "\"can be made\" always places as many as divide");
    check(shiftPress(List::All, true) == ShiftPress::TakeStack, "\"All\" takes a stack where taking is allowed");
    check(shiftPress(List::All, false) == ShiftPress::Recipes, "\"All\" shows the recipes elsewhere and never places");
    check(shiftPress(List::Favorites, true) == ShiftPress::TakeStack, "favorites take in creative");
    check(shiftPress(List::Favorites, false) == ShiftPress::PlaceMany, "favorites place as many outside creative");
    check(shiftPress(List::Plan, true) == ShiftPress::TakeStack, "the plan's list takes in creative");
    check(shiftPress(List::Plan, false) == ShiftPress::Own, "a plan row keeps its own press outside creative");
    check(shiftHint(true) == ShiftHint::IntoInventory && shiftHint(false) == ShiftHint::PlaceMany, "the hint follows the mode");
    check(into(true, true) == Into::Pointer, "onto an empty pointer");
    check(into(true, false) == Into::Inventory, "a held item stays: into the inventory");
    check(into(false, true) == Into::Inventory, "Shift + press goes into the inventory");
    check(refusal(true, true, true) == Refusal::None && refusal(false, false, true) == Refusal::None, "a cell found takes");
    check(refusal(false, true, false) == Refusal::NotHere, "a screen without the creative list says so first");
    check(refusal(true, false, false) == Refusal::NotListed, "an item the creative inventory lacks");
    check(refusal(true, true, false) == Refusal::GameSearch, "a listed item without a cell: the game's search hides it");
    using lamium::recipes::hintRows;
    using Starts = std::vector<size_t>;
    check(hintRows({50, 60, 40}, 10, 200) == Starts{0}, "hints that fit keep one line");
    check(hintRows({50, 60, 40}, 10, 120) == Starts{0, 2}, "a line starts at the first part that does not fit");
    check(hintRows({100, 100, 100, 100}, 10, 120) == Starts{0, 1, 2, 3}, "they wrap onto as many lines as they need");
    check(hintRows({300, 20, 20}, 10, 100) == Starts{0, 1}, "a part wider than a line stands alone");
    check(hintRows({100, 100, 100, 100, 100}, 10, 120, 3) == Starts{0, 1, 2}, "the last line takes what is left");
    check(hintRows({}, 10, 100).empty(), "no hints, no lines");
}
