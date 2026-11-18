#pragma once
#include "features/recipes/RawView.h"
#include "features/recipes/RecipeNotes.h"
#include "features/recipes/RecipeService.h"
#include <map>
#include <memory>
#include <string>
class LocalPlayer;

// L-359: what the player entered (RecipeNotes.h) as the screens share it,
// loaded and saved here: the notes in Lamium's config folder for all worlds,
// the wanted list beside the world's waypoints (map/WaypointSession.h). A
// change is written at once and shows in every screen the next frame.
// Client thread only.
namespace lamium::recipes::state {
void start();
void stop();
Notes const& notes();
Wanted const& wanted();
void toggleFavourite(std::string const& key);
void toggleWanted(std::string const& key);
void setWanted(std::string const& key, Count amount);
void removeWanted(std::string const& key);
// A wanted item's stepper moved (steppedWanted): `button` is its minus or plus, not the wheel.
void stepWanted(std::string const& key, bool up, bool shift, bool button);
// The recipe an item is made by, as choiceKey names it; empty gives it back to the default.
void setChoice(std::string const& item, std::string const& recipe);
// The member used for a group; an empty member gives it back to "any".
void setPick(std::string const& group, std::string const& member);
void setStop(std::string const& item, bool gathered);
// The recipe table with the player's choices put in. The pointer holds until
// the notes change or the world is left.
std::shared_ptr<Choice const> choice(std::shared_ptr<service::Table const> const& table);
// What is wanted, worked out against what is carried: asked again when
// anything entered changes and twice a second for the inventory.
struct Plan {
    std::shared_ptr<service::Table const> table;
    std::shared_ptr<Choice const> choice;
    Request request;
    RawView view;
    std::map<Item, Count> held; // carried, by item
};
// Null until the recipe table is read.
Plan const* plan(LocalPlayer& player);
}
