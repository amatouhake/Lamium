#include "features/inventory/DeathRestore.h"
#include "features/inventory/DeathLayoutStore.h"
#include "features/inventory/game/InventoryMove.h"
#include "features/map/WaypointSession.h"
#include "app/Runtime.h"
#include "input/Actions.h"
#include "ui/Localization.h"
#include "ui/SettingsScreen.h"
#include "ui/Toast.h"
#include "ll/api/service/TargetedBedrock.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/world/ClientLevelTickEvent.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/world/item/Item.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/item/enchanting/ItemEnchants.h"
#include "mc/world/item/enchanting/EnchantmentInstance.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <format>
#include <stdexcept>

namespace lamium::inventory::death {
namespace {
using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;
// Rearranging waits this long after the last pickup, then moves one stack
// at a time, reading the inventory again before each move.
constexpr std::chrono::milliseconds quietAfterPickup = 1s, betweenMoves = 250ms, whenBusy = 100ms;
constexpr int movesPerRun = 100, sameMoveLimit = 3;
constexpr int saveAgeEvery = 200; // Ticks: a rejoin loses at most 10 s of the count.
ll::event::ListenerPtr tickListener;
// Client thread only.
struct State {
    unsigned world = 0;
    std::optional<std::filesystem::path> file;
    std::optional<Layout> layout;
    Slots lastAlive{}, previous{};
    LifeWatch life;
    bool haveAlive = false, havePrevious = false;
    int busy = 0; // Moves refused in a row (another transaction in progress).
    bool armed = false;
    Clock::time_point lastPickup{}, nextStep{};
    std::optional<Move> lastMove;
    int repeats = 0, moves = 0;
} state;
void log(std::string const& text) {
    try { Runtime::instance().self().getLogger().info("Death layout: {}", text); } catch (...) {}
}
void save() {
    if (!state.file) return;
    try { writeLayout(*state.file, state.layout); }
    catch (std::exception const& error) { log(std::string("could not save: ") + error.what()); }
}
void forget() {
    state.layout.reset();
    state.armed = false;
    save();
}
game::Location location(int slot) {
    if (slot >= offhandSlot) return {game::Place::Offhand, 0};
    if (slot >= armorFirst) return {game::Place::Armor, slot - armorFirst};
    return {game::Place::Inventory, slot};
}
Stack describe(ItemStack const& item) {
    if (item.isNull() || item.mCount == 0 || !item.mItem) return {};
    Stack out;
    out.kind = item.getTypeName();
    out.count = item.mCount;
    out.maxStack = std::max<int>(1, item.getMaxStackSize());
    // What tells two of the same item apart: variant, wear, enchantments, name.
    out.exact = std::format("{}|{}|{}", out.kind, item.getAuxValue(), item.mItem->getMaxDamage() > 0 ? item.getDamageValue() : 0);
    if (item.isEnchanted())
        for (auto const& e : item.constructItemEnchantsFromUserData().getAllEnchants())
            out.exact += std::format("|e{}:{}", static_cast<int>(static_cast<::Enchant::Type>(e.mEnchantType)), e.mLevel);
    if (auto name = item.getCustomName(); !name.empty()) out.exact += "|n" + name;
    return out;
}
Slots read(LocalPlayer& player) {
    Slots slots{};
    for (int s = 0; s < slotCount; ++s) slots[s] = describe(game::itemAt(player, location(s)));
    return slots;
}
bool gameplay(ClientInstance& client) {
    return client.isInGameInputEnabled() && !ui::ownsInput() && gameplayScreen(client.getScreenName());
}
// One move through the client's own transaction path; false when busy.
bool perform(LocalPlayer& player, Move const& move) {
    auto a = location(move.from), b = location(move.to);
    ItemStack from = game::itemAt(player, a), to = game::itemAt(player, b);
    if (move.kind == MoveKind::Swap) return game::movePair(player, a, to, b, from);
    int n = std::min<int>(move.count, from.mCount);
    if (n <= 0) return false;
    ItemStack newTo = to.isNull() || to.mCount == 0 ? from : to;
    newTo.set(static_cast<int>(to.isNull() ? 0 : to.mCount) + n);
    ItemStack newFrom = from;
    if (from.mCount - n > 0) newFrom.set(from.mCount - n);
    else newFrom = ItemStack();
    return game::movePair(player, a, newFrom, b, newTo);
}
void follow() {
    auto place = map::waypoints::place();
    if (place.world == state.world) return;
    state = State{};
    state.world = place.world;
    if (!place.active) return;
    state.file = place.deathLayoutFile;
    if (!state.file) return;
    try { state.layout = readLayout(*state.file); }
    catch (std::exception const& error) { log(std::string("could not load: ") + error.what()); }
}
Scope scopeOf(Settings const& prefs) {
    return prefs.inventory.deathRestoreAll ? Scope::All : Scope::HotbarEquipment;
}
void step(LocalPlayer& player, Slots const& now, Settings const& prefs) {
    auto scope = scopeOf(prefs);
    auto before = state.layout->done;
    bool all = markDone(*state.layout, now, scope);
    if (all) {
        forget();
        try { ui::showMessageToast(ui::translated("deathRestore.done")); } catch (...) {}
        return;
    }
    if (state.layout->done != before) save();
    auto move = nextMove(*state.layout, now, scope);
    if (!move) { state.armed = false; return; }
    // A move the inventory keeps undoing (the server refused it) or a run
    // that does not settle stops until the next pickup.
    if (move == state.lastMove) {
        if (++state.repeats >= sameMoveLimit) { log("a move did not take effect; waiting for the next pickup"); state.armed = false; return; }
    } else {
        state.lastMove = move;
        state.repeats = 0;
    }
    if (++state.moves > movesPerRun) { log("too many moves; waiting for the next pickup"); state.armed = false; return; }
    if (!perform(player, *move)) {
        --state.moves;
        if (++state.busy >= 50) { log("inventory stayed busy; waiting for the next pickup"); state.armed = false; return; }
        state.nextStep = Clock::now() + whenBusy;
        return;
    }
    state.busy = 0;
    state.nextStep = Clock::now() + betweenMoves;
}
void tick() noexcept {
    try {
        auto& runtime = Runtime::instance();
        auto prefs = runtime.snapshot();
        follow();
        bool const on = runtime.enabled() && prefs->inventory.deathRestore;
        if (!on) { state.armed = false; state.haveAlive = false; }
        if (!runtime.enabled()) return;
        auto client = ll::service::getClientInstance();
        auto* player = client ? client->getLocalPlayer() : nullptr;
        if (!player || !player->mInventory) return;
        // The dropped items age whether or not the feature is on (L-338); a
        // run in progress finishes first.
        if (state.layout && !state.armed) {
            auto at = player->getFeetPos();
            int const before = state.layout->loadedTicks;
            if (age(*state.layout, loadedAround(*state.layout, static_cast<int>(player->getDimensionId()), at.x, at.z))) {
                log("the dropped items' five minutes are over");
                forget();
                if (on) try { ui::showMessageToast(ui::translated("deathRestore.expired")); } catch (...) {}
            } else if (state.layout->loadedTicks != before && state.layout->loadedTicks % saveAgeEvery == 0) save();
        }
        if (!on) return;
        bool alive = player->isAlive();
        auto event = state.life.update(alive);
        if (event == LifeWatch::Event::Died) {
            // The snapshot from the last tick alive: by now the server may
            // already be emptying the inventory.
            if (state.haveAlive && anyItem(state.lastAlive) && !player->isCreative()) {
                auto feet = player->getFeetPos();
                Layout layout;
                layout.x = static_cast<int>(std::floor(feet.x));
                layout.y = static_cast<int>(std::floor(feet.y));
                layout.z = static_cast<int>(std::floor(feet.z));
                layout.dimension = static_cast<int>(player->getDimensionId());
                layout.slots = state.lastAlive;
                state.layout = layout;
                state.armed = false;
                save();
            } else if (state.layout) {
                forget(); // A new death replaces the old layout, even an empty one.
            }
        }
        if (!alive) { state.havePrevious = false; return; }
        auto now = read(*player);
        state.lastAlive = now;
        state.haveAlive = true;
        if (event == LifeWatch::Event::Respawned) {
            // keepInventory: nothing was dropped, so there is nothing to restore.
            if (state.layout && anyItem(now)) { log("inventory kept at respawn"); forget(); }
        }
        if (!state.layout) { state.previous = now; state.havePrevious = true; return; }
        // The layout belongs to the place of the death (L-332): pickups count
        // there only, and leaving it after the first one ends the layout.
        auto feet = player->getFeetPos();
        bool here = atPlace(*state.layout, static_cast<int>(player->getDimensionId()), feet.x, feet.y, feet.z);
        if (state.layout->started && !here) {
            log("left the place of the death");
            forget();
            try { ui::showMessageToast(ui::translated("deathRestore.left")); } catch (...) {}
            state.previous = now;
            state.havePrevious = true;
            return;
        }
        if (state.havePrevious && !game::moving() && here && pickedUp(*state.layout, state.previous, now, scopeOf(*prefs))) {
            state.armed = true;
            if (!state.layout->started) { state.layout->started = true; save(); }
            state.lastPickup = Clock::now();
            state.moves = state.repeats = state.busy = 0;
            state.lastMove.reset();
        }
        state.previous = now;
        state.havePrevious = true;
        if (!state.armed || player->isCreative() || player->isSpectator()) return;
        auto time = Clock::now();
        if (time - state.lastPickup < quietAfterPickup || time < state.nextStep || !gameplay(*client)) return;
        step(*player, now, *prefs);
    } catch (std::exception const& error) {
        state.armed = false;
        static bool reported = false;
        if (!reported) { reported = true; log(std::string("stopped: ") + error.what()); }
    } catch (...) { state.armed = false; }
}
}
void start() {
    if (tickListener) return;
    tickListener = ll::event::EventBus::getInstance().emplaceListener<ll::event::ClientLevelTickEvent>([](auto&) { tick(); });
    if (!tickListener) throw std::runtime_error("Could not subscribe the death layout to client ticks");
}
void stop() {
    if (tickListener) {
        ll::event::EventBus::getInstance().removeListener(tickListener);
        tickListener.reset();
    }
    state = State{};
}
}
