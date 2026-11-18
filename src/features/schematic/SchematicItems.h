#pragma once
#include <cstdint>
#include <map>
#include <vector>
#include <string>
class ContainerScreenController;
class ItemStack;
class LocalPlayer;
// Item helpers shared by the Schematics screen and HUD (BACKLOG L-93).
// Client thread only.
namespace lamium::schematic::items {
// An item stack for an icon stored as binary NBT, cached; null when unknown.
// Pickup squash is off so the renderer draws it still.
ItemStack const* iconStack(std::string const& icon);
// Drops the cached stacks. Called on world exit: a stack points at the
// world's items, which the game rebuilds (for example after a language
// change), and drawing an old one crashed.
void forget();
// An item by its id, or its key where an aux tells variants apart
// (recipes/ItemKey.h), as the game names and draws it (for items no schematic
// line names: what a recipe uses or makes). Cached with the icon stacks. The
// id as the name and no icon when the game does not know it.
struct Shown {
    std::string name, icon;
    int stack = 64;
};
Shown const& shown(std::string const& item);
// Items carried: the inventory and the contents of shulker boxes in it.
std::map<std::string, std::uint64_t> carried(LocalPlayer& player);
// The same stacks one by one, with the aux a recipe may tell them apart by (a bed's color).
struct Carried {
    std::string name;
    int aux = 0;
    std::uint64_t count = 0;
};
// `screen`: the item screen that is open, if one is. A bundle keeps only an id; what it holds is in a
// container of the client's that the game reads through an open item screen. With `screen` it is read
// and remembered by the bundle's id; without, what was last read for that id is used (none before the
// first item screen of the world).
std::vector<Carried> carriedStacks(LocalPlayer& player, ContainerScreenController const* screen = nullptr);
// One stack, and with a shulker box or a bundle what it holds.
void addStack(ItemStack const& stack, std::vector<Carried>& out, ContainerScreenController const* screen = nullptr);
}
