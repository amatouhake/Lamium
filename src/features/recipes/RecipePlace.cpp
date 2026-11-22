#include "features/recipes/RecipePlace.h"
#include <utility>
#include <ranges>
#include "app/Runtime.h"
#include "features/inspection/hover/HoverTracker.h"
#include "features/inventory/game/TransferRoutes.h"
#include "features/recipes/ItemKey.h"
#include "features/recipes/ItemList.h"
#include "features/recipes/MassCraftPlan.h"
#include "features/recipes/RecipeState.h"
#include "features/schematic/SchematicItems.h"
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
#include <format>
#include <numeric>
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
    int most = 64; // with `many`, no more crafts than that (L-385)
    int tries = 0; // frames waited for the grid to read empty
    std::chrono::steady_clock::time_point seen; // last ticked: a screen closed meanwhile drops it
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
// The empty slots of the hotbar and the inventory.
int emptySlots(ContainerManagerController& manager) {
    int out = 0;
    for (auto const& [collection, size] : {std::pair{&hotbar, 9}, std::pair{&inventory, 27}}) {
        if (!manager.hasContainerController(*collection) || manager.getContainerSize(*collection) < size) continue;
        for (int i = 0; i < size; ++i)
            if (auto const& stack = manager.getItemStack(*collection, i); stack.isNull() || stack.mCount <= 0) ++out;
    }
    return out;
}
int cellCount(ContainerManagerController& manager, int cell) {
    auto const& stack = manager.getItemStack(grid, cell);
    return stack.isNull() ? 0 : static_cast<int>(stack.mCount);
}
// Moves from the inventory into the grid. Each goes straight from its slot to its cell, as Sort and Item transfer
// move stacks (the manager's place-amount), so what the pointer holds stays there (L-381). Where a move shows
// nothing, it goes as a hand would: taken onto the pointer and put down, only while the pointer is empty. False
// where the game did not do as planned; what the pointer holds then is left to the player. The first of each
// way a session is logged. `spent`, if given, grows by the requests sent: one a straight move, three for a cell that
// went by the pointer (the straight move was sent too).
bool apply(CraftingScreenController& controller, std::vector<PlaceMove> const& moves, int* spent = nullptr) {
    auto& manager = *controller.mContainerManagerController;
    static bool toldDirect = false, toldHand = false;
    for (auto const& move : moves) {
        bool const top = move.slot < 9;
        std::string const& from = top ? hotbar : inventory;
        int const slot = top ? move.slot : move.slot - 9, before = cellCount(manager, move.cell);
        if (spent) ++*spent;
        if (manager.handlePlaceAmount(SlotData(from, slot), move.amount, SlotData(grid, move.cell)) && cellCount(manager, move.cell) == before + move.amount) {
            if (!toldDirect) toldDirect = true, Runtime::instance().self().getLogger().info("Recipes: a cell laid in straight from its slot");
            continue;
        }
        if (cellCount(manager, move.cell) != before || held(manager)) return false;
        if (spent) *spent += 2;
        controller._handleTakeAmount(move.amount, from, slot);
        if (held(manager) != move.amount) return false;
        controller._handlePlaceAll(grid, move.cell);
        if (held(manager)) return false;
        if (!toldHand) toldHand = true, Runtime::instance().self().getLogger().info("Recipes: a cell laid in through the pointer");
    }
    return true;
}
// The recipe laid into an empty grid.
bool lay(CraftingScreenController& controller, Index const& index, Recipe const& recipe, std::map<int, Item> const& picks, bool many,
         int* spent = nullptr, int most = 64) {
    auto& manager = *controller.mContainerManagerController;
    if (!gridEmpty(manager)) return false;
    auto const plan = placePlan(index, recipe, sideOf(manager), carriedOf(manager, index), picks, many, most);
    if (!plan.ready()) return false;
    return apply(controller, plan.moves, spent);
}
// A station's slots, one per recipe cell in order (L-380; the names from the game's UI files, 2026-11-21).
std::vector<std::string> const& stationSlots(Station station) {
    static std::vector<std::string> const furnace{"furnace_ingredient_items"}, stonecutter{"stonecutter_input_items"},
        smithing{"smithing_table_template_items", "smithing_table_input_items", "smithing_table_material_items"},
        brewing{"brewing_input_item", "brewing_result_items"}, none; // the stand: the ingredient, then its three bottles
    switch (station) {
    case Station::Furnace: case Station::BlastFurnace: case Station::Smoker: return furnace;
    case Station::Stonecutter: return stonecutter;
    case Station::Smithing: return smithing;
    case Station::BrewingStand: return brewing;
    default: return none;
    }
}
// The open screen as a station whose slots take a recipe, by its container's type; null elsewhere, or where the
// slot calls are not confirmed or a slot is not there.
ContainerScreenController* station(ScreenView& screen, Station& out) {
    if (!screen.mController || !screen.mController->_isContainerScreen()) return nullptr;
    auto& controller = static_cast<ContainerScreenController&>(*screen.mController);
    if (!inventory::game::transferRoutes::supported(controller)) return nullptr;
    auto manager = controller.mContainerManagerController;
    using Type = SharedTypes::Legacy::ContainerType;
    switch (manager->getContainerType()) {
    case Type::Furnace: out = Station::Furnace; break;
    case Type::BlastFurnace: out = Station::BlastFurnace; break;
    case Type::Smoker: out = Station::Smoker; break;
    case Type::Stonecutter: out = Station::Stonecutter; break;
    case Type::SmithingTable: out = Station::Smithing; break;
    case Type::BrewingStand: out = Station::BrewingStand; break;
    default: return nullptr;
    }
    for (auto const& slot : stationSlots(out))
        if (!manager->hasContainerController(slot) || manager->getContainerSize(slot) < 1) return nullptr;
    return &controller;
}
int slotCount(ContainerManagerController& manager, std::string const& slot) {
    auto const& stack = manager.getItemStack(slot, 0);
    return stack.isNull() ? 0 : static_cast<int>(stack.mCount);
}
// A station's recipe laid into its slots, each cell's amount straight from the slots carried. A slot that holds
// another item is emptied into the inventory first; one that holds the same item gets more, as far as its stack
// has room. False where nothing was laid in or the game did not do as planned.
bool layAtStation(ContainerScreenController& controller, Station at, Index const& index, Recipe const& recipe, std::map<int, Item> const& picks,
                  bool many, int most = 64) {
    auto& manager = *controller.mContainerManagerController;
    auto const& slots = stationSlots(at);
    if (static_cast<int>(recipe.cells.size()) != static_cast<int>(slots.size())) return false;
    auto carried = carriedOf(manager, index);
    auto plan = placePlan(index, recipe, 0, carried, picks, many, most);
    if (!plan.ready()) return false;
    auto itemAt = [&](std::vector<PlaceSource> const& from, int slot) {
        for (auto const& source : from) if (source.slot == slot) return source.item;
        return Item{-1};
    };
    bool emptied = false;
    for (auto const& move : plan.moves) {
        auto const& slot = slots[static_cast<size_t>(move.cell)];
        auto const& stack = manager.getItemStack(slot, 0);
        if (stack.isNull() || index.find(stack.getTypeName(), stack.getAuxValue()) == itemAt(carried, move.slot)) continue;
        controller._handleAutoPlace(INT_MAX, slot, 0);
        if (!manager.getItemStack(slot, 0).isNull()) return false;
        emptied = true;
    }
    // What went back may have joined a stack the plan draws from: planned again.
    if (emptied) {
        carried = carriedOf(manager, index);
        plan = placePlan(index, recipe, 0, carried, picks, many, most);
        if (!plan.ready()) return false;
    }
    bool any = false;
    for (auto const& move : plan.moves) {
        auto const& slot = slots[static_cast<size_t>(move.cell)];
        int stack = 64;
        for (auto const& source : carried) if (source.slot == move.slot) stack = source.stack;
        int const before = slotCount(manager, slot), amount = std::min(move.amount, stack - before);
        if (amount <= 0) continue;
        bool const top = move.slot < 9;
        if (!manager.handlePlaceAmount(SlotData(top ? hotbar : inventory, top ? move.slot : move.slot - 9), amount, SlotData(slot, 0))
            || slotCount(manager, slot) != before + amount)
            return any;
        any = true;
    }
    // The brewing stand with Shift (L-361): the bottle before into its other two bottles too, one ingredient brewing
    // all three. A bottle holding another potion goes back first, as any slot.
    if (at == Station::BrewingStand && many) {
        Item const bottle = itemAt(carried, plan.moves.back().slot);
        for (int i = 1; i < 3 && i < manager.getContainerSize(slots[1]); ++i) {
            auto const& held = manager.getItemStack(slots[1], i);
            if (!held.isNull() && index.find(held.getTypeName(), held.getAuxValue()) == bottle) continue;
            if (!held.isNull()) {
                controller._handleAutoPlace(INT_MAX, slots[1], i);
                if (!manager.getItemStack(slots[1], i).isNull()) break;
            }
            int from = -1;
            for (auto const& source : carriedOf(manager, index)) if (source.item == bottle && source.count > 0) { from = source.slot; break; }
            if (from < 0) break;
            bool const top = from < 9;
            if (!manager.handlePlaceAmount(SlotData(top ? hotbar : inventory, top ? from : from - 9), 1, SlotData(slots[1], i))) break;
            any = true;
        }
    }
    return any;
}
// The stonecutter's input Lamium laid in, until a result is chosen (L-380): Lamium can neither choose it nor read
// the choice, so a notice asks for it while this screen keeps that input and its result list was not pressed.
struct Notice { void const* screen = nullptr; std::string input; } notice;
std::string keyOf(ItemStack const& stack) { return stack.isNull() ? std::string() : itemKey(stack.getTypeName(), stack.getAuxValue()); }
}
namespace place {
bool stonecutterNotice(ScreenView& screen, bool pressed) {
    if (!notice.screen) return false;
    try {
        Station at = Station::Other;
        auto* controller = station(screen, at);
        if (!controller || at != Station::Stonecutter || controller != notice.screen) { notice = {}; return false; }
        auto& manager = *controller->mContainerManagerController;
        if (keyOf(manager.getItemStack(stationSlots(at).front(), 0)) != notice.input) { notice = {}; return false; }
        // A press on the result list (the screen's own collection `stones`, seen 2026-11-21) chooses one.
        if (pressed && *controller->mHoveredSlotData->mCollectionName == "stones") { notice = {}; return false; }
        return true;
    } catch (...) { notice = {}; return false; }
}
Bench bench(ScreenView& screen) {
    try {
        if (auto* controller = crafting(screen)) return {Station::Crafting, sideOf(*controller->mContainerManagerController)};
        Station at = Station::Other;
        if (station(screen, at)) return {at, 0};
    } catch (...) {}
    return {};
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
        Station at = Station::Other;
        // At a station only what is carried counts: a slot's own stays where it is or goes back whole. On any other
        // item screen too, for what a recipe would miss (the recipe view's place buttons).
        if (!controller) {
            if (auto* other = station(screen, at)) return carriedOf(*other->mContainerManagerController, index);
            if (!screen.mController || !screen.mController->_isContainerScreen()) return {};
            auto manager = static_cast<ContainerScreenController&>(*screen.mController).mContainerManagerController;
            return manager && !manager->mContainersClosed ? carriedOf(*manager, index) : std::vector<PlaceSource>{};
        }
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
bool run(ScreenView& screen, std::shared_ptr<service::Table const> const& table, int recipe, std::map<int, Item> const& picks, bool many,
         int most) {
    going = {};
    try {
        if (!table || recipe < 0 || recipe >= static_cast<int>(table->index.recipes.size())) return false;
        auto* controller = crafting(screen);
        if (!controller) {
            Station at = Station::Other;
            auto* other = station(screen, at);
            auto const& shape = table->index.recipes[static_cast<size_t>(recipe)];
            if (!other || !benchMakes({at, 0}, shape)) return false;
            bool const laid = layAtStation(*other, at, table->index, shape, picks, many, most);
            if (laid && at == Station::Stonecutter)
                notice = {other, keyOf(other->mContainerManagerController->getItemStack(stationSlots(at).front(), 0))};
            static bool told = false;
            if (laid && !told) told = true, Runtime::instance().self().getLogger().info("Recipes: a recipe laid in at a station, straight from its slots");
            return laid;
        }
        auto& manager = *controller->mContainerManagerController;
        if (!gridEmpty(manager)) {
            // Already laid as asked: taking it out and laying it in again only shows items flying (L-367).
            // With `many`, only when every cell holds as many as dividing would lay, the grid counted in.
            int const side = sideOf(manager);
            auto const& shape = table->index.recipes[static_cast<size_t>(recipe)];
            std::vector<Item> cells;
            std::vector<int> counts;
            for (int i = 0, size = manager.getContainerSize(grid); i < size && i < side * side; ++i) {
                auto const& stack = manager.getItemStack(grid, i);
                bool const empty = stack.isNull() || stack.mCount <= 0;
                cells.push_back(empty ? -1 : table->index.find(stack.getTypeName(), stack.getAuxValue()));
                counts.push_back(empty ? 0 : static_cast<int>(stack.mCount));
            }
            if (alreadyLaid(table->index, shape, side, cells)) {
                // A press adds one more of each, as the game's recipe book does; nothing when a cell is full or
                // something is short.
                if (!many) {
                    auto const moves = oneMore(table->index, shape, side, cells, counts, carriedOf(manager, table->index));
                    if (!moves.empty()) apply(*controller, moves);
                    return true;
                }
                auto const full = placePlan(table->index, shape, side, carried(screen, table->index), picks, true, most);
                if (full.ready() && std::ranges::all_of(std::views::iota(size_t{0}, cells.size()),
                                                        [&](size_t i) { return cells[i] < 0 || counts[i] == full.crafts; }))
                    return true;
            }
        }
        if (!gridEmpty(manager)) {
            controller->_evacuateCraftingGrid();
            // Not read as empty at once: the recipe is laid in when it is, a few frames on.
            if (!gridEmpty(manager)) {
                going = {table, recipe, picks, screen.getScreenName(), many, most, 0, std::chrono::steady_clock::now()};
                return true;
            }
        }
        return lay(*controller, table->index, table->index.recipes[static_cast<size_t>(recipe)], picks, many, nullptr, most);
    } catch (...) {
        try { Runtime::instance().self().getLogger().warn("Recipes: placing a recipe failed"); } catch (...) {}
        return false;
    }
}
void tickOnto(ScreenView& screen);
// A round being crafted onto the pointer (below): Mass Craft waits for it, since both move the same stacks.
bool ontoBusy();
namespace { void tickMaking(ScreenView& screen); }
void tick(ScreenView& screen) {
    tickOnto(screen);
    tickMaking(screen);
    if (going.recipe < 0) return;
    auto const now = std::chrono::steady_clock::now();
    if (now - going.seen > std::chrono::milliseconds(300)) { going = {}; return; }
    going.seen = now;
    try {
        auto* controller = crafting(screen);
        if (!controller || screen.getScreenName() != going.screen) { going = {}; return; }
        // Waiting for what lay in the grid to be back in the inventory (no room for it: given up).
        if (!gridEmpty(*controller->mContainerManagerController)) { if (++going.tries > mostTries) going = {}; return; }
        lay(*controller, going.table->index, going.table->index.recipes[static_cast<size_t>(going.recipe)], going.picks, going.many, nullptr,
            going.most);
    } catch (...) {}
    going = {};
}
namespace {
// L-385: steps of a plan being made. Lay: what the grid holds goes back, then a step's rounds are laid in; Take: the
// output taken as a Shift click takes it, and once the grid reads empty the rounds are made.
struct Making {
    bool on = false;
    std::shared_ptr<service::Table const> table;
    std::map<int, Item> picks;
    std::vector<MakeRun> steps; // what is left, in order
    std::string screen;
    enum class Phase { Lay, Take } phase = Phase::Lay;
    int laid = 0;   // rounds laid in by the last Lay
    bool took = false;
    int tries = 0;  // steps waited for the grid to read empty, or for the take to show
    int made = 0;   // rounds made, for the log
    int refused = 0; // lays in a row the grid gave back before any take
    std::chrono::steady_clock::time_point next, seen; // seen: the last frame its screen was drawn
} making;
void finishMaking(char const* why) {
    if (making.on) {
        try { Runtime::instance().self().getLogger().info("Recipes: a plan's steps made, {} rounds ({})", making.made, why); } catch (...) {}
    }
    making = {};
}
void tickMaking(ScreenView& screen) {
    if (!making.on) return;
    auto const now = std::chrono::steady_clock::now();
    // Its screen closed meanwhile (and maybe opened again, or another world joined): the run is over.
    if (now - making.seen > std::chrono::milliseconds(300)) { finishMaking("the screen went"); return; }
    making.seen = now;
    if (now < making.next) return;
    making.next = now + std::chrono::milliseconds(50);
    try {
        auto* controller = crafting(screen);
        if (!controller || screen.getScreenName() != making.screen) { finishMaking("the screen went"); return; }
        auto& manager = *controller->mContainerManagerController;
        if (making.steps.empty()) { finishMaking("done"); return; }
        auto& step = making.steps.front();
        auto const& index = making.table->index;
        if (making.phase == Making::Phase::Lay) {
            if (!gridEmpty(manager)) {
                if (!making.tries++) controller->_evacuateCraftingGrid();
                if (making.tries > mostTries) finishMaking("the grid did not empty");
                return;
            }
            making.tries = 0;
            // Of the step's recipes the one laid most of now: a group's member whose input an earlier step made counts.
            // No more rounds than the inventory has room for once they are laid in (2026-11-22): a take the inventory
            // cannot hold stuck the grid, and the run ended there.
            auto const carried = carriedOf(manager, index);
            int const empty = emptySlots(manager);
            PlacePlan plan;
            for (int number : step.recipes) {
                auto const& recipe = index.recipes[static_cast<size_t>(number)];
                int const stack = recipe.makes >= 0 ? std::max(1, schematic::items::shown(index.items[static_cast<size_t>(recipe.makes)]).stack) : 64;
                int const makes = std::max(1, static_cast<int>(recipe.count));
                // Fewer rounds take fewer inputs and may free fewer slots, so the room is looked at again a few times.
                int most = step.rounds;
                for (int tries = 0; tries < 4 && most > 0; ++tries) {
                    auto next = placePlan(index, recipe, sideOf(manager), carried, making.picks, true, most);
                    if (!next.ready()) break;
                    int const fits = static_cast<int>(std::min<long long>(roomAfter(carried, next.moves, empty, recipe.makes, stack) / makes, 1 << 20));
                    if (next.crafts <= fits) {
                        if (!plan.ready() || next.crafts > plan.crafts) plan = std::move(next);
                        break;
                    }
                    most = std::min(fits, next.crafts - 1);
                }
            }
            if (!plan.ready() || step.rounds <= 0) {
                // Nothing more of this step can be laid in, from what is carried or for the room left: the next one.
                making.steps.erase(making.steps.begin());
                return;
            }
            if (!apply(*controller, std::as_const(plan.moves))) { finishMaking("laying a round in failed"); return; }
            making.laid = plan.crafts;
            making.took = false;
            making.phase = Making::Phase::Take;
            return;
        }
        if (gridEmpty(manager)) {
            if (!making.took) {
                // Empty before any take was sent: the lay was refused and the inputs went back (L-390 1-7). Nothing
                // was made; it is laid in again, and the run ends when that keeps happening.
                if (++making.refused > mostTries) { finishMaking("the rounds laid in were given back"); return; }
                making.phase = Making::Phase::Lay;
                making.tries = 0;
                return;
            }
            making.refused = 0;
            step.rounds -= making.laid;
            making.made += making.laid;
            if (step.rounds <= 0) making.steps.erase(making.steps.begin());
            making.phase = Making::Phase::Lay;
            making.tries = 0;
            return;
        }
        // What a Shift click on the output does: all the grid makes, as far as the inventory has room. The client's
        // output slot does not show the product to read (seen 2026-11-22: a run waiting for it never took), so the
        // take is sent as Mass Craft sends it, and again every few steps while the grid stays laid.
        if (!making.took || making.tries % 5 == 4) {
            controller->_handleAutoPlace(INT_MAX, output, 0);
            making.took = true;
        }
        if (++making.tries > mostTries) {
            // No room for the product, or the recipe leaves something in the grid (a bucket, L-362).
            controller->_evacuateCraftingGrid();
            finishMaking("the grid did not empty after the take");
        }
    } catch (...) {
        finishMaking("a step failed");
    }
}
}
bool make(ScreenView& screen, std::shared_ptr<service::Table const> const& table, std::vector<MakeRun> steps, std::map<int, Item> const& picks) {
    finishMaking("started again");
    going = {};
    if (!table || steps.empty()) return false;
    for (auto const& step : steps)
        if (step.recipes.empty() || std::ranges::any_of(step.recipes, [&](int number) { return number < 0 || number >= static_cast<int>(table->index.recipes.size()); }))
            return false;
    try {
        if (!crafting(screen)) {
            // At a station its slots take the first step's rounds, and the station makes them.
            auto const& first = steps.front();
            return run(screen, table, first.recipes.front(), picks, true, first.rounds);
        }
    } catch (...) { return false; }
    making = {};
    making.on = true;
    making.table = table;
    making.picks = picks;
    making.steps = std::move(steps);
    making.screen = screen.getScreenName();
    making.seen = std::chrono::steady_clock::now();
    return true;
}
void stop() { going = {}; finishMaking("stopped"); }
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
// `mostRequests`: the slot calls a step may make (a move into a cell is one, two where it goes by the pointer;
// L-381). A local world closed with a
// network error under some hundreds a step's worth a second; a step stays well under that.
constexpr int mostPasses = 10, mostRequests = 16;
struct Run {
    bool on = false, over = false; // over: this press of the key does nothing more
    bool fromGrid = false;         // started over the output slot: it goes on only while the pointer stays there
    std::shared_ptr<service::Table const> table;
    int recipe = -1;
    std::map<int, Item> picks;
    Phase phase = Phase::Take;
    std::vector<int> kept; // of the product, per slot, what the player carried when the run began: it stays
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
    auto const choice = state::choice(shared);
    for (int recipe : recipesToTry(table.index, choice.get(), item))
        if (placePlan(table.index, table.index.recipes[static_cast<size_t>(recipe)], side, {}, {}, false).fits) return recipe;
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
    if (service::want() != service::State::Ready || place::ontoBusy()) return;
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
    // A plan's steps being made, or a recipe waiting to be laid in, use the same grid: the player's run wins.
    place::stop();
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
        run.kept.push_back(item == run.table->index.recipes[static_cast<size_t>(recipe)].makes ? count : 0);
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
// The pointer was empty when the run began, so what it holds is the run's. `spent` grows by the calls sent.
bool clearPointer(CraftingScreenController& controller, Index const& index, Item product, int& spent) {
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
                if (takes) controller._handlePlaceAll(collectionOf(slot), indexOf(slot)), ++spent;
            }
    }
    if (held(manager)) inventory::game::transferRoutes::dropCursor(controller), ++spent;
    return !held(manager);
}
// Throws out what the inventory holds of the product beyond what was carried at the start: how many
// throws, -1 when one could not be sent. `budget`: the slot calls the step has left.
int throwExcess(CraftingScreenController& controller, Index const& index, Item product, int& budget) {
    bool anyFree = false;
    int thrown = 0;
    for (auto const& out : excess(slotsOf(*controller.mContainerManagerController, index, anyFree), product, run.kept)) {
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
        int spent = 0;
        bool const cleared = clearPointer(controller, index, recipe.makes, spent);
        run.requests += spent;
        if (!cleared) { finish("the pointer holds an item"); return; }
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
                held(manager), carried, free, plan.fits, plan.missing.size(), plan.crafts, plan.moves.size(), std::accumulate(run.kept.begin(), run.kept.end(), 0));
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
        if (budget <= 0) return; // the take waits for the next step
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
            if (budget < 2) return; // the drop and the take go in one step
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
        // Another item on the pointer is put away first, and the step tried again.
        auto foreign = [&] {
            if (!have || product()) return false;
            int spent = 0;
            bool const cleared = clearPointer(controller, index, recipe.makes, spent);
            budget -= spent;
            if (!cleared) finish("the pointer holds an item");
            else ++run.cleared, again("the pointer holds an item", now);
            return true;
        };
        if (foreign()) return;
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
            if (foreign()) return;
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
            // A round is laid in by a step that has all its calls left, however many cells it has: a move straight
            // from its slot is one call (L-381), one that went by the pointer three, counted as sent.
            int const cost = static_cast<int>(plan.moves.size());
            if (cost > budget && budget < mostRequests) { run.wait = "the step has no calls left to lay in"; return; }
            int calls = 0;
            // Laid in part: the take that follows finds no product, and what was laid goes back.
            bool const laid = lay(controller, index, recipe, run.picks, true, &calls);
            budget -= calls;
            if (!laid && held(manager)) { again("laying the recipe in failed", now); return; }
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
        // Within the step's calls: what is left goes after the wait, if a slot is still wanted then.
        for (int slot : surplusSlots(index, recipe, carried)) {
            if (budget <= 0) break;
            if (!inventory::game::transferRoutes::drop(controller, collectionOf(slot), indexOf(slot), false)) { finish("throwing a surplus out failed"); return; }
            --budget;
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
// Crafting one round onto the pointer (L-379): the grid emptied, the round laid in, the output taken onto the
// pointer (the slot call's take off the output crafts, seen 2026-11-21), onto what it holds of the product. Where
// it holds another item or no room is left, the round is laid in only, and nothing it holds moves (L-381). Where
// nothing shows a moment after the take: with an empty pointer the output is Shift-taken (as Mass Craft does) and
// the round picked up from the inventory; else the round stays laid in. One log line says which way it went.
namespace place {
namespace {
using Clock = std::chrono::steady_clock;
constexpr auto ontoWait = std::chrono::milliseconds(200), ontoGiveUp = std::chrono::milliseconds(1500);
// A run not ticked for this long belongs to a screen that was closed: it is dropped without a game call.
constexpr auto ontoStale = std::chrono::milliseconds(500);
struct Onto {
    std::shared_ptr<service::Table const> table;
    int recipe = -1;
    std::map<int, Item> picks;
    std::string screen;
    enum class Step { Clear, Laid, Take, Fetch } step = Step::Clear;
    int heldBefore = 0; // what the pointer held of the product before the take
    int tries = 0;
    std::vector<int> before; // Fetch: the product per slot (0-8 the hotbar, 9-35 the inventory) before the Shift take
    Clock::time_point began, since, ticked;
} onto;
Item heldItem(ContainerManagerController& manager, Index const& index) {
    auto const& stack = manager.getItemStack(pointer, 0);
    return stack.isNull() || stack.mCount <= 0 ? -1 : index.find(stack.getTypeName(), stack.getAuxValue());
}
int heldMost(ContainerManagerController& manager) {
    auto const& stack = manager.getItemStack(pointer, 0);
    return stack.isNull() ? 64 : std::max(1, static_cast<int>(stack.getMaxStackSize()));
}
std::vector<int> productCounts(ContainerManagerController& manager, Index const& index, Item product) {
    std::vector<int> out(36, 0);
    for (int i = 0; i < 36; ++i) {
        auto const& stack = manager.getItemStack(i < 9 ? hotbar : inventory, i < 9 ? i : i - 9);
        if (!stack.isNull() && stack.mCount > 0 && index.find(stack.getTypeName(), stack.getAuxValue()) == product)
            out[static_cast<size_t>(i)] = static_cast<int>(stack.mCount);
    }
    return out;
}
void logOnto(std::string const& text) {
    try { Runtime::instance().self().getLogger().info("Recipes: {}", text); } catch (...) {}
}
// Ends the run; `tidy`: what a recipe left in the grid goes back to the inventory (not when the round is left
// laid in on purpose).
void finishOnto(CraftingScreenController& controller, std::string const& how, bool tidy = true) {
    auto& manager = *controller.mContainerManagerController;
    if (tidy && !gridEmpty(manager)) controller._evacuateCraftingGrid();
    auto const ms = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - onto.began).count();
    logOnto(std::format("crafting {} onto the pointer {} ({} ms)", onto.table->index.recipes[static_cast<size_t>(onto.recipe)].id, how, ms));
    onto = {};
}
void advanceOnto(CraftingScreenController& controller) {
    auto& manager = *controller.mContainerManagerController;
    auto const& index = onto.table->index;
    auto const& recipe = index.recipes[static_cast<size_t>(onto.recipe)];
    auto const now = Clock::now();
    auto holdsMore = [&] { return heldItem(manager, index) == recipe.makes && held(manager) > onto.heldBefore; };
    // Crafted: more of the product on the pointer and the round gone from the grid (one round was laid in).
    auto crafted = [&] { return holdsMore() && !massCraft::makesProduct(manager, index, recipe.makes); };
    switch (onto.step) {
    case Onto::Step::Clear:
        if (!gridEmpty(manager)) {
            if (!onto.tries) controller._evacuateCraftingGrid();
            if (!gridEmpty(manager)) {
                if (++onto.tries > mostTries) finishOnto(controller, "given up: the grid did not empty");
                return;
            }
        }
        if (!lay(controller, index, recipe, onto.picks, false)) { finishOnto(controller, "given up: the round could not be laid in"); return; }
        onto.step = Onto::Step::Laid, onto.since = now;
        [[fallthrough]];
    case Onto::Step::Laid:
        // The output may show a few frames after the round is laid in.
        if (!massCraft::makesProduct(manager, index, recipe.makes)) {
            if (now - onto.since > ontoWait) finishOnto(controller, "given up: the grid did not make it");
            return;
        }
        onto.heldBefore = heldItem(manager, index) == recipe.makes ? held(manager) : 0;
        controller._handleTakeAmount(static_cast<int>(recipe.count), output, 0);
        onto.step = Onto::Step::Take, onto.since = now;
        if (crafted()) finishOnto(controller, onto.heldBefore ? "onto the held stack by taking the output" : "by taking the output");
        return;
    case Onto::Step::Take:
        if (crafted()) { finishOnto(controller, "by taking the output, a moment later"); return; }
        if (now - onto.since < ontoWait) return;
        // More on the pointer while the grid still makes it: shown by the client only, not crafted.
        if (holdsMore()) { finishOnto(controller, "not confirmed: the pointer shows more but the grid still holds the round"); return; }
        // With something held the round stays laid in, as when the pointer had no room.
        if (held(manager)) { finishOnto(controller, "not taken onto the held stack: the round is left laid in", false); return; }
        onto.before = productCounts(manager, index, recipe.makes);
        controller._handleAutoPlace(INT_MAX, output, 0);
        onto.step = Onto::Step::Fetch, onto.since = now;
        [[fallthrough]];
    case Onto::Step::Fetch: {
        auto const after = productCounts(manager, index, recipe.makes);
        for (int i = 0; i < 36; ++i) {
            int const more = after[static_cast<size_t>(i)] - onto.before[static_cast<size_t>(i)];
            if (more <= 0) continue;
            controller._handleTakeAmount(more, i < 9 ? hotbar : inventory, i < 9 ? i : i - 9);
            finishOnto(controller, crafted() ? "by the Shift take, then from the inventory" : "given up: the Shift take's product could not be picked up");
            return;
        }
        if (now - onto.since > ontoGiveUp) finishOnto(controller, "given up: nothing came");
        return;
    }
    }
}
}
void abandonCraft() { onto = {}; finishMaking("the screen went"); }
bool ontoBusy() { return onto.recipe >= 0 && Clock::now() - onto.ticked <= ontoStale; }
void tickOnto(ScreenView& screen) {
    if (onto.recipe < 0) return;
    if (auto const now = Clock::now(); now - onto.ticked > ontoStale) {
        logOnto("crafting onto the pointer dropped: its screen was not drawn");
        onto = {};
        return;
    } else onto.ticked = now;
    try {
        auto* controller = crafting(screen);
        if (!controller || screen.getScreenName() != onto.screen) {
            logOnto("crafting onto the pointer given up: the screen went");
            onto = {};
            return;
        }
        advanceOnto(*controller);
    } catch (...) { onto = {}; }
}
CraftOnto craftCheck(ScreenView& screen, Index const& index, Recipe const& recipe, bool ready) {
    try {
        auto* controller = crafting(screen);
        if (!controller) return CraftOnto::Missing;
        auto& manager = *controller->mContainerManagerController;
        return craftOnto(ready, recipe.makes, static_cast<int>(recipe.count), heldItem(manager, index), held(manager), heldMost(manager));
    } catch (...) { return CraftOnto::Missing; }
}
bool craft(ScreenView& screen, std::shared_ptr<service::Table const> const& table, int recipe, std::map<int, Item> const& picks) {
    if (onto.recipe >= 0 || !table || recipe < 0 || recipe >= static_cast<int>(table->index.recipes.size())) return false;
    try {
        auto* controller = crafting(screen);
        if (!controller) return false;
        auto& manager = *controller->mContainerManagerController;
        auto const& index = table->index;
        auto const& shape = index.recipes[static_cast<size_t>(recipe)];
        bool const ready = placePlan(index, shape, sideOf(manager), carried(screen, index), picks, false).ready();
        auto const check = craftCheck(screen, index, shape, ready);
        if (check == CraftOnto::Missing) return false;
        if (check == CraftOnto::PlaceOnly) {
            // The pointer cannot take the round: it is laid in only (maintainer 2026-11-21).
            bool const laid = run(screen, table, recipe, picks, false);
            logOnto(std::format("crafting {} onto the pointer: the pointer is taken, {}", shape.id, laid ? "laid in only" : "not laid in"));
            return laid;
        }
        onto = {};
        onto.table = table, onto.recipe = recipe, onto.picks = picks, onto.screen = screen.getScreenName();
        onto.began = onto.since = onto.ticked = Clock::now();
        advanceOnto(*controller);
        return true;
    } catch (...) {
        logOnto("crafting onto the pointer failed");
        onto = {};
        return false;
    }
}
}
}
