#include "features/recipes/RecipeService.h"
#include "app/Runtime.h"
#include "app/Versions.h"
#include "features/recipes/RecipeSnapshot.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/client/ClientExitLevelEvent.h"
#include "ll/api/event/client/ClientStartJoinLevelEvent.h"
#include "ll/api/event/world/ClientLevelTickEvent.h"
#include "ll/api/service/TargetedBedrock.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include <atomic>
#include <mutex>

namespace lamium::recipes::service {
namespace {
constexpr int recipesPerTick = 600; // about a millisecond; a world has four to five thousand
ll::event::ListenerPtr tickListener, joinListener, exitListener;
std::mutex mutex;
std::shared_ptr<Table const> built;
std::atomic<bool> wanted{false};
std::atomic<State> state{State::Idle};
game::Snapshot snapshot; // the client tick's alone
void reset() {
    {
        std::lock_guard lock(mutex);
        built.reset();
    }
    wanted = false;
    state = State::Idle;
    snapshot.reset();
}
void tick() noexcept {
    try {
        if (!wanted.load() || state.load() == State::Ready || state.load() == State::Unreadable) return;
        if (!versionSensitiveAllowed(capabilities::Id::RecipeTable)) { state = State::Paused; return; }
        auto client = ll::service::getClientInstance();
        auto* player = client ? client->getLocalPlayer() : nullptr;
        if (!player) return;
        state = State::Reading;
        if (!snapshot.step(player->getLevel(), recipesPerTick)) return;
        if (snapshot.failed() || snapshot.source().recipes.empty()) {
            state = State::Unreadable;
        } else {
            auto table = std::make_shared<Table>();
            table->index = build(snapshot.source());
            table->choice = choose(table->index);
            std::lock_guard lock(mutex);
            built = std::move(table);
            state = State::Ready;
        }
        snapshot.reset();
    } catch (...) { state = State::Unreadable; }
}
}
State want() {
    if (!versionSensitiveAllowed(capabilities::Id::RecipeTable)) return State::Paused;
    wanted = true;
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
    tickListener = bus.emplaceListener<ll::event::ClientLevelTickEvent>([](auto&) { tick(); });
}
void stop() {
    for (auto* listener : {&tickListener, &joinListener, &exitListener})
        if (*listener) { ll::event::EventBus::getInstance().removeListener(*listener); listener->reset(); }
    reset();
}
}
