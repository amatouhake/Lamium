#include "features/interaction/EdgeGuard.h"
#include "features/interaction/EdgeGuardPlan.h"
#include "features/interaction/EdgeGuardHistory.h"
#include "features/interaction/EdgeGuardPacketPlan.h"
#include "app/Runtime.h"
#include "app/Versions.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/service/TargetedBedrock.h"
#include "ll/api/thread/ClientThreadExecutor.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/deps/ecs/gamerefs_entity/EntityContext.h"
#include "mc/deps/ecs/gamerefs_entity/EntityRegistry.h"
#include "mc/deps/ecs/strict/StrictEntityContext.h"
#include "mc/deps/vanilla_components/AABBShapeComponent.h"
#include "mc/deps/vanilla_components/ActorDataFlagComponent.h"
#include "mc/deps/vanilla_components/IConstBlockSource.h"
#include "mc/deps/vanilla_components/MoveRequestComponent.h"
#include "mc/entity/components/MoveInputComponent.h"
#include "mc/entity/components/RawMoveInputComponent.h"
#include "mc/entity/systems/move_collision_system/MoveCollisionSystem.h"
#include "mc/input/MoveInputState.h"
#include "mc/world/actor/provider/PlayerMovement.h"
#include "mc/world/gamemode/GameMode.h"
#include "mc/world/gamemode/InteractionResult.h"
#include "mc/world/gamemode/SurvivalMode.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/BlockType.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/phys/AABB.h"
#include "mc/network/LoopbackPacketSender.h"
#include "mc/network/MinecraftPacketIds.h"
#include "mc/network/packet/PlayerAuthInputPacket.h"
#include <cmath>
#include <atomic>
#include <chrono>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <tuple>
#include <vector>

namespace lamium::interaction::edgeGuard {
namespace {
// Drops up to this height (stairs, slabs, a carpet) stay walkable, as with
// vanilla sneaking.
constexpr float stepHeight = 0.6f;
// Ground friction of ordinary blocks (block 0.6 times 0.91): the velocity kept
// per tick. Slipperier blocks slide further than the input guard expects.
constexpr double groundFriction = 0.546;
// The move-input guard keeps the feet this far over the block, not touching
// its edge: released from sneaking right at an edge, a server drops a player
// whose box only touches the block (seen on BDS, L-160).
// The margin is a setting (default 0.1, at least 0.02): on BDS the server's
// position drifted 0.04 further out than the client's at an edge and the
// player fell after its correction with 0.02 (L-161).
float overlapMargin() { return Runtime::instance().snapshot()->interaction.edgeMargin; }
bool installed = false;
std::atomic<bool> reportedServerCopy = false;
bool reportedInput = false;
bool reportedPacket = false;
// A local world's server computes move vectors on its own thread; only the
// client's are guarded there, and client state is read only on its thread.
std::atomic<std::thread::id> clientThread;
// What the server's copy is guarded with: the client player as its last
// client-thread move left it. Older than a few ticks, nothing is guarded.
struct ClientCopy {
    AABB box;
    bool walking = false;
    std::chrono::steady_clock::time_point at;
};
constexpr auto clientCopyLife = std::chrono::milliseconds{250};
std::mutex clientCopyMutex;
ClientCopy clientCopy;

bool isLocal(StrictEntityContext const& entity, LocalPlayer& player) {
    auto const& own = player.getEntityContext();
    return static_cast<EntityId const&>(entity.mEntity) == own.mEntity
        && static_cast<uint const&>(entity.mRegistryId) == static_cast<uint const&>(own.mRegistry.mId);
}
// Walking on the ground, the case both guards cover.
bool walking(LocalPlayer& player, bool sneakingToo = false) {
    return player.isOnGround() && (sneakingToo || !player.isSneaking()) && !player.isFlying() && !player.isGliding()
        && !player.isSwimming() && !player.isInWater() && !player.getVehicle();
}
void guard(StrictEntityContext const& entity, AABBShapeComponent const& shape, MoveRequestComponent& request,
           IConstBlockSource const& region, GetCollisionShapeInterface const& collisionShape) {
    auto& runtime = Runtime::instance();
    if (!runtime.enabled() || !runtime.snapshot()->interaction.edgeGuard) return;
    auto boxOf = [](AABB const& b) { return Box{b.min.x, b.min.y, b.min.z, b.max.x, b.max.y, b.max.z}; };
    auto reportServerCopy = [&] {
        if (!reportedServerCopy.exchange(true))
            runtime.self().getLogger().info("Edge Guard: also guarding the local server's copy of the player");
    };
    bool walks;
    if (std::this_thread::get_id() == clientThread.load()) {
        auto client = ll::service::getClientInstance();
        auto* player = client ? client->getLocalPlayer() : nullptr;
        if (!player) return;
        bool local = isLocal(entity, *player);
        if (!local) {
            if (!sameBodyNearby(boxOf(player->getAABB()), boxOf(shape.mAABB.get()))) return;
            reportServerCopy();
        }
        // Walking on the ground, sneaking too: vanilla sneaking stops with the
        // box only touching an edge, which a server treats as no support (L-160).
        walks = walking(*player, true);
        if (local) {
            std::lock_guard lock{clientCopyMutex};
            clientCopy = {player->getAABB(), walks, std::chrono::steady_clock::now()};
        }
    } else {
        // In a local world the integrated server moves its own copy of this
        // player on its own thread and corrects the client to it, so that copy
        // is guarded too, against the player as the client's last tick left
        // it: the client player is never read from this thread (L-406).
        ClientCopy own;
        {
            std::lock_guard lock{clientCopyMutex};
            own = clientCopy;
        }
        if (std::chrono::steady_clock::now() - own.at > clientCopyLife) return;
        if (!sameBodyNearby(boxOf(own.box), boxOf(shape.mAABB.get()))) return;
        reportServerCopy();
        walks = own.walking;
    }
    if (!walks) return;
    Vec3& speed = request.mSpeed;
    if (speed.y > 0) return; // A jump may leave the edge on purpose.
    AABB const& box = shape.mAABB;
    std::vector<AABB> shapes;
    float const margin = overlapMargin();
    auto supported = [&](double dx, double dz) {
        // The margin keeps the box over the block: sliding along a wall into
        // a corner left it only touching, and the client then fell (L-160).
        AABB probe{Vec3{box.min.x + margin + static_cast<float>(dx), box.min.y - stepHeight,
                        box.min.z + margin + static_cast<float>(dz)},
                   Vec3{box.max.x - margin + static_cast<float>(dx), box.min.y,
                        box.max.z - margin + static_cast<float>(dz)}};
        shapes.clear();
        region.fetchCollisionShapes(shapes, probe, false, collisionShape, nullptr);
        return !shapes.empty();
    };
    if (!supported(0, 0)) return; // Already over a drop: do not freeze in the air.
    auto [dx, dz] = guardEdge(speed.x, speed.z, supported);
    speed.x = static_cast<float>(dx);
    speed.z = static_cast<float>(dz);
}
// L-160: a server moves the player from the move vector the client sends, so
// the vector itself is guarded where vanilla computes it from the input state
// (sent and simulated alike). Only the local player's own states are touched.
void guardMoveVector(MoveInputState const& state, Vec2& move) {
    if (std::this_thread::get_id() != clientThread.load()) {
        return;
    }
    auto& runtime = Runtime::instance();
    if (!runtime.enabled() || !runtime.snapshot()->interaction.edgeGuard) return;
    auto client = ll::service::getClientInstance();
    auto* player = client ? client->getLocalPlayer() : nullptr;
    if (!player) return;
    auto& context = player->getEntityContext();
    auto raw = context.tryGetComponent<RawMoveInputComponent>();
    auto input = context.tryGetComponent<MoveInputComponent>();
    bool own = (raw && &state == &*raw->mRawInput)
        || (input && (&state == &*input->mInputState || &state == &*input->mRawInputState));
    // Sneaking too: the guard then keeps the margin, so letting go of sneak
    // at an edge does not leave the player only touching it.
    if (!own) return;
    history::observe();
    if (!walking(*player, true)) return;
    if (state.mFlagValues->test(static_cast<size_t>(MoveInputState::Flag::JumpDown))) return; // May leave on purpose.
    auto const& box = player->getAABB();
    auto const& region = player->getDimensionBlockSourceConst();
    std::vector<AABB> shapes;
    float const margin = overlapMargin();
    auto supported = [&](double dx, double dz) {
        AABB probe{Vec3{box.min.x + margin + static_cast<float>(dx), box.min.y - stepHeight,
                        box.min.z + margin + static_cast<float>(dz)},
                   Vec3{box.max.x - margin + static_cast<float>(dx), box.min.y,
                        box.max.z - margin + static_cast<float>(dz)}};
        shapes.clear();
        region.fetchCollisionShapes(shapes, probe, false, nullptr, nullptr);
        return !shapes.empty();
    };
    double yaw = player->getRotation().y;
    auto [x, z] = worldMove(move.x, move.z, yaw);
    double gx = x, gz = z;
    if (supported(0, 0)) {
        auto const& velocity = player->getPosDelta();
        double accel = std::max(0.1, static_cast<double>(player->getSpeed()));
        std::tie(gx, gz) = guardInput(x, z, velocity.x, velocity.z, accel, groundFriction, supported);
    } else if (auto back = stepBack([&](double dx, double dz) {
                   // Only where the body can go: in a corner the first
                   // supported direction was into the wall (L-160).
                   AABB body{Vec3{box.min.x + static_cast<float>(dx), box.min.y + 0.1f, box.min.z + static_cast<float>(dz)},
                             Vec3{box.max.x + static_cast<float>(dx), box.max.y, box.max.z + static_cast<float>(dz)}};
                   shapes.clear();
                   region.fetchCollisionShapes(shapes, body, false, nullptr, nullptr);
                   return shapes.empty() && supported(dx, dz);
               })) {
        std::tie(gx, gz) = *back; // Only touching an edge: back onto the block.
    } else {
        return; // Already over a drop: do not freeze in the air.
    }
    if (gx == x && gz == z) return;
    auto [strafe, forward] = inputMove(gx, gz, yaw);
    move.x = static_cast<float>(strafe);
    move.z = static_cast<float>(forward);
    // The server rebuilds the vector from the sent input state (seen on BDS:
    // the packet's move was guarded, its direction keys were not, and the
    // server's correction followed the keys). The state therefore carries the
    // guarded vector as an analog input without direction keys; it is rebuilt
    // from the devices every tick.
    using Flag = MoveInputState::Flag;
    auto& sent = const_cast<MoveInputState&>(state);
    for (auto flag : {Flag::Up, Flag::Down, Flag::Left, Flag::Right, Flag::UpLeft, Flag::UpRight, Flag::DownLeft, Flag::DownRight})
        sent.mFlagValues->set(static_cast<size_t>(flag), false);
    // The sent input is scaled for sneaking again (0.3, seen in the trace).
    float scale = player->isSneaking() ? 0.3f : 1.0f;
    Vec2 analog{move.x / scale, move.z / scale};
    *sent.mAnalogMoveVector = analog;
    if (raw && &state == &*raw->mRawInput) *raw->mRawMove = analog;
    if (!reportedInput) {
        reportedInput = true;
        runtime.self().getLogger().info("Edge Guard: guarding the move input");
    }
}
// L-161: sneaking refuses a block placed into the player's own cell; walking
// with Edge Guard does not, and a slab merged into the slab the player barely
// stands on pushes them out sideways, off the edge. Refused like sneaking,
// only then: a merge under the player's center lifts them and stays allowed.
bool refusesMerge(Player& actor, ItemStack const& item, BlockPos const& pos, uchar face) {
    auto& runtime = Runtime::instance();
    if (!runtime.enabled() || !runtime.snapshot()->interaction.edgeGuard) return false;
    auto client = ll::service::getClientInstance();
    auto* player = client ? client->getLocalPlayer() : nullptr;
    if (!player || static_cast<Player*>(player) != &actor || !walking(*player)) return false;
    Block const* held = item.mBlock;
    if (!held || !held->getBlockType().isSlabBlock()) return false;
    auto const& box = player->getAABB();
    Box body{box.min.x, box.min.y, box.min.z, box.max.x, box.max.y, box.max.z};
    auto const& region = player->getDimensionBlockSourceConst();
    auto pushes = [&](BlockPos const& cell) {
        return region.getBlock(cell).getBlockType().isSlabBlock() && mergePushesOff(body, cell.x, cell.y, cell.z);
    };
    BlockPos next = pos;
    switch (face) {
    case 0: --next.y; break;
    case 1: ++next.y; break;
    case 2: --next.z; break;
    case 3: ++next.z; break;
    case 4: --next.x; break;
    case 5: ++next.x; break;
    default: return false;
    }
    // The clicked slab merges only through its top or bottom face.
    bool refused = ((face == 0 || face == 1) && pushes(pos)) || pushes(next);
    return refused;
}
bool refuses(Player& actor, ItemStack const& item, BlockPos const& pos, uchar face) {
    try {
        return versionSensitiveAllowed(capabilities::Id::EdgeGuardMovement) && refusesMerge(actor, item, pos, face);
    } catch (...) { return false; }
}
// Building starts before useItemOn and may already ask the server; refuse
// there too, with the item in the building hand (as FreeCamera does).
ItemStack const& heldIn(Player& actor, HandSlot hand) {
    return hand == HandSlot::Offhand ? actor.getOffhandSlot() : actor.getSelectedItem();
}
LL_TYPE_INSTANCE_HOOK(EdgeGuardBuildHook, ll::memory::HookPriority::Normal, GameMode,
    &GameMode::$buildBlock, bool, BlockPos const& pos, uchar face, HandSlot hand, bool simTick) {
    if (refuses(mPlayer, heldIn(mPlayer, hand), pos, face)) return false;
    return origin(pos, face, hand, simTick);
}
LL_TYPE_INSTANCE_HOOK(EdgeGuardSurvivalBuildHook, ll::memory::HookPriority::Normal, SurvivalMode,
    &SurvivalMode::$buildBlock, bool, BlockPos const& pos, uchar face, HandSlot hand, bool simTick) {
    if (refuses(mPlayer, heldIn(mPlayer, hand), pos, face)) return false;
    return origin(pos, face, hand, simTick);
}
LL_TYPE_INSTANCE_HOOK(EdgeGuardStartBuildHook, ll::memory::HookPriority::Normal, GameMode,
    &GameMode::$startBuildBlock, void, BlockPos const& pos, uchar face, HandSlot hand) {
    if (!refuses(mPlayer, heldIn(mPlayer, hand), pos, face)) origin(pos, face, hand);
}
LL_TYPE_INSTANCE_HOOK(EdgeGuardSurvivalStartBuildHook, ll::memory::HookPriority::Normal, SurvivalMode,
    &SurvivalMode::$startBuildBlock, void, BlockPos const& pos, uchar face, HandSlot hand) {
    if (!refuses(mPlayer, heldIn(mPlayer, hand), pos, face)) origin(pos, face, hand);
}
LL_TYPE_INSTANCE_HOOK(EdgeGuardPlaceHook, ll::memory::HookPriority::Normal, GameMode,
    &GameMode::$useItemOn, InteractionResult, ItemStack& item, BlockPos const& pos, uchar face,
    Vec3 const& hit, HandSlot hand, Block const* target, bool first) {
    if (refuses(mPlayer, item, pos, face)) return InteractionResult{false, false};
    return origin(item, pos, face, hit, hand, target, first);
}
// SurvivalMode overrides useItemOn; the base hook does not run in survival.
LL_TYPE_INSTANCE_HOOK(EdgeGuardSurvivalPlaceHook, ll::memory::HookPriority::Normal, SurvivalMode,
    &SurvivalMode::$useItemOn, InteractionResult, ItemStack& item, BlockPos const& pos, uchar face,
    Vec3 const& hit, HandSlot hand, Block const* target, bool first) {
    if (refuses(mPlayer, item, pos, face)) return InteractionResult{false, false};
    return origin(item, pos, face, hit, hand, target, first);
}
LL_STATIC_HOOK(EdgeGuardInputHook, ll::memory::HookPriority::Normal, &PlayerMovement::calculateMoveVector, Vec2,
    MoveInputState const& inputState, bool playerIsFlying, ActorDataFlagComponent const& playerData, bool playerIsInWater,
    SneakingComponent const* sneakComponent) {
    auto move = origin(inputState, playerIsFlying, playerData, playerIsInWater, sneakComponent);
    try {
        if (versionSensitiveAllowed(capabilities::Id::EdgeGuardMovement)) guardMoveVector(inputState, move);
    } catch (...) {}
    return move;
}
LL_TYPE_INSTANCE_HOOK(EdgeGuardSendHook, ll::memory::HookPriority::High, LoopbackPacketSender,
    &LoopbackPacketSender::$send, void, Packet& packet) {
    try {
        auto& runtime = Runtime::instance();
        if (history::onClientThread() && runtime.enabled() && runtime.snapshot()->interaction.edgeGuard
            && packet.getId() == MinecraftPacketIds::PlayerAuthInputPacket
            && versionSensitiveAllowed(capabilities::Id::EdgeGuardMovement)) {
            auto client = ll::service::getClientInstance();
            auto* player = client ? client->getLocalPlayer() : nullptr;
            auto input = player ? player->getEntityContext().tryGetComponent<MoveInputComponent>() : nullptr;
            if (input && packet.mSenderSubId == client->getClientSubId() && walking(*player, true)) {
                auto& auth = static_cast<PlayerAuthInputPacket&>(packet);
                using A = PlayerAuthInputPacketPayload::InputData;
                using F = MoveInputState::Flag;
                bool directions = false;
                for (auto flag : {A::Up, A::Down, A::Left, A::Right, A::UpLeft, A::UpRight, A::DownLeft, A::DownRight})
                    directions |= auth.mInputData->contains(flag);
                for (auto flag : {F::Up, F::Down, F::Left, F::Right, F::UpLeft, F::UpRight, F::DownLeft, F::DownRight})
                    directions |= input->mInputState->mFlagValues->test(static_cast<size_t>(flag));
                bool mayLeave = input->mInputState->mFlagValues->test(static_cast<size_t>(F::JumpDown))
                    || input->mRawInputState->mFlagValues->test(static_cast<size_t>(F::JumpDown));
                for (auto flag : {A::JumpDown, A::Jumping, A::StartJumping, A::JumpCurrentRaw,
                                  A::JumpPressedRaw, A::HandledTeleport, A::IsInClientPredictedVehicle})
                    mayLeave |= auth.mInputData->contains(flag);
                if (alignGuardedPacketRaw(*auth.mRawMoveVector, *auth.mMove, *auth.mAnalogMoveVector,
                                         *input->mMove, *input->mInputState->mAnalogMoveVector, directions, mayLeave)) {
                    if (!reportedPacket) {
                        reportedPacket = true;
                        runtime.self().getLogger().info("Edge Guard: sending guarded raw movement without changing device input");
                    }
                }
            }
        }
    } catch (...) {}
    origin(packet);
}
LL_STATIC_HOOK(EdgeGuardHook, ll::memory::HookPriority::Normal, &MoveCollisionSystem::fetchCollisionShapes, void,
    StrictEntityContext const& entity, AABBShapeComponent const& aabb, MaxAutoStepComponent const& autoStep,
    Optional<CollidableMobNearFlagComponent const> collidableMobNear, MoveRequestComponent& request,
    Optional<MinecartFlagComponent const> isMinecart,
    ViewT<StrictEntityContext, Include<CollidableMobFlagComponent>, AABBShapeComponent const> const& collidableMobs,
    ViewT<StrictEntityContext, AABBShapeComponent const, ActorDataFlagComponent const> const& stackableView,
    ViewT<StrictEntityContext, Include<FallingBlockFlagComponent>> const& fallingBlocks, IConstBlockSource const& region,
    LocalSpatialEntityFetcher& fetcher, GetCollisionShapeInterface const& collisionShape,
    std::vector<BlockSourceVisitor::CollisionShape>& tempCollisionShapes,
    std::vector<BlockSourceVisitor::CollisionShape>& scratchCollisionShapes, std::vector<AABB>& tempShapes) {
    try {
        if (versionSensitiveAllowed(capabilities::Id::EdgeGuardMovement)) guard(entity, aabb, request, region, collisionShape);
    } catch (...) {}
    origin(entity, aabb, autoStep, collidableMobNear, request, isMinecart, collidableMobs, stackableView, fallingBlocks,
        region, fetcher, collisionShape, tempCollisionShapes, scratchCollisionShapes, tempShapes);
}
}
namespace history {
bool onClientThread() noexcept { return std::this_thread::get_id() == clientThread.load(); }
bool walking(LocalPlayer& player) { return edgeGuard::walking(player, true); }
}
void start() {
    if (installed) return;
    capabilities::begin(capabilities::Id::EdgeGuardMovement);
    capabilities::begin(capabilities::Id::EdgeGuardReplay);
    ll::thread::ClientThreadExecutor::getDefault().execute([] { clientThread.store(std::this_thread::get_id()); });
    if (EdgeGuardHook::hook(true) != 0) {
        capabilities::hooks(capabilities::Id::EdgeGuardMovement,false);
        throw std::runtime_error("Could not install the edge guard hook");
    }
    if (EdgeGuardInputHook::hook(true) != 0) {
        capabilities::hooks(capabilities::Id::EdgeGuardMovement,false);
        EdgeGuardHook::unhook(true);
        throw std::runtime_error("Could not install the edge guard input hook");
    }
    if (EdgeGuardPlaceHook::hook(true) != 0 || EdgeGuardSurvivalPlaceHook::hook(true) != 0
        || EdgeGuardBuildHook::hook(true) != 0 || EdgeGuardSurvivalBuildHook::hook(true) != 0
        || EdgeGuardStartBuildHook::hook(true) != 0 || EdgeGuardSurvivalStartBuildHook::hook(true) != 0) {
        EdgeGuardPlaceHook::unhook(true);
        EdgeGuardSurvivalPlaceHook::unhook(true);
        EdgeGuardBuildHook::unhook(true);
        EdgeGuardSurvivalBuildHook::unhook(true);
        EdgeGuardStartBuildHook::unhook(true);
        EdgeGuardSurvivalStartBuildHook::unhook(true);
        EdgeGuardInputHook::unhook(true);
        EdgeGuardHook::unhook(true);
        capabilities::hooks(capabilities::Id::EdgeGuardMovement,false);
        throw std::runtime_error("Could not install the edge guard placement hooks");
    }
    if (EdgeGuardSendHook::hook(true) != 0) {
        capabilities::hooks(capabilities::Id::EdgeGuardMovement,false);
        EdgeGuardPlaceHook::unhook(true); EdgeGuardSurvivalPlaceHook::unhook(true);
        EdgeGuardBuildHook::unhook(true); EdgeGuardSurvivalBuildHook::unhook(true);
        EdgeGuardStartBuildHook::unhook(true); EdgeGuardSurvivalStartBuildHook::unhook(true);
        EdgeGuardInputHook::unhook(true); EdgeGuardHook::unhook(true);
        throw std::runtime_error("Could not install the edge guard packet hook");
    }
    installed = true;
    capabilities::ready(capabilities::Id::EdgeGuardMovement);
}
void stop() {
    capabilities::stop(capabilities::Id::EdgeGuardMovement);
    if (!installed) return;
    bool place = EdgeGuardPlaceHook::unhook(true) & EdgeGuardSurvivalPlaceHook::unhook(true);
    bool survival = EdgeGuardBuildHook::unhook(true) & EdgeGuardSurvivalBuildHook::unhook(true)
        & EdgeGuardStartBuildHook::unhook(true) & EdgeGuardSurvivalStartBuildHook::unhook(true);
    bool input = EdgeGuardInputHook::unhook(true), movement = EdgeGuardHook::unhook(true);
    bool capture = history::stop();
    bool send = EdgeGuardSendHook::unhook(true);
    if (place && survival && input && movement && capture && send) installed = false;
    else capabilities::fail(capabilities::Id::EdgeGuardMovement,capabilities::Reason::RemovalFailed);
}
}
