#include "features/interaction/PeriodicInput.h"
#include "features/camera/Zoom.h"
#include "app/Runtime.h"
#include "input/Actions.h"
#include "ui/SettingsScreen.h"
#include "ui/Localization.h"
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

namespace lamium::interaction {
std::string autoModeText(AutoMode mode, bool heldOnly) {
    if (mode == AutoMode::Fast && heldOnly) return ui::translated("autoFastHeld");
    return ui::translated(autoModeLabels[static_cast<size_t>(mode)]);
}
}
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
void emit(Button& button, bool down, IClientInstance& client) {
    auto& count = down ? button.presses : button.releases;
    if (count < 1000) ++count;
#ifdef LAMIUM_AUTOMATION_TRACE
    if (count <= 8) Runtime::instance().self().getLogger().info(
        "Auto input edge: down={} count={}", down, count);
#endif
    // Copy callbacks in case a callback changes screen ownership.
    // Registration/destruction cannot invalidate this iteration.
    auto callbacks = down ? button.down : button.up;
    for (auto const& callback : callbacks) callback(FocusImpact::DeactivateFocus, client);
}
// Stops acting until gameplay input returns and delivers the release Lamium
// still owes, if the client that received the press is still current.
void suspend(Button& button) {
    if (!button.click.suspend() || !button.client) return;
    auto current = ll::service::getClientInstance();
    if (current && &current.get() == button.client) emit(button, false, *button.client);
}
Callback capture(InputHandler* handler, std::string const& name, bool down, Callback callback) {
    auto index = actionIndex(name);
    if (index < 0 || !callback) return callback;
    auto& owner = owners[handler];
    if (!owner) owner = std::make_shared<Owner>();
    auto& button = owner->buttons[index];
    // A changing registration invalidates synthetic state before adding callbacks.
    button.click.forgetHeld();
    suspend(button);
    (down ? button.down : button.up).push_back(callback);
    return [weak = std::weak_ptr<Owner>(owner), index, down, callback = std::move(callback)]
        (FocusImpact focus, IClientInstance& client) {
        // The user's own press takes priority while held; automation resumes
        // after release. Fast click turns the held press into bursts.
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
        if (!click.on() && !click.releaseOwed()) continue;
        bool owned = current && &current.get() == &primary && button.client == &primary && eligible(primary)
            && primary.getInput() && &primary.getInput()->mInputHandler == this;
        if (!owned) {
#ifdef LAMIUM_AUTOMATION_TRACE
            if (!click.isSuspended()) Runtime::instance().self().getLogger().info(
                "Auto input paused: primary={} armedClient={} eligible={}",
                current && &current.get() == &primary, button.client == &primary, eligible(primary));
#endif
            suspend(button);
            continue;
        }
        for (auto edge : click.update()) emit(button, edge == InputEdge::Press, primary);
    }
}
LL_TYPE_INSTANCE_HOOK(ChangeDimension, ll::memory::HookPriority::Normal, LevelRendererPlayer,
    &LevelRendererPlayer::$onWillChangeDimension, void, Player& player) {
    interrupt();
    origin(player);
}
struct Hook { int (*install)(bool); bool (*remove)(bool); bool installed = false; };
Hook hooks[] = {{RegisterDown::hook, RegisterDown::unhook}, {RegisterUp::hook, RegisterUp::unhook},
    {DestroyOwner::hook, DestroyOwner::unhook}, {Update::hook, Update::unhook},
    {ChangeDimension::hook, ChangeDimension::unhook}};
// Follow the settings once per client tick, then advance timing.
void followSettings() {
    auto preferences = Runtime::instance().preferences();
    preferences.normalize();
    auto const& value = preferences.interaction;
    auto current = ll::service::getClientInstance();
    IClientInstance* client = current ? &current.get() : nullptr;
    for (auto const& [_, owner] : owners)
        for (size_t i = 0; i < owner->buttons.size(); ++i) {
            auto& button = owner->buttons[i];
            bool attack = i == 0;
            bool on = enabled && (attack ? value.autoAttack : value.autoUse);
            if (on && button.client != client) { suspend(button); button.client = client; }
            button.click.configure(on, attack ? value.attackMode : value.useMode,
                static_cast<int>(attack ? value.attackTicks : value.useTicks),
                static_cast<int>(attack ? value.attackClicks : value.useClicks),
                attack ? value.attackHeldOnly : value.useHeldOnly);
            button.click.tick();
        }
}
}
void interrupt() {
    for (auto const& [_, owner] : owners)
        for (auto& button : owner->buttons) { button.click.forgetHeld(); suspend(button); }
}
void endSession() {
    interrupt();
    auto& runtime = Runtime::instance();
    auto value = runtime.preferences();
    if (!value.interaction.autoAttack && !value.interaction.autoUse) return;
    value.interaction.autoAttack = value.interaction.autoUse = false;
    if (!runtime.save(value)) runtime.self().getLogger().error("Could not switch Auto Attack/Use off");
}
bool paused(IClientInstance& client) { return !eligible(client); }
void start() {
    try {
        for (auto& hook : hooks) if (!hook.installed) {
            if (hook.install(true) != 0) throw std::runtime_error("Could not install periodic input hook");
            hook.installed = true;
        }
        tickListener = ll::event::EventBus::getInstance().emplaceListener<ll::event::ClientLevelTickEvent>(
            [](auto&) { followSettings(); });
        if (!tickListener) throw std::runtime_error("Could not subscribe to client ticks");
        enabled = true;
    } catch (...) { stop(); throw; }
}
void stop() {
    enabled = false;
    interrupt();
    if (tickListener) ll::event::EventBus::getInstance().removeListener(tickListener);
    tickListener.reset();
    owners.clear();
    for (auto it = std::rbegin(hooks); it != std::rend(hooks); ++it)
        if (it->installed && it->remove(true)) it->installed = false;
}
}
