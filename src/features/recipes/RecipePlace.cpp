#include "features/recipes/RecipePlace.h"
#include "app/Runtime.h"
#include "features/inventory/game/TransferRoutes.h"
#include "mc/client/gui/screens/ScreenController.h"
#include "mc/client/gui/screens/ScreenView.h"
#include "mc/client/gui/screens/controllers/ContainerScreenController.h"
#include "mc/client/gui/screens/controllers/CraftingScreenController.h"
#include "mc/world/containers/managers/controllers/ContainerManagerController.h"
#include "mc/world/containers/managers/controllers/CraftingContainerManagerController.h"
#include "mc/world/item/ItemStack.h"
#include <climits>
#include <string>

namespace lamium::recipes::place {
namespace {
std::string const grid = "crafting_input_items", output = "crafting_output_items", pointer = "cursor_items", hotbar = "hotbar_items",
    inventory = "inventory_items";
// A run of "craft all": the recipe again each round while ingredients are left outside the grid.
// Also a recipe waiting to be laid in: what lay in the grid is on its way back to the inventory.
struct Run {
    std::shared_ptr<service::Table const> table;
    int recipe = -1, rounds = 0, wait = 0;
    std::map<int, Item> picks;
    std::string screen;
    bool many = false, craft = false;
    int tries = 0; // frames waited for the grid to read empty
} going;
constexpr int mostRounds = 64, framesBetween = 3, mostTries = 20;
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
// One round, into an empty grid: the recipe laid in and, with `craft`, the output taken.
bool round(CraftingScreenController& controller, Index const& index, Recipe const& recipe, std::map<int, Item> const& picks, bool many, bool craft) {
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
    if (!craft) return true;
    // The output slot reads empty while the game shows a product (seen 2026-11-18): the recipe the grid makes tells.
    if (!static_cast<CraftingContainerManagerController&>(manager).mCraftingRecipe) return false;
    // What a Shift click on the output does: all the grid makes, as far as the inventory has room.
    controller._handleAutoPlace(INT_MAX, output, 0);
    return true;
}
}
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
bool run(ScreenView& screen, std::shared_ptr<service::Table const> const& table, int recipe, std::map<int, Item> const& picks, bool many, bool craft) {
    going = {};
    try {
        auto* controller = crafting(screen);
        if (!controller || !table || recipe < 0 || recipe >= static_cast<int>(table->index.recipes.size())) return false;
        auto& manager = *controller->mContainerManagerController;
        if (held(manager)) return false;
        if (!gridEmpty(manager)) {
            controller->_evacuateCraftingGrid();
            // Not read as empty at once: the recipe is laid in when it is, a few frames on.
            if (!gridEmpty(manager)) {
                going = {table, recipe, 0, 0, picks, screen.getScreenName(), many, craft, 0};
                return true;
            }
        }
        if (!round(*controller, table->index, table->index.recipes[static_cast<size_t>(recipe)], picks, many, craft)) return false;
        if (craft) going = {table, recipe, 1, framesBetween, picks, screen.getScreenName(), many, true, 0};
        return true;
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
        if (going.wait-- > 0) return;
        auto& manager = *controller->mContainerManagerController;
        auto const& recipe = going.table->index.recipes[static_cast<size_t>(going.recipe)];
        if (!going.rounds) {
            // Waiting for what lay in the grid to be back in the inventory (no room for it: given up).
            if (!gridEmpty(manager)) { if (++going.tries > mostTries) going = {}; return; }
            if (!round(*controller, going.table->index, recipe, going.picks, going.many, going.craft) || !going.craft) { going = {}; return; }
            going.rounds = 1;
            going.wait = framesBetween;
            return;
        }
        // What the last take left in the grid found no room: the run ends there, the grid as it is.
        if (going.rounds >= mostRounds || held(manager) || !gridEmpty(manager)
            || !round(*controller, going.table->index, recipe, going.picks, going.many, true)) {
            going = {};
            return;
        }
        ++going.rounds;
        going.wait = framesBetween;
    } catch (...) { going = {}; }
}
void stop() { going = {}; }
}
