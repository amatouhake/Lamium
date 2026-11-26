#include "features/interaction/SneakPlacement.h"
#include "features/interaction/SneakPlacementPlan.h"
#include "features/interaction/PeriodicInput.h"
#include "app/Runtime.h"
#include "app/Versions.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/client/ClientExitLevelEvent.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/service/TargetedBedrock.h"
#include "ll/api/thread/ClientThreadExecutor.h"
#include "mc/client/entity/systems/ClientInputUpdateSystem.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/input/ClientMoveInputHandler.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/deps/vanilla_components/ActorDataFlagComponent.h"
#include "mc/world/actor/ActorFlags.h"
#include "mc/entity/components/MoveInputComponent.h"
#include "mc/entity/components/RawMoveInputComponent.h"
#include "mc/entity/components/SneakingComponent.h"
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
#include <array>
#include <format>
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
ll::event::ListenerPtr exitListener;
// A new world starts from vanilla's sneak, not from the last packet of the old one.
void reset() {
    pulse = {};
    serverSneak = edited = false;
    heldAt.reset();
}
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
// Send only (L-410): the server decides the use with the sneak state it was sent, the client's own use call
// with the local player's flag, which that method leaves alone. A block the client toggles itself (a door)
// then opened here and stayed shut there. For the length of that one call the flag reads as the state sent
// (and the move input's, InputAsSent); pose and speed are not touched. Left alone unless the bit is the one the game
// reads back.
class AsSent {
    ActorDataFlagComponent* flags = nullptr;
    bool was = false;
    static constexpr size_t bit = static_cast<size_t>(ActorFlags::Sneaking);

public:
    explicit AsSent(Player& actor) noexcept {
        try {
            auto* player = localPlayer();
            if (!player || static_cast<Player*>(player) != &actor || !edited || !packetMode() || !on()) return;
            auto found = actor.getEntityContext().tryGetComponent<ActorDataFlagComponent>();
            if (!found) return;
            bool const local = found->mValue.test(bit);
            if (local != actor.isSneaking() || local == serverSneak) return;
            found->mValue.set(bit, serverSneak);
            if (actor.isSneaking() != serverSneak) {
                found->mValue.set(bit, local);
                return;
            }
            flags = &*found, was = local;
            static int logged = 0;
            if (++logged <= 6)
                Runtime::instance().self().getLogger().info("Sneak placement: the client's use runs with sneak {} as sent", serverSneak);
        } catch (...) {}
    }
    ~AsSent() {
        if (flags) flags->mValue.set(bit, was);
    }
    bool applied() const { return flags != nullptr; }
    AsSent(AsSent const&) = delete;
    AsSent& operator=(AsSent const&) = delete;
};
// With AsSent: the move input's sneak flags read as the state sent for the call too. The flag alone left the door
// opening (43930ea5); with these the client's use leaves it shut as the server does (cf793f85, 2026-11-27).
// Written and put back inside the call, between the ticks that read them for movement.
class InputAsSent {
    MoveInputComponent* input = nullptr;
    bool sneaking = false, down = false;

public:
    explicit InputAsSent(bool apply) noexcept {
        try {
            auto client = ll::service::getClientInstance();
            if (!apply || !client) return;
            auto* found = ClientMoveInputHandler::getMoveInput(*client);
            if (!found) return;
            sneaking = found->mFlagValues->test(static_cast<size_t>(MoveInputComponent::Flag::Sneaking));
            down = found->mInputState->mFlagValues->test(static_cast<size_t>(MoveInputState::Flag::SneakDown));
            found->mFlagValues->set(static_cast<size_t>(MoveInputComponent::Flag::Sneaking), serverSneak);
            found->mInputState->mFlagValues->set(static_cast<size_t>(MoveInputState::Flag::SneakDown), serverSneak);
            input = found;
        } catch (...) {}
    }
    ~InputAsSent() {
        if (!input) return;
        input->mFlagValues->set(static_cast<size_t>(MoveInputComponent::Flag::Sneaking), sneaking);
        input->mInputState->mFlagValues->set(static_cast<size_t>(MoveInputState::Flag::SneakDown), down);
    }
};
#ifdef LAMIUM_SNEAK_TRACE
// Research builds (L-410): what a use call saw of sneaking, and whether it changed the block it aimed at.
int traced = 0;
std::string sneakSeen(Player& actor) {
    std::string out = std::format("flag {} sent {} edited {} method {} wanted {}", actor.isSneaking(), serverSneak, edited,
                                  packetMode() ? "send" : "crouch", pulse.wanted ? (*pulse.wanted ? "1" : "0") : "-");
    try {
        out += std::format(" component {}", static_cast<bool>(actor.getEntityContext().tryGetComponent<SneakingComponent>()));
        auto client = ll::service::getClientInstance();
        if (auto const* input = client ? ClientMoveInputHandler::getMoveInput(*client) : nullptr)
            out += std::format(" input sneaking {} down {} raw {}", input->mFlagValues->test(static_cast<size_t>(MoveInputComponent::Flag::Sneaking)),
                               input->mInputState->mFlagValues->test(static_cast<size_t>(MoveInputState::Flag::SneakDown)),
                               input->mRawInputState->mFlagValues->test(static_cast<size_t>(MoveInputState::Flag::SneakDown)));
    } catch (...) { out += " (read failed)"; }
    return out;
}
// The block aimed at with the ones under and over it: a door keeps its open state in one half.
std::array<Block const*, 3> column(Player& actor, BlockPos const& pos) {
    auto& region = actor.getDimensionBlockSource();
    return {&region.getBlock({pos.x, pos.y - 1, pos.z}), &region.getBlock(pos), &region.getBlock({pos.x, pos.y + 1, pos.z})};
}
void trace(std::string const& text) {
    if (++traced > 120) return;
    try { Runtime::instance().self().getLogger().info("Sneak placement trace: {}", text); } catch (...) {}
}
#endif
// One use call of the client: held back, or run with the sneak state sent.
template <class Call> InteractionResult useOn(char const* mode, Player& actor, ItemStack& item, BlockPos const& pos, uchar face, Call&& call) {
    if (holds(actor, item, pos, face)) {
#ifdef LAMIUM_SNEAK_TRACE
        trace(std::format("{} useItemOn ({},{},{}) held; {}", mode, pos.x, pos.y, pos.z, sneakSeen(actor)));
#endif
        return InteractionResult{false, false};
    }
    AsSent const sneak(actor);
    InputAsSent const input(sneak.applied());
#ifdef LAMIUM_SNEAK_TRACE
    std::array<Block const*, 3> before{};
    std::string seen;
    try {
        before = column(actor, pos);
        seen = sneakSeen(actor);
    } catch (...) {}
    InteractionResult const result = call();
    try {
        trace(std::format("{} useItemOn ({},{},{}) {} {} item {} flag written {}; {}; result success {} swing {}; column {}", mode, pos.x, pos.y, pos.z,
                          before[1] ? before[1]->getBlockType().mNameInfo->mFullName->getString() : "?",
                          static_cast<Player*>(localPlayer()) == &actor ? "client" : "server", item.isNull() ? "none" : "held", sneak.applied(), seen,
                          static_cast<bool>(result.mSuccess), static_cast<bool>(result.mSwing), column(actor, pos) == before ? "unchanged" : "CHANGED"));
    } catch (...) {}
    return result;
#else
    (void)mode;
    return call();
#endif
}
// Research builds: a build call let through, and whether the column changed during it.
template <class Call> bool buildTraced(char const* mode, Player& actor, BlockPos const& pos, Call&& call) {
#ifdef LAMIUM_SNEAK_TRACE
    std::array<Block const*, 3> before{};
    bool interactive = false;
    try {
        before = column(actor, pos);
        interactive = before[1]->getBlockType().isInteractiveBlock();
    } catch (...) {}
    bool const result = call();
    try {
        if (interactive)
            trace(std::format("{} buildBlock ({},{},{}) {} result {}; column {}", mode, pos.x, pos.y, pos.z,
                              static_cast<Player*>(localPlayer()) == &actor ? "client" : "server", result,
                              column(actor, pos) == before ? "unchanged" : "CHANGED"));
    } catch (...) {}
    return result;
#else
    (void)mode, (void)actor, (void)pos;
    return call();
#endif
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
    return buildTraced(#Mode, mPlayer, pos, [&] { return origin(pos, face, hand, simTick); }); \
} \
LL_TYPE_INSTANCE_HOOK(SneakPlacement##Mode##StartBuild, ll::memory::HookPriority::Normal, Mode, \
    &Mode::$startBuildBlock, void, BlockPos const& pos, uchar face, HandSlot hand) { \
    if (!holds(mPlayer, heldIn(mPlayer, hand), pos, face)) origin(pos, face, hand); \
} \
LL_TYPE_INSTANCE_HOOK(SneakPlacement##Mode##UseOn, ll::memory::HookPriority::Normal, Mode, \
    &Mode::$useItemOn, InteractionResult, ItemStack& item, BlockPos const& pos, uchar face, \
    Vec3 const& hit, HandSlot hand, Block const* target, bool first) { \
    return useOn(#Mode, mPlayer, item, pos, face, [&] { return origin(item, pos, face, hit, hand, target, first); }); \
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
        exitListener = ll::event::EventBus::getInstance().emplaceListener<ll::event::ClientExitLevelEvent>([](auto&) { reset(); });
        capabilities::ready(capabilities::Id::SneakPlacement);
    } catch (...) { capabilities::hooks(capabilities::Id::SneakPlacement, false); stop(); throw; }
}
void stop() {
    capabilities::stop(capabilities::Id::SneakPlacement);
    if (exitListener) ll::event::EventBus::getInstance().removeListener(exitListener);
    exitListener.reset();
    reset();
    for (auto it = std::rbegin(hooks); it != std::rend(hooks); ++it)
        if (it->installed && it->remove(true)) it->installed = false;
    installed = false;
}
}
