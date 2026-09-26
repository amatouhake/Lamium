#include "features/interaction/PermanentSneak.h"
#include "features/interaction/AutomationInput.h"
#include "features/camera/Zoom.h"
#include "app/Runtime.h"
#include "input/Actions.h"
#include "ui/SettingsScreen.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/service/TargetedBedrock.h"
#include "mc/client/entity/systems/ClientInputUpdateSystem.h"
#include "mc/client/input/ClientMoveInputHandler.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/client/renderer/game/LevelRendererPlayer.h"
#include "mc/entity/components/MoveInputComponent.h"
#include "mc/entity/components/RawMoveInputComponent.h"
#include <stdexcept>

namespace lamium::interaction::sneak {
namespace {
AutomationInput intent;
AutomationInput sprintIntent;
bool installed = false;
bool dimensionInstalled = false;
unsigned observed = 0, matched = 0, rawSneak = 0;
bool eligible(IClientInstance& client) {
    auto* player = client.getLocalPlayer();
    return Runtime::instance().enabled() && !ui::ownsInput()
        && gameplayScreen(client.getScreenName()) && player && player->isAlive()
        && !player->isSleeping() && !player->getVehicle()
        && !Zoom::instance().blocksLookInteraction(*player);
}
LL_STATIC_HOOK(ExtractSneakInput, ll::memory::HookPriority::Normal,
    &ClientInputUpdateSystem::extractRawHIDInput, void,
    MovementAbilitiesComponent const& abilities, MoveInputComponent const& input,
    ActorDataFlagComponent const& flags, RawMoveInputComponent& raw,
    Optional<SneakingComponent const> sneaking, Optional<WasInWaterFlagComponent const> water) {
    auto client = ll::service::getClientInstance();
    if (intent.active() && observed < 1000) ++observed;
    // Sneak and Sprint survive menus and focus changes; they only pause while
    // ineligible and end on death (dimension change and world exit cancel
    // them elsewhere).
    auto* player = client ? client->getLocalPlayer() : nullptr;
    if (player && !player->isAlive()) { intent.cancel(); sprintIntent.cancel(); }
    bool ready = client && eligible(*client);
    bool sneak = intent.active() && ready;
    bool sprint = sprintIntent.active() && ready;
    if ((!sneak && !sprint) || !client
        || ClientMoveInputHandler::getMoveInput(*client) != &input) {
        origin(abilities, input, flags, raw, sneaking, water);
        return;
    }
    // Feed vanilla a transient copy. Never leave synthetic bits in the user's
    // stored HID state, so cancelling cannot clear a physically held key.
    auto augmented = input;
    if (sneak) {
        if (matched < 1000) ++matched;
        augmented.mRawInputState->mFlagValues->set(static_cast<size_t>(MoveInputState::Flag::SneakDown));
    }
    // Vanilla still decides whether sprinting starts (forward input, food, blindness).
    if (sprint)
        augmented.mRawInputState->mFlagValues->set(static_cast<size_t>(MoveInputState::Flag::SprintDown));
    origin(abilities, augmented, flags, raw, sneaking, water);
    if (rawSneak < 1000 && raw.mRawInput->mFlagValues->test(static_cast<size_t>(MoveInputState::Flag::SneakDown))) ++rawSneak;
}
LL_TYPE_INSTANCE_HOOK(SneakDimensionChange, ll::memory::HookPriority::Normal, LevelRendererPlayer,
    &LevelRendererPlayer::$onWillChangeDimension, void, Player& player) {
    intent.cancel();
    sprintIntent.cancel();
    origin(player);
}
}
void cancel() {
    if (intent.active()) Runtime::instance().self().getLogger().info(
        "Permanent Sneak stopped: extractionCalls={} localMatches={} rawSneakSamples={}", observed, matched, rawSneak);
    intent.cancel();
}
void toggle(IClientInstance&) {
    if (intent.active()) cancel();
    else { observed = matched = rawSneak = 0; intent.arm(); }
    Runtime::instance().self().getLogger().info("Permanent Sneak: {}", intent.active() ? "on" : "off");
}
bool active(IClientInstance& client) { return intent.active() && eligible(client); }
bool armed() { return intent.active(); }
void start() {
    try {
        if (!installed) {
            if (ExtractSneakInput::hook(true) != 0) throw std::runtime_error("Could not install sneak input hook");
            installed = true;
        }
        if (!dimensionInstalled) {
            if (SneakDimensionChange::hook(true) != 0) throw std::runtime_error("Could not install sneak dimension hook");
            dimensionInstalled = true;
        }
    } catch (...) { stop(); throw; }
}
void stop() {
    cancel();
    sprint::cancel();
    if (dimensionInstalled && SneakDimensionChange::unhook(true)) dimensionInstalled = false;
    if (installed && ExtractSneakInput::unhook(true)) installed = false;
}
}
namespace lamium::interaction::sprint {
void cancel() {
    if (sneak::sprintIntent.active()) Runtime::instance().self().getLogger().info("Permanent Sprint stopped");
    sneak::sprintIntent.cancel();
}
void toggle(IClientInstance&) {
    if (sneak::sprintIntent.active()) cancel();
    else sneak::sprintIntent.arm();
    Runtime::instance().self().getLogger().info("Permanent Sprint: {}", sneak::sprintIntent.active() ? "on" : "off");
}
bool active(IClientInstance& client) { return sneak::sprintIntent.active() && sneak::eligible(client); }
bool armed() { return sneak::sprintIntent.active(); }
}
