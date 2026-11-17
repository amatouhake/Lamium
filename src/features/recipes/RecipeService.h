#pragma once
#include "features/recipes/RecipeIndex.h"
#include <memory>

// The recipe index of the world the player is in, shared by whatever plans
// with it (L-359). Nothing is read until a screen asks: then the client tick
// reads the game's table a few hundred recipes at a time and publishes the
// index once, until the world is left. Behind the RecipeTable capability.
namespace lamium::recipes::service {
struct Table {
    Index index;
    Choice choice;
};
enum class State { Idle, Reading, Ready, Paused, Unreadable };
void start();
void stop();
// Asks for the table and says where it stands. Client thread.
State want();
// The table once Ready, else empty. Never changes after it is handed out.
std::shared_ptr<Table const> table();
}
