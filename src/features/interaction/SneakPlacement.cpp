#include "features/interaction/SneakPlacement.h"
#include "features/interaction/SneakPlacementPlan.h"
#include "features/interaction/PeriodicInput.h"
#include "app/Runtime.h"
#include "app/Versions.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/service/TargetedBedrock.h"
#include "ll/api/thread/ClientThreadExecutor.h"
#include "mc/client/entity/systems/ClientInputUpdateSystem.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/input/ClientMoveInputHandler.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/entity/components/MoveInputComponent.h"
#include "mc/entity/components/RawMoveInputComponent.h"
#include "mc/input/MoveInputState.h"
#include "mc/network/LoopbackPacketSender.h"
#include "mc/network/MinecraftPacketIds.h"
#include "mc/network/packet/PlayerAuthInputPacket.h"
#include "mc/world/gamemode/GameMode.h"
#include "mc/world/gamemode/InteractionResult.h"
#include "mc/world/gamemode/SurvivalMode.h"
#include "mc/world/item/HandSlot.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/actor/player/Inventory.h"
#include "mc/world/actor/player/PlayerInventory.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/BlockType.h"
#include "mc/world/phys/HitResult.h"
#include <stdexcept>

namespace lamium::interaction::sneakPlacement {
namespace {
// Client thread only: the build calls, the input extraction and the auth
// input send all run there.
Pulse pulse;
bool serverSneak = false; // Sneaking in the last auth input sent.
bool edited = false;      // Lamium changed the sneak bits of that packet.
bool installed = false;
// What a held-back use aimed at and the hotbar slot it used. The replay is a
// fresh use press, which goes wherever the crosshair is then, with the slot
// selected then; Fake Offhand's slot is selected only during its own calls.
struct HeldUse { BlockPos pos; uchar face; int slot; };
std::optional<HeldUse> heldAt;
using Auth = PlayerAuthInputPacketPayload::InputData;

bool on() {
    auto& runtime = Runtime::instance();
    return runtime.enabled() && runtime.snapshot()->interaction.sneakPlacement
        && versionSensitiveAllowed(capabilities::Id::SneakPlacement);
}
bool packetMode() { return Runtime::instance().snapshot()->interaction.sneakPlacementPacket; }
LocalPlayer* localPlayer() {
    auto client = ll::service::getClientInstance();
    return client ? client->getLocalPlayer() : nullptr;
}
ItemStack const& heldIn(Player& actor, HandSlot hand) {
    return hand == HandSlot::Offhand ? actor.getOffhandSlot() : actor.getSelectedItem();
}
// Whether a build call on this block must wait for the sneak state first.
bool holds(Player& actor, ItemStack const& item, BlockPos const& pos, uchar face) noexcept {
    try {
        auto* player = localPlayer();
        if (!player || static_cast<Player*>(player) != &actor) return false;
        auto& runtime = Runtime::instance();
        bool enabled = on();
        if (!enabled && !pulse.wanted) return false;
        bool interactive = player->getDimensionBlockSource().getBlock(pos).getBlockType().isInteractiveBlock();
        bool held = gate(pulse, enabled, runtime.snapshot()->interaction.sneakPlacementInvert, player->isSneaking(),
            interactive, item.isNull(), serverSneak) == Gate::Hold;
        if (held) {
            auto* inventory = player->mInventory.get();
            bool hotbar = inventory && inventory->mSelectedContainerId == ContainerID::Inventory;
            heldAt = HeldUse{pos, face, hotbar ? inventory->mSelected : -1};
        }
        return held;
    } catch (...) { return false; }
}
void set(PlayerAuthInputPacket& auth, Auth flag, bool value) {
    auth.mInputData->mContainer.set(static_cast<size_t>(flag), value);
}
void replay() {
    ll::thread::ClientThreadExecutor::getDefault().execute([at = heldAt] {
        try {
            auto client = ll::service::getClientInstance();
            if (!client || !client->getLocalPlayer() || !on() || !at) return;
            // Looking elsewhere by now: the use would land on another block.
            auto const& hit = client->getLatestHitResult();
            if (hit.mType != HitResultType::Tile || hit.mBlock != at->pos || hit.mFacing != at->face) return;
            auto* player = client->getLocalPlayer();
            auto* inventory = player->mInventory.get();
            if (!inventory || inventory->mSelectedContainerId != ContainerID::Inventory || at->slot < 0 || at->slot >= 9)
                return;
            int selected = inventory->mSelected;
            // The held slot is selected for the replay alone, as Fake Offhand
            // does; an emptied slot would act with whatever is held now.
            if (selected != at->slot) {
                if (player->getInventory().getItem(at->slot).isNull()) return;
                if (!inventory->selectSlot(at->slot, ContainerID::Inventory)) return;
            }
            struct Restore {
                PlayerInventory& inventory;
                int slot, previous;
                ~Restore() {
                    if (slot != previous && inventory.mSelected == slot) inventory.selectSlot(previous, ContainerID::Inventory);
                }
            } restore{*inventory, at->slot, selected};
            if (periodic::sendUseEdge(*client, true)) periodic::sendUseEdge(*client, false);
        } catch (...) {}
    });
}

LL_STATIC_HOOK(SneakPlacementInput, ll::memory::HookPriority::Low,
    &ClientInputUpdateSystem::extractRawHIDInput, void,
    MovementAbilitiesComponent const& abilities, MoveInputComponent const& input,
    ActorDataFlagComponent const& flags, RawMoveInputComponent& raw,
    Optional<SneakingComponent const> sneaking, Optional<WasInWaterFlagComponent const> water) {
    auto client = ll::service::getClientInstance();
    if (!pulse.wanted || packetMode() || !client || ClientMoveInputHandler::getMoveInput(*client) != &input) {
        origin(abilities, input, flags, raw, sneaking, water);
        return;
    }
    // A transient copy, as Permanent Sneak does: the stored device state keeps
    // what the player really holds.
    auto augmented = input;
    augmented.mRawInputState->mFlagValues->set(static_cast<size_t>(MoveInputState::Flag::SneakDown), *pulse.wanted);
    origin(abilities, augmented, flags, raw, sneaking, water);
}
LL_TYPE_INSTANCE_HOOK(SneakPlacementSend, ll::memory::HookPriority::Normal, LoopbackPacketSender,
    &LoopbackPacketSender::$send, void, Packet& packet) {
    try {
        auto client = ll::service::getClientInstance();
        if (packet.getId() == MinecraftPacketIds::PlayerAuthInputPacket && client
            && packet.mSenderSubId == client->getClientSubId()) {
            auto& auth = static_cast<PlayerAuthInputPacket&>(packet);
            if (!client->getLocalPlayer() || (!on() && pulse.wanted)) pulse = {};
            bool vanilla = auth.mInputData->contains(Auth::Sneaking), sneaking = vanilla;
            bool sendOnly = packetMode() && versionSensitiveAllowed(capabilities::Id::SneakPlacement);
            if (sendOnly) releaseBeforeAuth(pulse);
            if (pulse.wanted && sendOnly) {
                // The server reads these bits before the use that follows.
                sneaking = *pulse.wanted;
                set(auth, Auth::Sneaking, sneaking);
                set(auth, Auth::SneakDown, sneaking);
                edited = true;
            }
            // Vanilla marks only its own transitions; after an edited packet
            // the server's state is restored with the matching one.
            if (edited && sneaking != serverSneak)
                set(auth, sneaking ? Auth::StartSneaking : Auth::StopSneaking, true);
            if (sneaking == vanilla && !pulse.wanted) edited = false;
            serverSneak = sneaking;
            switch (afterAuth(pulse, serverSneak)) {
            case Step::Replay: replay(); break;
            case Step::GiveUp:
                Runtime::instance().self().getLogger().warn("Sneak placement: the sneak state was not sent; vanilla acts for a moment");
                break;
            default: break;
            }
        }
    } catch (...) {}
    origin(packet);
}
#define GATE_HOOKS(Mode) \
LL_TYPE_INSTANCE_HOOK(SneakPlacement##Mode##Build, ll::memory::HookPriority::Normal, Mode, \
    &Mode::$buildBlock, bool, BlockPos const& pos, uchar face, HandSlot hand, bool simTick) { \
    if (holds(mPlayer, heldIn(mPlayer, hand), pos, face)) return false; \
    return origin(pos, face, hand, simTick); \
} \
LL_TYPE_INSTANCE_HOOK(SneakPlacement##Mode##StartBuild, ll::memory::HookPriority::Normal, Mode, \
    &Mode::$startBuildBlock, void, BlockPos const& pos, uchar face, HandSlot hand) { \
    if (!holds(mPlayer, heldIn(mPlayer, hand), pos, face)) origin(pos, face, hand); \
} \
LL_TYPE_INSTANCE_HOOK(SneakPlacement##Mode##UseOn, ll::memory::HookPriority::Normal, Mode, \
    &Mode::$useItemOn, InteractionResult, ItemStack& item, BlockPos const& pos, uchar face, \
    Vec3 const& hit, HandSlot hand, Block const* target, bool first) { \
    if (holds(mPlayer, item, pos, face)) return InteractionResult{false, false}; \
    return origin(item, pos, face, hit, hand, target, first); \
}
GATE_HOOKS(GameMode)
GATE_HOOKS(SurvivalMode)
#undef GATE_HOOKS

struct Hook { int (*install)(bool); bool (*remove)(bool); bool installed = false; };
Hook hooks[] = {
    {SneakPlacementInput::hook, SneakPlacementInput::unhook},
    {SneakPlacementSend::hook, SneakPlacementSend::unhook},
    {SneakPlacementGameModeBuild::hook, SneakPlacementGameModeBuild::unhook},
    {SneakPlacementGameModeStartBuild::hook, SneakPlacementGameModeStartBuild::unhook},
    {SneakPlacementGameModeUseOn::hook, SneakPlacementGameModeUseOn::unhook},
    {SneakPlacementSurvivalModeBuild::hook, SneakPlacementSurvivalModeBuild::unhook},
    {SneakPlacementSurvivalModeStartBuild::hook, SneakPlacementSurvivalModeStartBuild::unhook},
    {SneakPlacementSurvivalModeUseOn::hook, SneakPlacementSurvivalModeUseOn::unhook},
};
}
std::optional<bool> sneakForUse(bool userSneaking) {
    if (!installed || !on()) return {};
    return wantedSneak(true, Runtime::instance().snapshot()->interaction.sneakPlacementInvert, userSneaking, true, false);
}
void start() {
    if (installed) return;
    capabilities::begin(capabilities::Id::SneakPlacement);
    try {
        for (auto& hook : hooks) if (!hook.installed) {
            if (hook.install(true) != 0) throw std::runtime_error("Could not install sneak placement hooks");
            hook.installed = true;
        }
        installed = true;
        capabilities::ready(capabilities::Id::SneakPlacement);
    } catch (...) { capabilities::hooks(capabilities::Id::SneakPlacement, false); stop(); throw; }
}
void stop() {
    capabilities::stop(capabilities::Id::SneakPlacement);
    pulse = {};
    for (auto it = std::rbegin(hooks); it != std::rend(hooks); ++it)
        if (it->installed && it->remove(true)) it->installed = false;
    installed = false;
}
}
