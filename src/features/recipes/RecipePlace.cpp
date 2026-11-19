#include "features/recipes/RecipePlace.h"
#include "app/Runtime.h"
#include "features/inspection/hover/HoverTracker.h"
#include "features/inventory/game/TransferRoutes.h"
#include "features/recipes/ItemList.h"
#include "features/recipes/MassCraftPlan.h"
#include "features/recipes/RecipeState.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/client/ClientExitLevelEvent.h"
#include "ll/api/event/render/UIRenderEvent.h"
#include "mc/client/gui/screens/ScreenController.h"
#include "mc/client/gui/screens/ScreenView.h"
#include "mc/client/gui/screens/controllers/ContainerScreenController.h"
#include "mc/client/gui/screens/controllers/CraftingScreenController.h"
#include "mc/deps/shared_types/legacy/ContainerType.h"
#include "mc/world/containers/SlotData.h"
#include "mc/world/containers/managers/controllers/ContainerManagerController.h"
#include "mc/world/containers/managers/controllers/CraftingContainerManagerController.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/item/crafting/Recipe.h"
#include <atomic>
#include <chrono>
#include <climits>
#include <string>

namespace lamium::recipes {
namespace {
std::string const grid = "crafting_input_items", output = "crafting_output_items", pointer = "cursor_items", hotbar = "hotbar_items",
    inventory = "inventory_items";
// A recipe waiting to be laid in: what lay in the grid is on its way back to the inventory.
struct Waiting {
    std::shared_ptr<service::Table const> table;
    int recipe = -1;
    std::map<int, Item> picks;
    std::string screen;
    bool many = false;
    int tries = 0; // frames waited for the grid to read empty
} going;
constexpr int mostTries = 20;
// The crafting screens: the inventory's 2x2 and the crafting table's 3x3 (seen with craft_trace, 2026-11-18).
CraftingScreenController* crafting(ScreenView& screen) {
    if (!screen.mController || !screen.mController->_isContainerScreen()) return nullptr;
    std::string const name = screen.getScreenName();
    if (name != "inventory_screen" && name != "crafting_screen") return nullptr;
    auto& controller = static_cast<CraftingScreenController&>(*screen.mController);
    if (!inventory::game::transferRoutes::supported(controller)) return nullptr;
    auto manager = controller.mContainerManagerController;
    if (!manager->hasContainerController(grid) || !manager->hasContainerController(pointer)) return nullptr;
    return &controller;
}
int sideOf(ContainerManagerController& manager) {
    int const size = manager.getContainerSize(grid);
    return size == 4 ? 2 : size == 9 ? 3 : 0;
}
int held(ContainerManagerController& manager) {
    auto const& stack = manager.getItemStack(pointer, 0);
    return stack.isNull() ? 0 : static_cast<int>(stack.mCount);
}
bool gridEmpty(ContainerManagerController& manager) {
    for (int i = 0, size = manager.getContainerSize(grid); i < size; ++i)
        if (!manager.getItemStack(grid, i).isNull()) return false;
    return true;
}
std::vector<PlaceSource> carriedOf(ContainerManagerController& manager, Index const& index) {
    std::vector<PlaceSource> out;
    auto add = [&](std::string const& collection, int size, int first) {
        if (!manager.hasContainerController(collection) || manager.getContainerSize(collection) < size) return;
        for (int i = 0; i < size; ++i) {
            auto const& stack = manager.getItemStack(collection, i);
            if (stack.isNull() || stack.mCount <= 0) continue;
            Item const item = index.find(stack.getTypeName(), stack.getAuxValue());
            if (item >= 0) out.push_back({first + i, item, static_cast<int>(stack.mCount), std::max(1, static_cast<int>(stack.getMaxStackSize()))});
        }
    };
    add(hotbar, 9, 0);
    add(inventory, 27, 9);
    return out;
}
// The recipe laid into an empty grid.
bool lay(CraftingScreenController& controller, Index const& index, Recipe const& recipe, std::map<int, Item> const& picks, bool many) {
    auto& manager = *controller.mContainerManagerController;
    if (held(manager) || !gridEmpty(manager)) return false;
    auto const plan = placePlan(index, recipe, sideOf(manager), carriedOf(manager, index), picks, many);
    if (!plan.ready()) return false;
    for (auto const& move : plan.moves) {
        bool const top = move.slot < 9;
        controller._handleTakeAmount(move.amount, top ? hotbar : inventory, top ? move.slot : move.slot - 9);
        // Not as planned: what the pointer holds, if anything, is left to the player.
        if (held(manager) != move.amount) return false;
        controller._handlePlaceAll(grid, move.cell);
        if (held(manager)) return false;
    }
    return true;
}
}
namespace place {
int gridSide(ScreenView& screen) {
    try {
        auto* controller = crafting(screen);
        return controller ? sideOf(*controller->mContainerManagerController) : 0;
    } catch (...) { return 0; }
}
std::vector<PlaceSource> carried(ScreenView& screen, Index const& index) {
    try {
        auto* controller = crafting(screen);
        if (!controller) return {};
        auto& manager = *controller->mContainerManagerController;
        auto out = carriedOf(manager, index);
        // What lies in the grid goes back to the inventory before a recipe is laid in, so it counts.
        for (int i = 0, size = manager.getContainerSize(grid); i < size; ++i) {
            auto const& stack = manager.getItemStack(grid, i);
            if (stack.isNull() || stack.mCount <= 0) continue;
            if (Item const item = index.find(stack.getTypeName(), stack.getAuxValue()); item >= 0)
                out.push_back({100 + i, item, static_cast<int>(stack.mCount), std::max(1, static_cast<int>(stack.getMaxStackSize()))});
        }
        return out;
    } catch (...) {}
    return {};
}
bool run(ScreenView& screen, std::shared_ptr<service::Table const> const& table, int recipe, std::map<int, Item> const& picks, bool many) {
    going = {};
    try {
        auto* controller = crafting(screen);
        if (!controller || !table || recipe < 0 || recipe >= static_cast<int>(table->index.recipes.size())) return false;
        auto& manager = *controller->mContainerManagerController;
        if (held(manager)) return false;
        if (!many && !gridEmpty(manager)) {
            // Already laid as asked: taking it out and laying it in again only shows items flying (L-367).
            int const side = sideOf(manager);
            std::vector<Item> cells;
            for (int i = 0, size = manager.getContainerSize(grid); i < size && i < side * side; ++i) {
                auto const& stack = manager.getItemStack(grid, i);
                cells.push_back(stack.isNull() || stack.mCount <= 0 ? -1 : table->index.find(stack.getTypeName(), stack.getAuxValue()));
            }
            if (alreadyLaid(table->index, table->index.recipes[static_cast<size_t>(recipe)], side, cells)) return true;
        }
        if (!gridEmpty(manager)) {
            controller->_evacuateCraftingGrid();
            // Not read as empty at once: the recipe is laid in when it is, a few frames on.
            if (!gridEmpty(manager)) {
                going = {table, recipe, picks, screen.getScreenName(), many, 0};
                return true;
            }
        }
        return lay(*controller, table->index, table->index.recipes[static_cast<size_t>(recipe)], picks, many);
    } catch (...) {
        try { Runtime::instance().self().getLogger().warn("Recipes: placing a recipe failed"); } catch (...) {}
        return false;
    }
}
void tick(ScreenView& screen) {
    if (going.recipe < 0) return;
    try {
        auto* controller = crafting(screen);
        if (!controller || screen.getScreenName() != going.screen) { going = {}; return; }
        // Waiting for what lay in the grid to be back in the inventory (no room for it: given up).
        if (!gridEmpty(*controller->mContainerManagerController)) { if (++going.tries > mostTries) going = {}; return; }
        lay(*controller, going.table->index, going.table->index.recipes[static_cast<size_t>(going.recipe)], going.picks, going.many);
    } catch (...) {}
    going = {};
}
void stop() { going = {}; }
}
namespace massCraft {
namespace {
using Clock = std::chrono::steady_clock;
// Between two steps of a run, so a fast frame rate does not send requests faster (a step is a whole
// round where everything shows at once: laid in, taken, thrown out); and how long a
// round that cannot be laid in waits for more to arrive, or for what it threw out to be gone.
// `patience`: how long a take's result or a product may take to show before the step is tried again. A
// request is refused when the server's inventory is not the client's (items picked up meanwhile), so a
// step that shows nothing is tried again, `mostStalls` times in a row before the run gives up.
constexpr auto step = std::chrono::milliseconds(50), idle = std::chrono::milliseconds(250), afterSurplus = std::chrono::milliseconds(1000),
               patience = std::chrono::milliseconds(150);
constexpr int mostStalls = 20;
constexpr int slots = 36;
// Hand: the inventory has no room for the product, so it is taken onto the pointer and thrown from there.
enum class Phase { Lay, Take, Throw, Hand };
// `mostRequests`: the slot calls a step may make (a move into a cell is two). A local world closed with a
// network error under some hundreds a step's worth a second; a step stays well under that.
constexpr int mostPasses = 10, mostRequests = 16;
struct Run {
    bool on = false, over = false; // over: this press of the key does nothing more
    bool fromGrid = false;         // started over the output slot: it goes on only while the pointer stays there
    std::shared_ptr<service::Table const> table;
    int recipe = -1;
    std::map<int, Item> picks;
    Phase phase = Phase::Take;
    int keep = 0;      // of the product, what the player carried when the run began: it stays
    char const* wait = ""; // why the last step did nothing, for the log
    char const* told = nullptr;
    int done = 0;          // rounds, takes and throws when something last happened
    Clock::time_point doneAt;
    bool laid = false; // this step laid a round in: one a step
    Clock::time_point next, began, since; // since: when the current step began
    bool clicked = false; // Hand: a press on the output is out
    int handSeen = 0;     // Hand: what the pointer held before that press
    int rounds = 0, takes = 0, thrown = 0, surplus = 0, hands = 0, stalls = 0, retries = 0, cleared = 0, roomed = 0, requests = 0;
} run;
std::atomic<bool> keyHeld{false};
Clock::time_point pressedAt; // when the key went down
bool evacuated = false;      // this press already sent the grid's contents back
bool refusalTold = false;    // this press already logged why it does not start
ll::event::ListenerPtr renderListener, exitListener;
void finish(char const* why) {
    if (run.on) {
        auto const ms = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - run.began).count();
        try {
            Runtime::instance().self().getLogger().info("Mass Craft: {} rounds laid, {} takes, {} stacks thrown, {} thrown from the pointer, {} surplus stacks, {} stacks thrown for room, {} steps tried again, {} times the pointer cleared, {} requests in {} ms ({})",
                                                       run.rounds, run.takes, run.thrown, run.hands, run.surplus, run.roomed, run.retries, run.cleared, run.requests, ms, why);
        } catch (...) {}
    }
    run = {};
    run.over = keyHeld;
}
// As Item transfer reads the slot under the pointer: the inventory screen's own record of it is not kept
// up, so the hover tracker tells there.
bool overOutput(CraftingScreenController& controller) {
    auto const& native = controller.mHoveredSlotData.get();
    if (controller.mContainerManagerController->getContainerType() != SharedTypes::Legacy::ContainerType::Inventory && !native.mCollectionName->empty())
        return native.mCollectionName.get() == output;
    auto const& current = inspection::hover::HoverTracker::getInstance().current();
    return current && current->controller == &controller && current->collectionName == output;
}
std::string const& collectionOf(int slot) { return slot < 9 ? hotbar : inventory; }
int indexOf(int slot) { return slot < 9 ? slot : slot - 9; }
// Hotbar and inventory, slot by slot: the item (-1: empty or not in the table) and how many.
std::vector<std::pair<Item, int>> slotsOf(ContainerManagerController& manager, Index const& index, bool& anyFree) {
    std::vector<std::pair<Item, int>> out(slots, {-1, 0});
    anyFree = false;
    if (manager.getContainerSize(hotbar) < 9 || manager.getContainerSize(inventory) < 27) return out;
    for (int slot = 0; slot < slots; ++slot) {
        auto const& stack = manager.getItemStack(collectionOf(slot), indexOf(slot));
        if (stack.isNull() || stack.mCount <= 0) { anyFree = true; continue; }
        out[static_cast<size_t>(slot)] = {index.find(stack.getTypeName(), stack.getAuxValue()), static_cast<int>(stack.mCount)};
    }
    return out;
}
// A free slot, or a stack of the product that takes more: a Shift take has somewhere to go.
bool roomFor(ContainerManagerController& manager, Index const& index, Item product) {
    if (manager.getContainerSize(hotbar) < 9 || manager.getContainerSize(inventory) < 27) return false;
    for (int slot = 0; slot < slots; ++slot) {
        auto const& stack = manager.getItemStack(collectionOf(slot), indexOf(slot));
        if (stack.isNull() || stack.mCount <= 0) return true;
        if (stack.mCount < stack.getMaxStackSize() && index.find(stack.getTypeName(), stack.getAuxValue()) == product) return true;
    }
    return false;
}
bool makes(ContainerManagerController& manager) { return static_cast<CraftingContainerManagerController&>(manager).mCraftingRecipe != nullptr; }
// The grid's items, cell by cell, and the id of the recipe the game says they make.
std::vector<Item> cellsOf(ContainerManagerController& manager, Index const& index, std::string& named) {
    std::vector<Item> cells;
    for (int i = 0, size = manager.getContainerSize(grid); i < size; ++i) {
        auto const& stack = manager.getItemStack(grid, i);
        cells.push_back(stack.isNull() || stack.mCount <= 0 ? -1 : index.find(stack.getTypeName(), stack.getAuxValue()));
    }
    named.clear();
    if (::Recipe const* made = static_cast<CraftingContainerManagerController&>(manager).mCraftingRecipe) named = *made->mRecipeId;
    return cells;
}
// Whether the grid makes the run's product: a round the server let through in part only lays out
// another recipe (three planks of a chest's eight make slabs), which must not be taken.
bool makesProduct(ContainerManagerController& manager, Index const& index, Item product) {
    if (!makes(manager)) return false;
    std::string named;
    auto const cells = cellsOf(manager, index, named);
    int const recipe = recipeInGrid(index, sideOf(manager), cells, named);
    return recipe >= 0 && index.recipes[static_cast<size_t>(recipe)].makes == product;
}
// The crafting recipe an item is made by that fits the grid: the one the player's choices name, else the first.
int recipeFor(std::shared_ptr<service::Table const> const& shared, Item item, int side) {
    auto const& table = *shared;
    auto fits = [&](int recipe) {
        return recipe >= 0 && placePlan(table.index, table.index.recipes[static_cast<size_t>(recipe)], side, {}, {}, false).fits;
    };
    if (auto const choice = state::choice(shared); choice && static_cast<size_t>(item) < choice->recipe.size() && fits(choice->recipe[static_cast<size_t>(item)]))
        return choice->recipe[static_cast<size_t>(item)];
    for (int recipe : table.index.makers[static_cast<size_t>(item)]) if (fits(recipe)) return recipe;
    return -1;
}
void begin(CraftingScreenController& controller) {
    auto& manager = *controller.mContainerManagerController;
    if (held(manager)) {
        // Waits for the pointer to be empty; said once, since nothing else shows it.
        if (!refusalTold) {
            refusalTold = true;
            try { Runtime::instance().self().getLogger().info("Mass Craft: not started, the pointer holds an item"); } catch (...) {}
        }
        return;
    }
    if (service::want() != service::State::Ready) return;
    auto table = service::table();
    if (!table) return;
    int const side = sideOf(manager);
    bool const fromGrid = overOutput(controller);
    int recipe = -1;
    std::string named;
    if (fromGrid) {
        // Over the output slot the recipe is the one lying in the grid: nothing in it, nothing to do yet.
        if (!makes(manager)) return;
        auto const cells = cellsOf(manager, table->index, named);
        recipe = recipeInGrid(table->index, side, cells, named);
    } else {
        // Elsewhere it is the recipe of the item under the pointer, in Lamium's lists or a slot of the game.
        Item item = -1;
        if (auto const key = itemList::hoveredItem(); !key.empty()) item = table->index.findKey(key);
        else if (auto const* stack = inspection::hover::HoverTracker::getInstance().resolveItem(controller); stack && !stack->isNull())
            item = table->index.find(stack->getTypeName(), stack->getAuxValue());
        if (item < 0) return;
        recipe = recipeFor(table, item, side);
    }
    if (recipe < 0) {
        try { Runtime::instance().self().getLogger().info("Mass Craft: no crafting recipe for it fits this grid"); } catch (...) {}
        run.over = true;
        return;
    }
    if (!fromGrid && !gridEmpty(manager)) {
        // What lies in the grid goes back first, once a press; the run starts when the grid reads empty.
        if (!evacuated) controller._evacuateCraftingGrid(), evacuated = true;
        if (!gridEmpty(manager)) return;
    }
    Request request;
    apply(table->index, state::notes(), request);
    run = {};
    run.on = true;
    run.table = std::move(table);
    run.recipe = recipe;
    run.picks = std::move(request.picks);
    run.fromGrid = fromGrid;
    run.phase = fromGrid ? Phase::Take : Phase::Lay;
    run.next = run.began = run.since = run.doneAt = Clock::now();
    bool anyFree = false;
    for (auto const& [item, count] : slotsOf(manager, run.table->index, anyFree))
        if (item == run.table->index.recipes[static_cast<size_t>(recipe)].makes) run.keep += count;
    try {
        Runtime::instance().self().getLogger().info("Mass Craft: started with {} (the game names {})", run.table->index.recipes[static_cast<size_t>(recipe)].id, named);
    } catch (...) {}
}
// A step showed nothing: it is tried again from the take, or the run ends when that keeps happening.
void again(char const* why, Clock::time_point now) {
    if (++run.stalls > mostStalls) { finish(why); return; }
    ++run.retries;
    run.phase = Phase::Take;
    run.since = now;
    run.clicked = false;
    run.next = now + step;
}
// What a step of the run left on the pointer (a cell that was not placed, a take the server answered
// late): the product is thrown out, anything else goes back to a slot that takes it, and out if none does.
// The pointer was empty when the run began, so what it holds is the run's.
bool clearPointer(CraftingScreenController& controller, Index const& index, Item product) {
    auto& manager = *controller.mContainerManagerController;
    if (!held(manager)) return true;
    auto const& stack = manager.getItemStack(pointer, 0);
    std::string const name = stack.getTypeName();
    int const aux = stack.getAuxValue();
    if (index.find(name, aux) != product) {
        for (int pass = 0; pass < 2 && held(manager); ++pass)
            for (int slot = 0; slot < slots && held(manager); ++slot) {
                auto const& there = manager.getItemStack(collectionOf(slot), indexOf(slot));
                bool const empty = there.isNull() || there.mCount <= 0;
                // Stacks of the same item first, then an empty slot.
                bool const takes = pass ? empty : !empty && there.mCount < there.getMaxStackSize() && there.getTypeName() == name && there.getAuxValue() == aux;
                if (takes) controller._handlePlaceAll(collectionOf(slot), indexOf(slot));
            }
    }
    if (held(manager)) inventory::game::transferRoutes::dropCursor(controller);
    return !held(manager);
}
// Throws out what the inventory holds of the product beyond what was carried at the start: how many
// throws, -1 when one could not be sent. `budget`: the slot calls the step has left.
int throwExcess(CraftingScreenController& controller, Index const& index, Item product, int& budget) {
    bool anyFree = false;
    int thrown = 0;
    for (auto const& out : excess(slotsOf(*controller.mContainerManagerController, index, anyFree), product, run.keep)) {
        if (budget <= 0) break;
        budget -= out.whole ? 1 : 2;
        bool const sent = out.whole ? inventory::game::transferRoutes::drop(controller, collectionOf(out.slot), indexOf(out.slot), false)
                                    : inventory::game::transferRoutes::dropAmount(controller, collectionOf(out.slot), indexOf(out.slot), out.amount);
        if (!sent) return -1;
        ++thrown;
    }
    return thrown;
}
void advance(CraftingScreenController& controller) {
    auto& manager = *controller.mContainerManagerController;
    auto const& index = run.table->index;
    auto const& recipe = index.recipes[static_cast<size_t>(run.recipe)];
    auto const now = Clock::now();
    if (now < run.next) return;
    if (run.phase != Phase::Hand && held(manager)) {
        if (!clearPointer(controller, index, recipe.makes)) { finish("the pointer holds an item"); return; }
        ++run.cleared;
        again("the pointer holds an item", now);
        return;
    }
    bool anyFree = false;
    run.next = now + step;
    run.laid = false;
    // A run that does nothing for a second says once why, with what it sees: the log is all there is to
    // tell a run that waits from one that is stuck.
    if (int const done = run.rounds + run.takes + run.thrown + run.hands + run.surplus + run.roomed; done != run.done) run.done = done, run.doneAt = now, run.told = nullptr;
    else if (now - run.doneAt > std::chrono::seconds(1) && run.told != run.wait) {
        run.told = run.wait;
        try {
            int carried = 0, free = 0;
            auto const plan = placePlan(index, recipe, sideOf(manager), carriedOf(manager, index), run.picks, true);
            for (auto const& [item, count] : slotsOf(manager, index, anyFree)) carried += count > 0, free += count <= 0;
            Runtime::instance().self().getLogger().info(
                "Mass Craft: waiting ({}): phase {}, grid {} makes {} ours {}, pointer {}, slots filled {} free {}, plan fits {} missing {} crafts {} moves {}, keep {}",
                run.wait, static_cast<int>(run.phase), gridEmpty(manager) ? "empty" : "filled", makes(manager), makesProduct(manager, index, recipe.makes),
                held(manager), carried, free, plan.fits, plan.missing.size(), plan.crafts, plan.moves.size(), run.keep);
        } catch (...) {}
    }
    run.wait = "nothing to do";
    // What the game shows at once goes on in the same frame: a round is laid in, taken and thrown out in one,
    // and a take that found room for a part only is followed by the next.
    int budget = mostRequests;
    struct Spent {
        int& budget;
        ~Spent() { if (run.on) run.requests += mostRequests - budget; }
    } spent{budget};
    for (int pass = 0; pass < mostPasses && budget > 0; ++pass)
    switch (run.phase) {
    case Phase::Take: {
        // Also what came back: thrown out and picked up again, or a throw the server refused.
        if (int const thrown = throwExcess(controller, index, recipe.makes, budget); thrown < 0) { finish("throwing the product out failed"); return; }
        else if (thrown) run.thrown += thrown, run.stalls = 0;
        if (!makesProduct(manager, index, recipe.makes)) {
            if (gridEmpty(manager)) { run.phase = Phase::Lay; run.since = now; continue; }
            // What a recipe leaves behind (a bucket), a product not shown yet, or a part of the recipe only.
            run.wait = "the grid does not make the product";
            if (now - run.since <= patience) return;
            // Started from an item, the grid is this run's: what stays in it goes back and it is laid in anew.
            if (run.fromGrid) { finish("the grid holds something that is not the recipe"); return; }
            controller._evacuateCraftingGrid();
            again("the grid holds something that is not the recipe", now);
            return;
        }
        if (!roomFor(manager, index, recipe.makes)) {
            // No slot for the product: one stack of an ingredient goes out, so the take has one. Only where
            // nothing can go (it is the ingredient's only stack) the product goes out over the pointer.
            int const slot = roomSlot(index, recipe, carriedOf(manager, index));
            if (slot < 0 || !inventory::game::transferRoutes::drop(controller, collectionOf(slot), indexOf(slot), false)) {
                run.phase = Phase::Hand; run.since = now; run.clicked = false;
                continue;
            }
            --budget;
            ++run.roomed;
            if (!roomFor(manager, index, recipe.makes)) return;
        }
        // What a Shift click on the output does: all the grid makes, as far as the inventory has room.
        controller._handleAutoPlace(INT_MAX, output, 0);
        --budget;
        ++run.takes;
        run.phase = Phase::Throw;
        run.since = now;
        continue;
    }
    case Phase::Throw: {
        int const thrown = throwExcess(controller, index, recipe.makes, budget);
        if (thrown < 0) { finish("throwing the product out failed"); return; }
        if (!thrown) {
            run.wait = "the take showed nothing";
            if (now - run.since > patience) again("the product found no room", now);
            return;
        }
        run.thrown += thrown;
        run.stalls = 0;
        run.phase = Phase::Take;
        run.since = now;
        continue;
    }
    case Phase::Hand: {
        int have = held(manager);
        auto more = [&](int count) {
            int most = 64;
            if (count) most = static_cast<int>(manager.getItemStack(pointer, 0).getMaxStackSize());
            return makesProduct(manager, index, recipe.makes) && count + static_cast<int>(recipe.count) <= most;
        };
        auto product = [&] {
            auto const& stack = manager.getItemStack(pointer, 0);
            return index.find(stack.getTypeName(), stack.getAuxValue()) == recipe.makes;
        };
        if (have && !product()) {
            if (!clearPointer(controller, index, recipe.makes)) { finish("the pointer holds an item"); return; }
            ++run.cleared;
            again("the pointer holds an item", now);
            return;
        }
        // A press on the output crafts once onto the pointer; again while the last one showed. Away from the
        // output slot the product is taken onto the pointer by the slot call instead (not seen to craft yet).
        if (more(have) && (!run.clicked || have > run.handSeen)) {
            while (budget > 1) {
                --budget;
                run.handSeen = held(manager);
                if (!more(run.handSeen)) break;
                if (!overOutput(controller)) controller._handleTakeAmount(static_cast<int>(recipe.count), output, 0);
                else if (!inventory::game::transferRoutes::pressAsPlainClick(controller)) { finish("pressing the output failed"); return; }
                run.clicked = true;
                if (held(manager) <= run.handSeen) break;
            }
            have = held(manager);
            if (have && !product()) {
            if (!clearPointer(controller, index, recipe.makes)) { finish("the pointer holds an item"); return; }
            ++run.cleared;
            again("the pointer holds an item", now);
            return;
        }
        }
        if (!have) {
            if (now - run.since > patience) again("the product could not be taken", now);
            return;
        }
        if (!inventory::game::transferRoutes::dropCursor(controller)) { finish("throwing the product out failed"); return; }
        --budget;
        ++run.hands;
        run.stalls = 0;
        run.phase = Phase::Take;
        run.since = now;
        run.clicked = false;
        continue;
    }
    case Phase::Lay: {
        if (!gridEmpty(manager)) { run.wait = "the grid is not empty"; run.phase = Phase::Take; run.since = now; return; }
        if (run.laid) return;
        run.laid = true;
        // While a round waits for its ingredients, the product that was thrown out and is picked up again
        // would fill the inventory and leave no slot for them (seen: 36 slots of planks, logs on the ground).
        int const back = throwExcess(controller, index, recipe.makes, budget);
        if (back < 0) { finish("throwing the product out failed"); return; }
        if (back) run.thrown += back, run.stalls = 0;
        auto const carried = carriedOf(manager, index);
        auto const plan = placePlan(index, recipe, sideOf(manager), carried, run.picks, true);
        if (!plan.fits) { finish("the recipe does not fit the grid"); return; }
        if (plan.ready()) {
            // A round is laid in by a step that has all its calls left, however many cells it has.
            int const cost = 2 * static_cast<int>(plan.moves.size());
            if (cost > budget && budget < mostRequests) { run.wait = "the step has no calls left to lay in"; return; }
            budget -= cost;
            // Laid in part: the take that follows finds no product, and what was laid goes back.
            if (!lay(controller, index, recipe, run.picks, true) && held(manager)) { again("laying the recipe in failed", now); return; }
            ++run.rounds;
            run.phase = Phase::Take;
            run.since = now;
            continue;
        }
        // Not enough of an ingredient: more may arrive while the key is held. With no slot free for it,
        // what the others fill beyond their share goes out.
        run.wait = "an ingredient is missing";
        // While the product keeps coming back the wait is a step, so the slots are open for the ingredients
        // most of the time; else it is longer.
        run.next = now + (back ? step : idle);
        slotsOf(manager, index, anyFree);
        if (anyFree) return;
        for (int slot : surplusSlots(index, recipe, carried)) {
            if (!inventory::game::transferRoutes::drop(controller, collectionOf(slot), indexOf(slot), false)) { finish("throwing a surplus out failed"); return; }
            ++run.surplus;
            run.next = now + afterSurplus;
        }
        return;
    }
    }
}
void tick(ScreenView& screen) {
    if (!keyHeld) { if (run.on || run.over) finish("the key was released"); return; }
    if (run.over) return;
    auto& runtime = Runtime::instance();
    if (!runtime.enabled() || !runtime.snapshot()->inventory.massCraft) { finish("switched off"); return; }
    auto* controller = crafting(screen);
    if (!controller) { finish("not a crafting screen"); return; }
    if (run.on && run.fromGrid && !overOutput(*controller)) { finish("the pointer left the output slot"); return; }
    // Held a moment first, so a brush of the key starts nothing.
    if (!run.on && Clock::now() - pressedAt >= std::chrono::milliseconds(static_cast<int>(runtime.snapshot()->inventory.massCraftDelay * 1000)))
        begin(*controller);
    if (run.on) advance(*controller);
}
}
void setHeld(bool held) {
    if (held && !keyHeld) pressedAt = Clock::now(), evacuated = false, refusalTold = false;
    keyHeld = held;
    if (!held) finish("the key was released");
}
void start() {
    auto& bus = ll::event::EventBus::getInstance();
    renderListener = bus.emplaceListener<ll::event::AfterUIRenderEvent>([](auto& event) {
        try {
            auto& screen = event.screenView();
            // Several screens are drawn a frame (the HUD under a container): only the focused item screen is this run's.
            if (!screen.mHasFocus || !screen.mController || !screen.mController->_isContainerScreen()) return;
            tick(screen);
        } catch (...) { try { finish("an error"); } catch (...) {} }
    });
    exitListener = bus.emplaceListener<ll::event::ClientExitLevelEvent>([](auto&) { run = {}; });
}
void stop() {
    for (auto* listener : {&renderListener, &exitListener})
        if (*listener) { ll::event::EventBus::getInstance().removeListener(*listener); listener->reset(); }
    run = {};
}
}
}
