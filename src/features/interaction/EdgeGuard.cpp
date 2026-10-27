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
#ifdef LAMIUM_EDGE_TRACE
#include "features/interaction/EdgeGuardTracePlan.h"
#include "mc/entity/utilities/IReplayableActorInput.h"
#include "mc/entity/components/ReplayStateComponent.h"
#include "mc/world/actor/ActorHistory.h"
#include "mc/client/network/ClientNetworkHandler.h"
#include "mc/network/packet/CorrectPlayerMovePredictionPacket.h"
#include "mc/entity/systems/EntitySystems.h"
#include "mc/entity/systems/ClientPlayerRewindSystem.h"
#include "mc/deps/vanilla_components/StateVectorComponent.h"
#include <deque>
#include <format>
#include <string>
#include <array>
#endif
#include <atomic>
#include <chrono>
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
bool reportedServerCopy = false;
bool reportedInput = false;
#ifdef LAMIUM_EDGE_PERF
// L-250: what Edge Guard costs. Every world query the guards make is counted
// and timed, and a window is logged every five seconds.
namespace perf {
inline int64_t nowNs() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
struct Counters {
    std::atomic<uint64_t> calc{0}, owned{0}, full{0}, probes{0}, observeCalls{0}, systemCalls{0}, systemFull{0};
    std::atomic<int64_t> guardNs{0}, probeNs{0}, observeNs{0}, systemNs{0}, maxFullNs{0}, maxProbeNs{0};
};
Counters counters;
std::atomic<int64_t> windowStart{0};
void addMax(std::atomic<int64_t>& slot, int64_t value) noexcept {
    auto current = slot.load(std::memory_order_relaxed);
    while (value > current && !slot.compare_exchange_weak(current, value, std::memory_order_relaxed)) {}
}
// Times a scope into one total, optionally keeping the longest.
struct Timer {
    explicit Timer(std::atomic<int64_t>& totalNs, std::atomic<int64_t>* maxNs = nullptr) : total(totalNs), max(maxNs) {}
    ~Timer() {
        auto const elapsed = nowNs() - start;
        total.fetch_add(elapsed, std::memory_order_relaxed);
        if (max) addMax(*max, elapsed);
    }
    int64_t start = nowNs();
    std::atomic<int64_t>& total;
    std::atomic<int64_t>* max;
};
// Times one collision query and counts it.
inline void probe(void const* region, auto&& fetch) {
    auto const started = nowNs();
    fetch();
    counters.probes.fetch_add(1, std::memory_order_relaxed);
    counters.probeNs.fetch_add(nowNs() - started, std::memory_order_relaxed);
    (void)region;
}
void flush() noexcept {
    try {
        auto const now = nowNs();
        auto window = windowStart.load(std::memory_order_relaxed);
        if (window == 0) {
            windowStart.store(now, std::memory_order_relaxed);
            return;
        }
        if (now - window < 5'000'000'000LL) return;
        if (!windowStart.compare_exchange_strong(window, now, std::memory_order_relaxed)) return;
        auto take = [](std::atomic<uint64_t>& c) { return c.exchange(0, std::memory_order_relaxed); };
        auto ns = [](std::atomic<int64_t>& c) { return c.exchange(0, std::memory_order_relaxed); };
        auto const calc = take(counters.calc), owned = take(counters.owned), full = take(counters.full),
                   probes = take(counters.probes), observeCalls = take(counters.observeCalls),
                   systemCalls = take(counters.systemCalls), systemFull = take(counters.systemFull);
        auto const guard = ns(counters.guardNs), probe = ns(counters.probeNs), observe = ns(counters.observeNs),
                   system = ns(counters.systemNs), maxFull = ns(counters.maxFullNs), maxProbe = ns(counters.maxProbeNs);
        auto const ms = [](int64_t value) { return static_cast<double>(value) / 1e6; };
        auto const each = [](double total, uint64_t count) { return count ? total * 1e3 / static_cast<double>(count) : 0.0; };
        Runtime::instance().self().getLogger().info(
            "L-250 perf 5 s: calc {} (owned {}, full {}); probes {} ({:.1f} per full guard); guard {:.2f} ms "
            "({:.0f} us per full, max {:.0f} us); probe {:.2f} ms ({:.0f} us each, max {:.0f}); observe {:.2f} ms "
            "({:.0f} us per call); move system {} calls, {} full, {:.2f} ms ({:.0f} us per full)",
            calc, owned, full, probes, full ? static_cast<double>(probes) / static_cast<double>(full) : 0.0, ms(guard),
            each(ms(guard), full), static_cast<double>(maxFull) / 1e3, ms(probe), each(ms(probe), probes),
            static_cast<double>(maxProbe) / 1e3, ms(observe), each(ms(observe), observeCalls), systemCalls, systemFull,
            ms(system), each(ms(system), systemFull));
    } catch (...) {}
}
}
#endif
bool reportedPacket = false;
// A local world's server computes move vectors on its own thread; only the
// client's are guarded there, and client state is read only on its thread.
std::atomic<std::thread::id> clientThread;
#ifdef LAMIUM_EDGE_TRACE
// L-160 trace: the last lines around the guard's activity are kept in memory
// and written out only when the player falls soon after the guard acted.
int traceQuiet = 1000, traceDumps = 0, traceSends = 0;
int traceCorrections = 0, traceCorrectionTail = 0, traceCorrectionDumps = 0;
int traceBaselineTail = -1;
thread_local int traceReplayDepth = 0;
std::atomic<int> traceOtherReplay = 0, traceOtherCalc = 0, traceOtherCollision = 0;
std::deque<std::string> traceRing;
bool traceFalling = false;
float traceGroundY = 0;
void trace(std::string text) {
    traceRing.push_back(std::format("#{} {}", traceSends, std::move(text)));
    if (traceRing.size() > 400) traceRing.pop_front();
}
bool traceActive() {
    auto& runtime = Runtime::instance();
    return std::this_thread::get_id() == clientThread.load() && runtime.enabled()
        && runtime.snapshot()->interaction.edgeGuard && traceQuiet <= 100
        && versionSensitiveAllowed(capabilities::Id::EdgeGuardMovement);
}
void traceCorrectionEnd() {
    if (!traceCorrectionTail || --traceCorrectionTail) return;
    ++traceCorrectionDumps;
    auto& logger = Runtime::instance().self().getLogger();
    logger.info("L-162 correction window {} (correction {}); last {} lines:",
        traceCorrectionDumps, traceCorrections, traceRing.size());
    for (auto const& line : traceRing) logger.info("L-162 {}", line);
    traceRing.clear();
}
// Placement lines are rare: written at once, bounded.
int tracePlaceLines = 0;
void traceNow(std::string const& text) {
    if (tracePlaceLines >= 3000) return;
    ++tracePlaceLines;
    Runtime::instance().self().getLogger().info("L-161 {}", text);
}
void traceFall(float y, bool ground) {
    if (ground) { traceGroundY = y; traceFalling = false; return; }
    if (traceFalling || y > traceGroundY - 1.2f) return;
    traceFalling = true;
    if (traceQuiet > 100 || traceDumps >= 12) return;
    ++traceDumps;
    auto& logger = Runtime::instance().self().getLogger();
    logger.info("L-160 fall {} after the guard acted {} sends ago; last {} lines:", traceDumps, traceQuiet, traceRing.size());
    for (auto const& line : traceRing) logger.info("L-160 {}", line);
    traceRing.clear();
}
char const* stateName(MoveInputState const& state, LocalPlayer& player) {
    auto& context = player.getEntityContext();
    auto raw = context.tryGetComponent<RawMoveInputComponent>();
    auto input = context.tryGetComponent<MoveInputComponent>();
    if (raw && &state == &*raw->mRawInput) return "raw";
    if (input && &state == &*input->mInputState) return "input";
    if (input && &state == &*input->mRawInputState) return "inputRaw";
    return "other";
}
std::string flagText(MoveInputState const& state) {
    using F = MoveInputState::Flag;
    std::string out;
    for (auto [flag, name] : {std::pair{F::Up, "U"}, {F::Down, "D"}, {F::Left, "L"}, {F::Right, "R"}, {F::UpLeft, "UL"},
                              {F::UpRight, "UR"}, {F::JumpDown, "J"}, {F::SprintDown, "S"}, {F::SneakDown, "Sh"}})
        if (state.mFlagValues->test(static_cast<size_t>(flag))) { out += name; out += ' '; }
    return out;
}
void traceInput(char const* stage, MoveInputComponent const& input, void const* history) {
    trace(std::format("{} history {} input [{}] analog {:.3f},{:.3f} raw [{}] analog {:.3f},{:.3f} move {:.3f},{:.3f}",
        stage, history, flagText(input.mInputState), input.mInputState->mAnalogMoveVector->x,
        input.mInputState->mAnalogMoveVector->z, flagText(input.mRawInputState),
        input.mRawInputState->mAnalogMoveVector->x, input.mRawInputState->mAnalogMoveVector->z,
        input.mMove->x, input.mMove->z));
}
void traceEntity(char const* stage, EntityContext const& entity, void const* history) {
    auto client = ll::service::getClientInstance();
    auto* player = client ? client->getLocalPlayer() : nullptr;
    if (!player) return;
    auto const& own = player->getEntityContext();
    trace(std::format("{} history {} replay {} entity {}/{} live {}/{} clientThread {} liveGround {}",
        stage, history, traceReplayDepth, static_cast<uint>(entity.mRegistry.mId), entity.mEntity.mRawId,
        static_cast<uint>(own.mRegistry.mId), own.mEntity.mRawId,
        std::this_thread::get_id() == clientThread.load(), player->isOnGround()));
    if (auto input = entity.tryGetComponent<MoveInputComponent>()) traceInput(stage, *input, history);
    if (auto shape = entity.tryGetComponent<AABBShapeComponent>()) {
        auto const& box = shape->mAABB.get();
        trace(std::format("{} box {:.3f},{:.3f},{:.3f}..{:.3f},{:.3f},{:.3f}", stage,
            box.min.x, box.min.y, box.min.z, box.max.x, box.max.y, box.max.z));
    }
    if (auto motion = entity.tryGetComponent<StateVectorComponent>())
        trace(std::format("{} position {:.3f},{:.3f},{:.3f} delta {:.3f},{:.3f},{:.3f}", stage,
            motion->mPos->x, motion->mPos->y, motion->mPos->z,
            motion->mPosDelta->x, motion->mPosDelta->y, motion->mPosDelta->z));
}
bool traceLocalEntity(EntityContext const& entity) {
    auto client = ll::service::getClientInstance();
    auto* player = client ? client->getLocalPlayer() : nullptr;
    if (!player) return false;
    auto const& own = player->getEntityContext();
    return &entity.mRegistry == &own.mRegistry && entity.mEntity == own.mEntity;
}
std::uintptr_t traceCaptureTarget = 0, tracePreTarget = 0, tracePostTarget = 0;
bool traceInputInstalled = false, traceInputUnavailable = false;
LL_TYPE_INSTANCE_HOOK(EdgeTraceCapture, ll::memory::HookPriority::Normal, IReplayableActorInput,
    traceCaptureTarget, void, MoveInputComponent const& input) {
    try {
        if (traceActive()) {
            auto client = ll::service::getClientInstance();
            auto* player = client ? client->getLocalPlayer() : nullptr;
            auto own = player ? player->getEntityContext().tryGetComponent<MoveInputComponent>() : nullptr;
            if (own && &input == &*own) traceInput("capture local input", input, this);
            else if (traceReplayDepth) traceInput("capture replay input", input, this);
        }
    } catch (...) {}
    origin(input);
}
LL_TYPE_INSTANCE_HOOK(EdgeTracePreApply, ll::memory::HookPriority::Normal, IReplayableActorInput,
    tracePreTarget, void, EntityContext& entity) {
    try { if (traceActive()) traceEntity("preApply before", entity, this); } catch (...) {}
    origin(entity);
    try { if (traceActive()) traceEntity("preApply after", entity, this); } catch (...) {}
}
LL_TYPE_INSTANCE_HOOK(EdgeTracePostApply, ll::memory::HookPriority::Normal, IReplayableActorInput,
    tracePostTarget, void, EntityContext& entity) {
    try { if (traceActive()) traceEntity("postApply before", entity, this); } catch (...) {}
    origin(entity);
    try { if (traceActive()) traceEntity("postApply after", entity, this); } catch (...) {}
}
void bindInputTrace(IReplayableActorInput& input) {
    if (traceInputUnavailable) return;
    auto unavailable = [](char const* reason) {
        traceInputUnavailable = true;
        Runtime::instance().self().getLogger().warn("L-162 input diagnostics unavailable: {}", reason);
    };
    auto slots = history::inputSlots();
    auto targets = history::inputTargets(input);
    if (!slots || !targets) { unavailable("unverified compiler slots or game input targets"); return; }
    if (traceInputInstalled) {
        if (*targets != std::array{traceCaptureTarget, tracePreTarget, tracePostTarget})
            unavailable("selected input implementation changed");
        return;
    }
    traceCaptureTarget = (*targets)[0]; tracePreTarget = (*targets)[1]; tracePostTarget = (*targets)[2];
    if (EdgeTraceCapture::hook(true) != 0 || EdgeTracePreApply::hook(true) != 0 || EdgeTracePostApply::hook(true) != 0) {
        EdgeTraceCapture::unhook(true); EdgeTracePreApply::unhook(true); EdgeTracePostApply::unhook(true);
        unavailable("could not attach input diagnostics"); return;
    }
    traceInputInstalled = true;
    Runtime::instance().self().getLogger().info("L-162 input diagnostics attached (compiler slots {},{},{}); read only", (*slots)[0], (*slots)[1], (*slots)[2]);
}
LL_TYPE_INSTANCE_HOOK(EdgeTraceSelectedInput, ll::memory::HookPriority::Normal, ReplayStateComponent,
    &ReplayStateComponent::getCurrentCapturedInput, IReplayableActorInput*) {
    auto* result = origin();
    try {
        if (result && traceActive()) {
            auto client = ll::service::getClientInstance();
            auto* player = client ? client->getLocalPlayer() : nullptr;
            auto own = player ? player->getEntityContext().tryGetComponent<ReplayStateComponent>() : nullptr;
            bool local = own && &*own == this;
            if (local) bindInputTrace(*result);
            if (local || traceReplayDepth)
                trace(std::format("selected input {} replay {} local {} tick {} history {}", static_cast<void*>(result),
                    traceReplayDepth, local, static_cast<uint64>(mCurrentTick), static_cast<void*>(mHistory.get())));
        }
    } catch (...) {}
    return result;
}
LL_TYPE_INSTANCE_HOOK(EdgeTraceHistoryFrame, ll::memory::HookPriority::Normal, ActorHistory,
    &ActorHistory::addFrame, void, EntityContext& entity, uint64 frame, EntityRegistry& registry) {
    try {
        if (traceActive() && traceLocalEntity(entity)) {
            trace(std::format("history frame begin tick {}", frame));
            traceEntity("frame live input", entity, this);
        }
    } catch (...) {}
    origin(entity, frame, registry);
    try {
        if (traceActive() && traceLocalEntity(entity))
            trace(std::format("history frame end tick {}", frame));
    } catch (...) {}
}
LL_STATIC_HOOK(EdgeTracePrepare, ll::memory::HookPriority::Normal, &ActorHistory::prepareEntityForRewindFromCapture,
    void, EntityContext const& captured, EntityContext const& live, EntityContext& rewind) {
    try {
        if (traceActive() && traceLocalEntity(live)) {
            traceEntity("prepare captured", captured, nullptr);
            traceEntity("prepare live", live, nullptr);
            traceEntity("prepare rewind before", rewind, nullptr);
        }
    } catch (...) {}
    origin(captured, live, rewind);
    try {
        if (traceActive() && traceLocalEntity(live)) traceEntity("prepare rewind after", rewind, nullptr);
    } catch (...) {}
}
LL_STATIC_HOOK(EdgeTraceApply, ll::memory::HookPriority::Normal, &ClientPlayerRewindSystem::applyHistoryChangesToLiveEntity,
    void, ReplayStateTrackerComponent const& state, EntityContext& live) {
    try { if (traceActive() && traceLocalEntity(live)) traceEntity("apply live before", live, nullptr); } catch (...) {}
    origin(state, live);
    try { if (traceActive() && traceLocalEntity(live)) traceEntity("apply live after", live, nullptr); } catch (...) {}
}
LL_TYPE_INSTANCE_HOOK(EdgeTraceReplay, ll::memory::HookPriority::Normal, EntitySystems,
    &EntitySystems::$tickMovementCorrectionReplay, void, EntityRegistry& registry) {
    struct Scope {
        Scope() { ++traceReplayDepth; }
        ~Scope() { --traceReplayDepth; }
    } scope;
    if (std::this_thread::get_id() != clientThread.load()) ++traceOtherReplay;
    try { if (traceActive()) trace(std::format("replay begin registry {}", static_cast<uint>(registry.mId))); } catch (...) {}
    origin(registry);
    try { if (traceActive()) trace(std::format("replay end registry {}", static_cast<uint>(registry.mId))); } catch (...) {}
}
#endif

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
#ifdef LAMIUM_EDGE_PERF
    perf::counters.systemCalls.fetch_add(1, std::memory_order_relaxed);
#endif
    auto& runtime = Runtime::instance();
    if (!runtime.enabled() || !runtime.snapshot()->interaction.edgeGuard) return;
    auto client = ll::service::getClientInstance();
    auto* player = client ? client->getLocalPlayer() : nullptr;
    if (!player) return;
    // In a local world the integrated server moves its own copy of this
    // player and corrects the client to it, so that copy is guarded too. It
    // is the other registry's entity whose box sits where the player's does.
    if (!isLocal(entity, *player)) {
        auto const& own = player->getAABB();
        auto const& other = shape.mAABB.get();
        if (std::abs(own.min.x - other.min.x) > .3f || std::abs(own.min.y - other.min.y) > .3f
            || std::abs(own.min.z - other.min.z) > .3f || std::abs((own.max.x - own.min.x) - (other.max.x - other.min.x)) > .05f)
            return;
        if (!reportedServerCopy) {
            reportedServerCopy = true;
            runtime.self().getLogger().info("Edge Guard: also guarding the local server's copy of the player");
        }
    }
    // Walking on the ground, sneaking too: vanilla sneaking stops with the box
    // only touching an edge, which a server treats as no support (L-160).
    if (!walking(*player, true)) return;
#ifdef LAMIUM_EDGE_PERF
    perf::counters.systemFull.fetch_add(1, std::memory_order_relaxed);
    perf::Timer guardTimer{perf::counters.systemNs};
#endif
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
#ifdef LAMIUM_EDGE_PERF
        perf::probe(&region, [&] { region.fetchCollisionShapes(shapes, probe, false, collisionShape, nullptr); });
#else
        region.fetchCollisionShapes(shapes, probe, false, collisionShape, nullptr);
#endif
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
#ifdef LAMIUM_EDGE_TRACE
        if (traceReplayDepth) ++traceOtherCalc;
#endif
        return;
    }
#ifdef LAMIUM_EDGE_TRACE
    Vec2 before = move;
    struct Report {
        MoveInputState const& state; Vec2 const& before; Vec2& move;
        ~Report() {
            try {
                auto client = ll::service::getClientInstance();
                auto* player = client ? client->getLocalPlayer() : nullptr;
                if (!player) return;
                bool changed = before.x != move.x || before.z != move.z;
                if (changed) {
                    traceQuiet = 0;
                    if (traceBaselineTail == -1) traceBaselineTail = 8;
                }
                if (traceQuiet > 100) return;
                char const* name = stateName(state, *player);
                if (std::string_view{name} == "other" && !traceReplayDepth) return;
                auto const& pos = player->getPosition();
                auto const& delta = player->getPosDelta();
                trace(std::format("calc {} replay {} flags [{}] analog {:.2f},{:.2f} move {:.2f},{:.2f} -> {:.2f},{:.2f} pos {:.3f},{:.3f} v {:.3f},{:.3f} ground {}",
                    name, traceReplayDepth, flagText(state), state.mAnalogMoveVector->x, state.mAnalogMoveVector->z,
                    before.x, before.z, move.x, move.z, pos.x, pos.z, delta.x, delta.z, player->isOnGround()));
                if (std::string_view{name} != "raw") return;
                // What the support probes can find around the feet (0.5 beyond the box).
                auto const& box = player->getAABB();
                std::vector<AABB> found;
                AABB around{Vec3{box.min.x - .5f, box.min.y - stepHeight, box.min.z - .5f}, Vec3{box.max.x + .5f, box.min.y + .5f, box.max.z + .5f}};
                player->getDimensionBlockSourceConst().fetchCollisionShapes(found, around, false, nullptr, nullptr);
                std::string text = std::format("feet {:.3f} box x {:.3f}..{:.3f} z {:.3f}..{:.3f} shapes", box.min.y, box.min.x, box.max.x,
                    box.min.z, box.max.z);
                for (size_t i = 0; i < found.size() && i < 8; ++i)
                    text += std::format(" [{:.2f}..{:.2f} y{:.2f}..{:.2f} {:.2f}..{:.2f}]", found[i].min.x, found[i].max.x, found[i].min.y,
                        found[i].max.y, found[i].min.z, found[i].max.z);
                trace(text);
            } catch (...) {}
        }
    } report{state, before, move};
#endif
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
#ifdef LAMIUM_EDGE_PERF
    perf::counters.owned.fetch_add(1, std::memory_order_relaxed);
    {
        perf::Timer observeTimer{perf::counters.observeNs};
        perf::counters.observeCalls.fetch_add(1, std::memory_order_relaxed);
        history::observe();
    }
#else
    history::observe();
#endif
    if (!walking(*player, true)) return;
#ifdef LAMIUM_EDGE_PERF
    perf::counters.full.fetch_add(1, std::memory_order_relaxed);
    perf::Timer guardTimer{perf::counters.guardNs, &perf::counters.maxFullNs};
#endif
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
#ifdef LAMIUM_EDGE_PERF
        perf::probe(&region, [&] { region.fetchCollisionShapes(shapes, probe, false, nullptr, nullptr); });
#else
        region.fetchCollisionShapes(shapes, probe, false, nullptr, nullptr);
#endif
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
#ifdef LAMIUM_EDGE_TRACE
    traceNow(std::format("check pos {},{},{} face {} slab here {} next {},{},{} slab {} box x {:.3f}..{:.3f} y {:.3f} z {:.3f}..{:.3f} -> {}",
        pos.x, pos.y, pos.z, face, region.getBlock(pos).getBlockType().isSlabBlock(), next.x, next.y, next.z,
        region.getBlock(next).getBlockType().isSlabBlock(), box.min.x, box.max.x, box.min.y, box.min.z, box.max.z,
        refused ? "refused" : "allowed"));
#endif
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
#ifdef LAMIUM_EDGE_PERF
    perf::counters.calc.fetch_add(1, std::memory_order_relaxed);
#endif
    auto move = origin(inputState, playerIsFlying, playerData, playerIsInWater, sneakComponent);
    try {
        if (versionSensitiveAllowed(capabilities::Id::EdgeGuardMovement)) guardMoveVector(inputState, move);
    } catch (...) {}
#ifdef LAMIUM_EDGE_PERF
    perf::flush();
#endif
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
                Vec2 const before = *auth.mRawMoveVector;
                if (alignGuardedPacketRaw(*auth.mRawMoveVector, *auth.mMove, *auth.mAnalogMoveVector,
                                         *input->mMove, *input->mInputState->mAnalogMoveVector, directions, mayLeave)) {
                    if (!reportedPacket) {
                        reportedPacket = true;
                        runtime.self().getLogger().info("Edge Guard: sending guarded raw movement without changing device input");
                    }
#ifdef LAMIUM_EDGE_TRACE
                    if (traceActive()) trace(std::format("packet raw {:.3f},{:.3f} -> {:.3f},{:.3f} tick {}",
                        before.x, before.z, auth.mRawMoveVector->x, auth.mRawMoveVector->z,
                        static_cast<uint64>(auth.mClientTick->mValue)));
#else
                    (void)before;
#endif
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
#ifdef LAMIUM_EDGE_TRACE
    Vec3 const before = request.mSpeed;
    if (traceReplayDepth && std::this_thread::get_id() != clientThread.load()) ++traceOtherCollision;
#endif
    try {
        if (versionSensitiveAllowed(capabilities::Id::EdgeGuardMovement)) guard(entity, aabb, request, region, collisionShape);
    } catch (...) {}
#ifdef LAMIUM_EDGE_TRACE
    try {
        if (traceReplayDepth && traceActive()) {
            auto const& box = aabb.mAABB.get();
            trace(std::format("replay collision entity {}/{} box {:.3f},{:.3f},{:.3f} speed {:.3f},{:.3f},{:.3f} -> {:.3f},{:.3f},{:.3f}",
                static_cast<uint>(entity.mRegistryId), static_cast<EntityId const&>(entity.mEntity).mRawId,
                box.min.x, box.min.y, box.min.z, before.x, before.y, before.z,
                request.mSpeed->x, request.mSpeed->y, request.mSpeed->z));
        }
    } catch (...) {}
#endif
    origin(entity, aabb, autoStep, collidableMobNear, request, isMinecart, collidableMobs, stackableView, fallingBlocks,
        region, fetcher, collisionShape, tempCollisionShapes, scratchCollisionShapes, tempShapes);
}
}
#ifdef LAMIUM_EDGE_TRACE
LL_TYPE_INSTANCE_HOOK(EdgeTraceBuild, ll::memory::HookPriority::Normal, GameMode,
    &GameMode::$buildBlock, bool, BlockPos const& pos, uchar face, HandSlot hand, bool simTick) {
    bool result = origin(pos, face, hand, simTick);
    traceNow(std::format("GameMode::buildBlock pos {},{},{} face {} simTick {} -> {}", pos.x, pos.y, pos.z, face, simTick, result));
    return result;
}
LL_TYPE_INSTANCE_HOOK(EdgeTraceSurvivalBuild, ll::memory::HookPriority::Normal, SurvivalMode,
    &SurvivalMode::$buildBlock, bool, BlockPos const& pos, uchar face, HandSlot hand, bool simTick) {
    bool result = origin(pos, face, hand, simTick);
    traceNow(std::format("SurvivalMode::buildBlock pos {},{},{} face {} simTick {} -> {}", pos.x, pos.y, pos.z, face, simTick, result));
    return result;
}
using AuthInput = PlayerAuthInputPacketPayload::InputData;
LL_TYPE_INSTANCE_HOOK(EdgeTraceSend, ll::memory::HookPriority::Normal, LoopbackPacketSender,
    &LoopbackPacketSender::$sendToServer, void, Packet& packet) {
    try {
        if (packet.getId() == MinecraftPacketIds::PlayerAuthInputPacket) {
            ++traceSends;
            ++traceQuiet;
            int const otherReplay = traceOtherReplay.exchange(0), otherCalc = traceOtherCalc.exchange(0),
                otherCollision = traceOtherCollision.exchange(0);
            if (traceQuiet <= 100 && (otherReplay || otherCalc || otherCollision))
                trace(std::format("other thread replay {} calc {} collision {}", otherReplay, otherCalc, otherCollision));
            auto& auth = static_cast<PlayerAuthInputPacket&>(packet);
            auto client = ll::service::getClientInstance();
            auto* player = client ? client->getLocalPlayer() : nullptr;
            if (player && traceActive() && !traceInputInstalled && !traceInputUnavailable) {
                if (auto replay = player->getEntityContext().tryGetComponent<ReplayStateComponent>())
                    replay->getCurrentCapturedInput();
            }
            if (player) traceFall(auth.mPos->y, player->isOnGround());
            if (traceQuiet > 100) { origin(packet); return; }
            auto& data = *auth.mInputData;
            std::string flags;
            for (auto [flag, name] : {std::pair{AuthInput::Up, "U"}, {AuthInput::Down, "D"}, {AuthInput::Left, "L"},
                                      {AuthInput::Right, "R"}, {AuthInput::UpLeft, "UL"}, {AuthInput::UpRight, "UR"},
                                      {AuthInput::Sprinting, "S"}, {AuthInput::Sneaking, "Sn"}, {AuthInput::HorizontalCollision, "HC"},
                                      {AuthInput::VerticalCollision, "VC"}, {AuthInput::PerformItemInteraction, "Item"},
                                      {AuthInput::PerformBlockActions, "Block"}, {AuthInput::StartJumping, "Jump"},
                                      {AuthInput::HandledTeleport, "Tp"}})
                if (data.contains(flag)) { flags += name; flags += ' '; }
            trace(std::format("send tick {} move {:.2f},{:.2f} raw {:.2f},{:.2f} analog {:.2f},{:.2f} pos {:.3f},{:.3f},{:.3f} delta {:.3f},{:.3f} flags [{}]",
                static_cast<uint64>(auth.mClientTick->mValue), auth.mMove->x, auth.mMove->z, auth.mRawMoveVector->x, auth.mRawMoveVector->z, auth.mAnalogMoveVector->x,
                auth.mAnalogMoveVector->z, auth.mPos->x, auth.mPos->y, auth.mPos->z, auth.mPosDelta->x, auth.mPosDelta->z, flags));
            if (traceBaselineTail > 0 && --traceBaselineTail == 0) {
                auto& logger = Runtime::instance().self().getLogger();
                logger.info("L-162 first guarded approach; last {} lines:", traceRing.size());
                for (auto const& line : traceRing) logger.info("L-162 {}", line);
            }
            traceCorrectionEnd();
        }
    } catch (...) {}
    origin(packet);
}
LL_TYPE_INSTANCE_HOOK(EdgeTraceCorrection, ll::memory::HookPriority::Normal, ClientNetworkHandler,
    static_cast<void (ClientNetworkHandler::*)(NetworkIdentifier const&, CorrectPlayerMovePredictionPacket const&)>(
        &ClientNetworkHandler::$handle),
    void, NetworkIdentifier const& source, CorrectPlayerMovePredictionPacket const& packet) {
    try {
        if (traceActive()) {
            ++traceCorrections;
            if (traceCorrectionDumps < 16) traceCorrectionTail = 3;
            trace(std::format("correction {} tick {} pos {:.3f},{:.3f},{:.3f} delta {:.3f},{:.3f},{:.3f} ground {}",
                traceCorrections, static_cast<uint64>(packet.mTick->mValue), packet.mPos->x,
                packet.mPos->y, packet.mPos->z, packet.mPosDelta->x, packet.mPosDelta->y, packet.mPosDelta->z, static_cast<bool>(packet.mOnGround)));
        }
    } catch (...) {}
    origin(source, packet);
}
#endif
namespace history {
bool onClientThread() noexcept { return std::this_thread::get_id() == clientThread.load(); }
bool walking(LocalPlayer& player) { return edgeGuard::walking(player, true); }
}
void start() {
    if (installed) return;
    capabilities::begin(capabilities::Id::EdgeGuardMovement);
    capabilities::begin(capabilities::Id::EdgeGuardReplay);
#ifdef LAMIUM_EDGE_TRACE
    EdgeTraceSend::hook();
    EdgeTraceCorrection::hook();
    EdgeTraceBuild::hook();
    EdgeTraceSurvivalBuild::hook();
    EdgeTraceReplay::hook();
    bool frame = EdgeTraceHistoryFrame::hook() == 0, prepare = EdgeTracePrepare::hook() == 0,
        apply = EdgeTraceApply::hook() == 0, selected = EdgeTraceSelectedInput::hook() == 0;
    Runtime::instance().self().getLogger().info("L-162 history diagnostics: frame {} prepare {} apply {} selected {}", frame, prepare, apply, selected);
    Runtime::instance().self().getLogger().warn("Edge Guard diagnostics enabled (L-160/L-162); up to 16 correction windows and 12 falls");
#endif
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
#ifdef LAMIUM_EDGE_TRACE
    EdgeTraceSelectedInput::unhook(true);
    if (traceInputInstalled) {
        EdgeTraceCapture::unhook(true); EdgeTracePreApply::unhook(true); EdgeTracePostApply::unhook(true);
        traceInputInstalled = false;
    }
    EdgeTraceReplay::unhook(true);
    EdgeTraceHistoryFrame::unhook(true);
    EdgeTracePrepare::unhook(true);
    EdgeTraceApply::unhook(true);
#endif
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
