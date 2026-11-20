#include "features/recipes/RecipeService.h"
#include "app/Runtime.h"
#include "app/Versions.h"
#include "features/recipes/RecipeSnapshot.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/client/ClientExitLevelEvent.h"
#include "ll/api/event/client/ClientStartJoinLevelEvent.h"
#include "ll/api/service/TargetedBedrock.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include <atomic>
#include <memory>
#include <thread>
#include <mutex>

namespace lamium::recipes::service {
namespace {
constexpr int recipesPerCall = 600; // about a millisecond; a world has four to five thousand
ll::event::ListenerPtr joinListener, exitListener;
std::mutex mutex;
std::shared_ptr<Table const> built;
std::atomic<State> state{State::Idle};
std::atomic<int> generation{0}; // a world's: a table built for one left is not published (DESIGN, stale async results)
std::atomic<bool> building{false};
game::Snapshot snapshot; // the client thread's alone
void reset() {
    ++generation;
    {
        std::lock_guard lock(mutex);
        built.reset();
    }
    state = State::Idle;
    snapshot.reset();
}
// One more part of the table. Driven by the screen that asks, not by the
// client tick: a screen can be open while the world's tick stands still.
void advance() noexcept {
    try {
        if (state.load() == State::Ready || state.load() == State::Unreadable || building.load()) return;
        auto client = ll::service::getClientInstance();
        auto* player = client ? client->getLocalPlayer() : nullptr;
        if (!player) return;
        state = State::Reading;
        if (!snapshot.step(player->getLevel(), recipesPerCall)) return;
        if (snapshot.failed() || snapshot.source().recipes.empty()) {
            state = State::Unreadable;
        } else {
            // The index is built off the client thread: some ten milliseconds in one frame made the first screen
            // of a world stutter (L-378). The source is plain data; nothing of the game's goes with it.
            auto source = std::make_shared<Source>(snapshot.source());
            int const forWorld = generation.load();
            building = true;
            std::thread([source, forWorld] {
                try {
                    auto table = std::make_shared<Table>();
                    table->index = build(*source);
                    table->choice = choose(table->index);
                    // How many recipes came with a grid, and how many of those fit their cells (L-359: the view lays out by it).
                    int shaped = 0, fits = 0;
                    for (auto const& recipe : source->recipes) {
                        if (!recipe.width) continue;
                        ++shaped;
                        fits += recipe.width >= 1 && recipe.width <= 3 && recipe.height >= 1 && recipe.height <= 3
                            && static_cast<size_t>(recipe.width * recipe.height) == recipe.uses.size();
                    }
                    Runtime::instance().self().getLogger().info("Recipe table read: {} recipes, {} with a grid, {} of them fit their cells",
                        source->recipes.size(), shaped, fits);
                    std::lock_guard lock(mutex);
                    if (forWorld == generation.load()) built = std::move(table), state = State::Ready;
                } catch (...) {
                    if (forWorld == generation.load()) state = State::Unreadable;
                }
                building = false;
            }).detach();
        }
        snapshot.reset();
    } catch (...) { state = State::Unreadable; }
}
}
State want() {
    if (!versionSensitiveAllowed(capabilities::Id::RecipeTable)) return State::Paused;
    advance();
    return state.load();
}
std::shared_ptr<Table const> table() {
    std::lock_guard lock(mutex);
    return built;
}
void start() {
    auto& bus = ll::event::EventBus::getInstance();
    joinListener = bus.emplaceListener<ll::event::ClientStartJoinLevelEvent>([](auto&) { reset(); });
    exitListener = bus.emplaceListener<ll::event::ClientExitLevelEvent>([](auto&) { reset(); });
}
void stop() {
    for (auto* listener : {&joinListener, &exitListener})
        if (*listener) { ll::event::EventBus::getInstance().removeListener(*listener); listener->reset(); }
    reset();
}
}
