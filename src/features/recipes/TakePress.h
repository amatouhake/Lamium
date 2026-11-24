#pragma once
// L-360: taking an item out of Lamium's lists in creative. Pure: what a
// Shift + press does on a listed item, where a taken item goes and what the
// player is told when it cannot be taken.
namespace lamium::recipes::take {
// The list a press lands on.
enum class List { All, CanMake, Favorites, Plan };
enum class ShiftPress { Recipes, PlaceMany, TakeStack, Own };
// Shift + a left press without Ctrl. "Can be made" is the list for crafting and always places; "All" takes where
// taking is allowed, else shows the recipes. The favorites and the plan's list have no filter and go by the game
// mode (cheats not counted): in creative as "All", elsewhere as "can be made"; a row of the plan's list keeps its
// own press there (it opens the plan's window). `taking`: creative and the capability allowed.
constexpr ShiftPress shiftPress(List list, bool taking) {
    switch (list) {
    case List::CanMake: return ShiftPress::PlaceMany;
    case List::All: return taking ? ShiftPress::TakeStack : ShiftPress::Recipes;
    case List::Favorites: return taking ? ShiftPress::TakeStack : ShiftPress::PlaceMany;
    case List::Plan: return taking ? ShiftPress::TakeStack : ShiftPress::Own;
    }
    return ShiftPress::Own;
}
// The key hints' line names what Shift + press does in the current mode.
enum class ShiftHint { IntoInventory, PlaceMany };
constexpr ShiftHint shiftHint(bool taking) { return taking ? ShiftHint::IntoInventory : ShiftHint::PlaceMany; }
// Onto the pointer only when it is empty: Lamium's presses never move what it holds, so with anything on it the
// item goes into the inventory instead.
enum class Into { Pointer, Inventory };
constexpr Into into(bool wantsPointer, bool pointerEmpty) { return wantsPointer && pointerEmpty ? Into::Pointer : Into::Inventory; }
// Why nothing was taken. Only the crafting screens (the inventory and the crafting table) carry the creative list
// Lamium takes from; an item the creative inventory does not hold (a book of two enchantments) has no cell
// anywhere; one it holds but whose cell the game's own search hides waits for that search to be cleared.
enum class Refusal { None, NotHere, NotListed, GameSearch };
constexpr Refusal refusal(bool screenHasList, bool listed, bool cellFound) {
    if (cellFound) return Refusal::None;
    if (!screenHasList) return Refusal::NotHere;
    return listed ? Refusal::GameSearch : Refusal::NotListed;
}
}
