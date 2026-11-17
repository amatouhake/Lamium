#pragma once
#include "features/recipes/RecipeIndex.h"
#include <memory>

// The recipe index of the world the player is in, shared by whatever plans
// with it (L-359). Nothing is read until a screen asks: each ask then reads
// a few hundred of the game's recipes, and the index is published once,
// until the world is left. Behind the RecipeTable capability.
namespace lamium::recipes::service {
struct Table {
    Index index;
    Choice choice;
};
enum class State { Idle, Reading, Ready, Paused, Unreadable };
void start();
void stop();
// Asks for the table, reads a part of it and says where it stands. Client
// thread only; a screen calls it every frame while it wants the table.
State want();
// The table once Ready, else empty. Never changes after it is handed out.
std::shared_ptr<Table const> table();
}
