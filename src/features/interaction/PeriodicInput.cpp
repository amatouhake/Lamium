#include "features/interaction/PeriodicInput.h"
#include "features/camera/Zoom.h"
#include "app/Runtime.h"
#include "input/Actions.h"
#include "ui/SettingsScreen.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/world/ClientLevelTickEvent.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/service/TargetedBedrock.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/input/ClientInputHandler.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/client/renderer/game/LevelRendererPlayer.h"
#include "mc/deps/input/InputHandler.h"
#include <array>
#include <map>
#include <memory>
#include <stdexcept>
#include <utility>

namespace lamium::interaction::periodic {
namespace {
using Callback = InputHandler::ButtonPressHandler;
struct Button {
    AutoClick click;
    std::vector<Callback> down, up;
    IClientInstance* client = nullptr;
    unsigned presses = 0, releases = 0;
};
struct Owner { std::array<Button, 2> buttons; };
std::map<InputHandler*, std::shared_ptr<Owner>> owners;
ll::event::ListenerPtr tickListener;
bool enabled = false;
int actionIndex(std::string_view name) {
    if (name == "button.destroy_or_attack") return 0;
    if (name == "button.build_or_interact") return 1;
    return -1;
}
bool eligible(IClientInstance& client) {
    auto* player = client.getLocalPlayer();
    return enabled && Runtime::instance().enabled() && !ui::ownsInput()
        && gameplayScreen(client.getScreenName()) && player && player->isAlive()
        && !player->isSleeping() && !player->getVehicle()
        && !Zoom::instance().blocksLookInteraction(*player);
}
int clicksPerTick(Settings const& value, size_t index) {
    return static_cast<int>(index == 0 ? value.interaction.attackClicks : value.interaction.useClicks);
}
void emit(Button& button, bool down, IClientInstance& client) {
    auto& count = down ? button.presses : button.releases;
    if (count < 1000) ++count;
#ifdef LAMIUM_AUTOMATION_TRACE
    if (count <= 8) Runtime::instance().self().getLogger().info(
        "Auto input edge: down={} count={}", down, count);
#endif
    // Copy callbacks in case a callback changes screen ownership and cancels
    // intent. Registration/destruction cannot invalidate this iteration.
    auto callbacks = down ? button.down : button.up;
    for (auto const& callback : callbacks) callback(FocusImpact::DeactivateFocus, client);
}
// Stops Periodic/Hold and delivers the release Lamium still owes. The held
// state is forgotten too: after focus loss the physical release may never
// reach us, and a stale "held" would keep Fast click bursting.
void cancelButton(Button& button) {
    button.click.stop();
    button.click.physical(false);
    if (button.click.releaseOwed()) {
        auto current = ll::service::getClientInstance();
        if (current && &current.get() == button.client) emit(button, false, *button.client);
        button.click.released();
    }
}
Callback capture(InputHandler* handler, std::string const& name, bool down, Callback callback) {
    auto index = actionIndex(name);
    if (index < 0 || !callback) return callback;
    auto& owner = owners[handler];
    if (!owner) owner = std::make_shared<Owner>();
    auto& button = owner->buttons[index];
    // A changing registration invalidates armed work before adding callbacks.
    cancelButton(button);
    (down ? button.down : button.up).push_back(callback);
    return [weak = std::weak_ptr<Owner>(owner), index, down, callback = std::move(callback)]
        (FocusImpact focus, IClientInstance& client) {
        // Hand input takes over Periodic/Hold and feeds Fast click.
        if (auto owner = weak.lock()) owner->buttons[index].click.physical(down);
        callback(focus, client);
    };
}
LL_TYPE_INSTANCE_HOOK(RegisterDown, ll::memory::HookPriority::Normal, InputHandler,
    &InputHandler::registerButtonDownHandler, void, std::string name, Callback callback, bool suspendable) {
    auto wrapped = capture(this, name, true, std::move(callback));
    origin(std::move(name), std::move(wrapped), suspendable);
}
LL_TYPE_INSTANCE_HOOK(RegisterUp, ll::memory::HookPriority::Normal, InputHandler,
    &InputHandler::registerButtonUpHandler, void, std::string name, Callback callback, bool suspendable) {
    auto wrapped = capture(this, name, false, std::move(callback));
    origin(std::move(name), std::move(wrapped), suspendable);
}
LL_TYPE_INSTANCE_HOOK(DestroyOwner, ll::memory::HookPriority::Normal, InputHandler,
    &InputHandler::$dtor, void) {
    // No replay during destruction. The owner and its native input state are
    // being destroyed together; weak observers become inert.
    owners.erase(this);
    origin();
}
// Edges go out on the native input update; timing comes from client ticks.
LL_TYPE_INSTANCE_HOOK(Update, ll::memory::HookPriority::Normal, InputHandler,
    &InputHandler::tick, void, IMinecraftGame* game, IClientInstance& primary,
    Bedrock::NotNullNonOwnerPtr<ControllerIDtoClientMap> const& map, bool multiple) {
    origin(game, primary, map, multiple);
    auto found = owners.find(this);
    if (found == owners.end()) return;
    auto owner = found->second;
    auto current = ll::service::getClientInstance();
    for (auto& button : owner->buttons) {
        auto& click = button.click;
        bool running = click.mode() != AutoMode::Off || click.releaseOwed();
        if (!running && !click.fast()) continue;
        bool owned = current && &current.get() == &primary && button.client == &primary && eligible(primary)
            && primary.getInput() && &primary.getInput()->mInputHandler == this;
        if (!owned) {
#ifdef LAMIUM_AUTOMATION_TRACE
            if (running) Runtime::instance().self().getLogger().info(
                "Auto input rejected update: primary={} armedClient={} eligible={}",
                current && &current.get() == &primary, button.client == &primary, eligible(primary));
#endif
            if (running) cancelButton(button);
            continue;
        }
        for (auto edge : click.update()) emit(button, edge == InputEdge::Press, primary);
    }
}
LL_TYPE_INSTANCE_HOOK(ChangeDimension, ll::memory::HookPriority::Normal, LevelRendererPlayer,
    &LevelRendererPlayer::$onWillChangeDimension, void, Player& player) {
    cancel();
    origin(player);
}
struct Hook { int (*install)(bool); bool (*remove)(bool); bool installed = false; };
Hook hooks[] = {{RegisterDown::hook, RegisterDown::unhook}, {RegisterUp::hook, RegisterUp::unhook},
    {DestroyOwner::hook, DestroyOwner::unhook}, {Update::hook, Update::unhook},
    {ChangeDimension::hook, ChangeDimension::unhook}};
Button* buttonFor(IClientInstance& client, Action action) {
    if (!client.getInput()) return nullptr;
    auto found = owners.find(&client.getInput()->mInputHandler);
    if (found == owners.end()) return nullptr;
    return &found->second->buttons[static_cast<size_t>(action)];
}
// The button a hotkey may drive, or nullptr with the reason logged.
Button* usable(IClientInstance& client, Action action) {
    auto& logger = Runtime::instance().self().getLogger();
    if (!eligible(client)) { logger.info("Auto input unavailable: gameplay input is not owned"); return nullptr; }
    auto* button = buttonFor(client, action);
    if (!button) { logger.info("Auto input unavailable: no registered owner (captured owners={})", owners.size()); return nullptr; }
    if (button->down.empty() || button->up.empty()) {
        logger.info("Auto input unavailable: down={} up={}", button->down.size(), button->up.size());
        return nullptr;
    }
    return button;
}
}
void cancel() {
    auto preferences = Runtime::instance().preferences();
    preferences.normalize();
    for (auto const& [_, owner] : owners)
        for (size_t i = 0; i < owner->buttons.size(); ++i) {
            auto& button = owner->buttons[i];
            cancelButton(button);
            // Screen changes include closing Settings: pick up a new rate.
            if (button.click.fast()) button.click.setFast(true, clicksPerTick(preferences, i));
        }
}
void endSession() {
    for (auto const& [_, owner] : owners)
        for (auto& button : owner->buttons) { cancelButton(button); button.click.setFast(false); }
}
AutoMode mode(IClientInstance& client, Action action) {
    auto* button = eligible(client) ? buttonFor(client, action) : nullptr;
    return button && button->client == &client ? button->click.mode() : AutoMode::Off;
}
bool fast(IClientInstance& client, Action action) {
    auto* button = buttonFor(client, action);
    return button && button->client == &client && button->click.fast();
}
void toggle(IClientInstance& client, Action action, AutoMode wanted) {
    auto* button = usable(client, action);
    if (!button) return;
    auto& logger = Runtime::instance().self().getLogger();
    if (button->client == &client && button->click.mode() == wanted) {
        cancelButton(*button);
        logger.info("Auto input {}: off", static_cast<int>(action));
        return;
    }
    if (button->click.physicallyHeld()) { logger.info("Auto input unavailable: the button is held"); return; }
    auto preferences = Runtime::instance().preferences();
    preferences.normalize();
    auto ticks = action == Action::Attack ? preferences.interaction.attackTicks : preferences.interaction.useTicks;
    // Another client's leftovers are released before this one takes over.
    if (button->client != &client) cancelButton(*button);
    button->presses = button->releases = 0;
    button->client = &client;
    button->click.start(wanted, static_cast<int>(ticks));
    logger.info("Auto input {}: mode {}", static_cast<int>(action), static_cast<int>(wanted));
}
void toggleFast(IClientInstance& client, Action action) {
    auto* button = usable(client, action);
    if (!button) return;
    auto preferences = Runtime::instance().preferences();
    preferences.normalize();
    bool on = !(button->client == &client && button->click.fast());
    if (button->client != &client) { cancelButton(*button); button->client = &client; }
    button->click.setFast(on, clicksPerTick(preferences, static_cast<size_t>(action)));
    Runtime::instance().self().getLogger().info("Auto input {}: fast click {}", static_cast<int>(action), on);
}
void start() {
    try {
        for (auto& hook : hooks) if (!hook.installed) {
            if (hook.install(true) != 0) throw std::runtime_error("Could not install periodic input hook");
            hook.installed = true;
        }
        tickListener = ll::event::EventBus::getInstance().emplaceListener<ll::event::ClientLevelTickEvent>([](auto&) {
            for (auto const& [_, owner] : owners)
                for (auto& button : owner->buttons) button.click.tick();
        });
        if (!tickListener) throw std::runtime_error("Could not subscribe to client ticks");
        enabled = true;
    } catch (...) { stop(); throw; }
}
void stop() {
    enabled = false;
    endSession();
    if (tickListener) ll::event::EventBus::getInstance().removeListener(tickListener);
    tickListener.reset();
    owners.clear();
    for (auto it = std::rbegin(hooks); it != std::rend(hooks); ++it)
        if (it->installed && it->remove(true)) it->installed = false;
}
}
