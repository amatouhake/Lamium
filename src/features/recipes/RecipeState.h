#pragma once
#include "features/recipes/PlanTargets.h"
#include "features/recipes/Places.h"
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
// What features hand over to the plan. A feature gives a function that says what it needs now (item
// key and count), or nothing while it is off; it is asked with the plan, twice a second. nullptr takes
// the feature out again.
using Provider = std::vector<std::pair<std::string, Count>> (*)();
void provide(std::string const& source, Provider provider);
std::vector<Handed> const& handed();
void toggleFavourite(std::string const& key);
void toggleWanted(std::string const& key);
void setWanted(std::string const& key, Count amount);
void removeWanted(std::string const& key);
// A wanted item's stepper moved (steppedWanted): `button` is its minus or plus, not the wheel.
void stepWanted(std::string const& key, bool up, bool shift, bool button);
// Wanted items the wheel left at 0 go out, but `keep`, the one under the pointer or being stepped.
void settleWanted(std::string_view keep);
// The wanted items, the player's then what features hand over, in the order added (L-370).
std::vector<WantedEntry> wantedEntries();
// The recipe an item is made by, as choiceKey names it; empty gives it back to the default.
void setChoice(std::string const& item, std::string const& recipe);
// The member used for a group; an empty member gives it back to "any".
void setPick(std::string const& group, std::string const& member);
void setStop(std::string const& item, bool gathered);
// The counted places of this world (Places.h), kept beside the wanted items. `seePlace`: what an open
// container holds now; it is kept if the container is the ender chest or a store.
Places const& places();
void seePlace(bool ender, Spot const& spot, std::optional<Spot> const& pair, std::string const& block, std::vector<Stored> items);
bool markStore(Spot const& spot, std::optional<Spot> const& pair, std::string const& block, std::vector<Stored> items);
void unmarkStore(int index);
void setPlaceCounted(int index, bool counted);
// Whether what is carried counts.
void setCarriedCounted(bool carried);
// The stores as boxes in the world, for the world overlay to outline; read from any thread, replaced
// when the places change or another world is joined.
std::shared_ptr<std::vector<StoreBox> const> storeMarks();
// What is held, by item name: what is carried and what the counted places held when last seen. The one
// count for the plan, the schematic's Materials tab and its HUD.
std::map<std::string, std::uint64_t> heldByName(LocalPlayer& player);
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
    std::map<Item, Count> held; // carried as far as it counts (the player's switches), by item
    std::map<Item, Count> stored; // in the counted places as last seen, by item; the plan counts both
    std::vector<LooseLine> loose; // wanted items the table does not know: gathered as they are
};
// Null until the recipe table is read.
Plan const* plan(LocalPlayer& player);
// L-370: the wanted entries in priority order and, for each, its share of the plan (item -1 where the table
// lacks it). Worked out only when asked (the plan window's wanted and make tabs), once per plan.
struct Shares {
    std::vector<WantedEntry> entries;
    std::vector<TargetPlan> targets;
};
Shares const* shares(LocalPlayer& player);
}
