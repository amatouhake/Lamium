#include "features/interaction/PeriodicInput.h"
#include "features/interaction/AutomationInput.h"
#include "features/camera/Zoom.h"
#include "app/Runtime.h"
#include "input/Actions.h"
#include "ui/SettingsScreen.h"
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
    AutomationInput intent;
    std::vector<Callback> down, up;
    IClientInstance* client = nullptr;
    bool physical = false, synthetic = false;
    unsigned presses = 0, releases = 0;
    AutomationInput::Duration interval = std::chrono::milliseconds(500);
};
struct Owner { std::array<Button, 2> buttons; };
std::map<InputHandler*, std::shared_ptr<Owner>> owners;
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
    if (count <= 4) Runtime::instance().self().getLogger().info(
        "Periodic input edge: down={} count={}", down, count);
#endif
    // Copy callbacks in case a callback changes screen ownership and cancels
    // intent. Registration/destruction cannot invalidate this iteration.
    auto callbacks = down ? button.down : button.up;
    for (auto const& callback : callbacks) callback(FocusImpact::DeactivateFocus, client);
}
void cancelButton(Button& button) {
#ifdef LAMIUM_AUTOMATION_TRACE
    bool wasActive = button.intent.active();
#endif
    button.intent.cancel();
    bool release = std::exchange(button.synthetic, false) && !button.physical;
    auto* client = std::exchange(button.client, nullptr);
    auto current = ll::service::getClientInstance();
    bool finalRelease = release && current && &current.get() == client;
    if (finalRelease) emit(button, false, *client);
#ifdef LAMIUM_AUTOMATION_TRACE
    if (wasActive) Runtime::instance().self().getLogger().info(
        "Periodic input stopped: presses={} releases={} physical={} finalRelease={} releaseSkipped={}",
        button.presses, button.releases, button.physical, finalRelease, release && !finalRelease);
#endif
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
        if (auto owner = weak.lock()) {
            auto& button = owner->buttons[index];
            // Hand input takes over and disarms automation. Its down callback
            // now owns the held state, so do not inject an up underneath it.
            button.physical = down;
            if (down) {
                button.intent.cancel();
                button.synthetic = false;
                button.client = nullptr;
            }
        }
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
LL_TYPE_INSTANCE_HOOK(Update, ll::memory::HookPriority::Normal, InputHandler,
    &InputHandler::tick, void, IMinecraftGame* game, IClientInstance& primary,
    Bedrock::NotNullNonOwnerPtr<ControllerIDtoClientMap> const& map, bool multiple) {
    origin(game, primary, map, multiple);
    auto found = owners.find(this);
    if (found == owners.end()) return;
    auto owner = found->second;
    auto current = ll::service::getClientInstance();
    for (auto& button : owner->buttons) {
        if (!button.intent.active() && !button.synthetic) continue;
        if (!current || &current.get() != &primary || button.client != &primary
            || !eligible(primary) || !primary.getInput()
            || &primary.getInput()->mInputHandler != this) {
#ifdef LAMIUM_AUTOMATION_TRACE
            Runtime::instance().self().getLogger().info(
                "Periodic input rejected update: primary={} armedClient={} eligible={} owner={}",
                current && &current.get() == &primary, button.client == &primary, eligible(primary),
                primary.getInput() && &primary.getInput()->mInputHandler == this);
#endif
            cancelButton(button);
            continue;
        }
        auto edge = button.intent.update(AutomationInput::Clock::now(), true,
            button.physical, false, button.interval);
        if (edge == InputEdge::Press) {
            button.synthetic = true;
            emit(button, true, primary);
        } else if (edge == InputEdge::Release) {
            button.synthetic = false;
            if (!button.physical) emit(button, false, primary);
        }
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
}
void cancel() { for (auto const& [_, owner] : owners) for (auto& button : owner->buttons) cancelButton(button); }
bool active(IClientInstance& client, Action action) {
    if (!eligible(client) || !client.getInput()) return false;
    auto found = owners.find(&client.getInput()->mInputHandler);
    if (found == owners.end()) return false;
    auto const& button = found->second->buttons[static_cast<size_t>(action)];
    return button.client == &client && button.intent.active();
}
void toggle(IClientInstance& client, Action action) {
    auto& logger = Runtime::instance().self().getLogger();
    if (!eligible(client) || !client.getInput()) {
        logger.info("Periodic input unavailable: gameplay input is not owned"); return;
    }
    auto found = owners.find(&client.getInput()->mInputHandler);
    if (found == owners.end()) {
        logger.info("Periodic input unavailable: no registered owner (captured owners={})", owners.size()); return;
    }
    auto& button = found->second->buttons[static_cast<size_t>(action)];
    if (button.intent.active()) { cancelButton(button); logger.info("Periodic input {}: off", static_cast<int>(action)); return; }
    if (button.down.empty() || button.up.empty() || button.physical) {
        logger.info("Periodic input unavailable: down={} up={} physical={}", button.down.size(), button.up.size(), button.physical);
        return;
    }
    button.presses = button.releases = 0;
    auto preferences = Runtime::instance().preferences();
    preferences.normalize();
    auto seconds = action == Action::Attack ? preferences.interaction.attackInterval : preferences.interaction.useInterval;
    button.interval = std::chrono::duration_cast<AutomationInput::Duration>(std::chrono::duration<float>(seconds));
    button.client = &client;
    button.intent.arm();
    logger.info("Periodic input {}: on", static_cast<int>(action));
}
void start() {
    try {
        for (auto& hook : hooks) if (!hook.installed) {
            if (hook.install(true) != 0) throw std::runtime_error("Could not install periodic input hook");
            hook.installed = true;
        }
        enabled = true;
    } catch (...) { stop(); throw; }
}
void stop() {
    enabled = false;
    cancel();
    owners.clear();
    for (auto it = std::rbegin(hooks); it != std::rend(hooks); ++it)
        if (it->installed && it->remove(true)) it->installed = false;
}
}
