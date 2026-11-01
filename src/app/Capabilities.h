#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <string_view>

namespace lamium::capabilities {
enum class Id {
    FreeCameraTerrain, ConnectedTextures, NightVision, EdgeGuardMovement, EdgeGuardReplay,
    RepeatedTrades, TransferRoutes, HideFog, HideWeather, HidePortal, HideNausea,
    SchematicBlocks, SchematicPiston, SchematicShulker, SchematicEntities, GameLanguage,
    SkipServerPacks, FreeCameraHotbar, FakeOffhandTargets, ChatPosition,
    FreeCameraHand, FreeCameraHud, HideParticles, HideBossBars,
    SchematicBlend, SchematicActorLight, SchematicCreativePick,
    InventoryDiagnostics, LanguageDiagnostics, SneakPlacement, Count
};
enum class Impact { None, Partly, Fully };
enum class Stage : std::uint8_t { Waiting, Ready, Stopped, Failed };
enum class Reason : std::uint8_t {
    None, UnverifiedGame, UnsupportedLoader, HookUnavailable, NativeContract,
    TargetChanged, OperationFailed, RemovalFailed, Stopped
};
struct Definition {
    Id id;
    std::string_view logName, feature, nameKey;
    Impact impact;
    bool gameSensitive = true, direct = false;
    // Empty means the shared verified executable. A reviewed exception is
    // scoped to this one capability, never a global compatibility bypass.
    std::array<unsigned,4> verifiedBuild{};
};
inline constexpr auto definitions = std::to_array<Definition>({
    {Id::FreeCameraTerrain, "FreeCamera terrain (native culler request)", "freecamera", "capability.FreeCameraTerrain", Impact::Partly},
    {Id::ConnectedTextures, "Connected Textures (chunk mesh vertices)", "connectedTextures", "capability.ConnectedTextures", Impact::Fully},
    {Id::NightVision, "Night Vision (light texture data)", "nightVision", "capability.NightVision", Impact::Fully},
    {Id::EdgeGuardMovement, "Edge Guard (movement request)", "edgeGuard", "capability.EdgeGuardMovement", Impact::Fully},
    {Id::EdgeGuardReplay, "Edge Guard (replay capture)", "edgeGuard", "capability.EdgeGuardReplay", Impact::Partly},
    {Id::RepeatedTrades, "Repeated trades (trade row handlers)", "repeatTrades", "capability.RepeatedTrades", Impact::Fully},
    {Id::TransferRoutes, "Item Transfer (native slot routes)", "transfer", "capability.TransferRoutes", Impact::Partly},
    {Id::HideFog, "Hide effects (fog and camera medium)", "hideEffects", "capability.HideFog", Impact::Partly},
    {Id::HideWeather, "Hide effects (weather densities)", "hideEffects", "capability.HideWeather", Impact::Partly},
    {Id::HidePortal, "Hide effects (portal screen)", "hideEffects", "capability.HidePortal", Impact::Partly},
    {Id::HideNausea, "Hide nausea (overlay mesh)", "hideEffects", "capability.HideNausea", Impact::Partly},
    {Id::SchematicBlocks, "Schematic ghost blocks (block tessellation)", "schematic", "capability.SchematicBlocks", Impact::Partly},
    {Id::SchematicPiston, "Schematic piston ghosts (piston actor state)", "schematic", "capability.SchematicPiston", Impact::Partly, true, true},
    {Id::SchematicShulker, "Schematic check (shulker box facing)", "schematic", "capability.SchematicShulker", Impact::Partly, true, true},
    {Id::SchematicEntities, "Schematic entity models (actor renderers)", "schematic", "capability.SchematicEntities", Impact::Partly},
    {Id::GameLanguage, "Game language (native apply)", "gameLanguage", "capability.GameLanguage", Impact::Fully, true, true},
    {Id::SkipServerPacks, "Skip server resource packs (pack stack packet)", "skipServerPacks", "capability.SkipServerPacks", Impact::Fully},
    {Id::FreeCameraHotbar, "FreeCamera hide (hotbar control)", "freecamera", "capability.FreeCameraHotbar", Impact::Partly},
    {Id::FakeOffhandTargets, "Fake Offhand (item use targets)", "fakeOffhand", "capability.FakeOffhandTargets", Impact::Partly},
    {Id::ChatPosition, "Chat position (HUD control positions)", "chatPosition", "capability.ChatPosition", Impact::Fully},
    {Id::FreeCameraHand, "FreeCamera hide (hand pass)", "freecamera", "capability.FreeCameraHand", Impact::Partly, false},
    {Id::FreeCameraHud, "FreeCamera hide (HUD pass)", "freecamera", "capability.FreeCameraHud", Impact::Partly, false},
    {Id::HideParticles, "Hide effects (particle passes)", "hideEffects", "capability.HideParticles", Impact::Partly, false},
    {Id::HideBossBars, "Hide effects (boss bar passes)", "hideEffects", "capability.HideBossBars", Impact::Partly, false},
    {Id::SchematicBlend, "Schematic ghosts (blended pass)", "schematic", "capability.SchematicBlend", Impact::Partly},
    {Id::SchematicActorLight, "Schematic ghosts (actor lighting)", "schematic", "capability.SchematicActorLight", Impact::Partly},
    {Id::SchematicCreativePick, "Schematic pick (creative inventory give)", "schematic", "capability.SchematicCreativePick", Impact::Partly, true, true},
    {Id::InventoryDiagnostics, "Inventory transfer diagnostics", "", "", Impact::None, true, true},
    {Id::LanguageDiagnostics, "Language diagnostics", "", "", Impact::None, true, true},
    {Id::SneakPlacement, "Sneak placement (use gate and sneak state)", "sneakPlacement", "capability.SneakPlacement", Impact::Fully},
});
inline constexpr size_t count = static_cast<size_t>(Id::Count);
static_assert(definitions.size() == count);
static_assert([] {
    for (size_t i = 0; i < count; ++i) {
        auto const& d = definitions[i];
        if (static_cast<size_t>(d.id) != i || d.logName.empty()
            || (!d.feature.empty() && (d.nameKey.empty() || d.impact == Impact::None))) return false;
        for (size_t j = 0; j < i; ++j) if (definitions[j].logName == d.logName) return false;
    }
    return true;
}());
inline constexpr Definition const& definition(Id id) { return definitions[static_cast<size_t>(id)]; }

// One policy for every native adapter. A patch is not a new ABI claim; native
// checks still apply. Minecraft verification is a separate, stricter decision.
inline constexpr int loaderMajor = 26, loaderMinor = 51, minimumLoaderPatch = 5;
struct Environment {
    bool verifiedGame;
    int major, minor, patch;
    std::array<unsigned,4> gameBuild{};
    bool forceUnverified = false;
};
inline constexpr bool loaderSupported(int major, int minor, int patch) {
    return major == loaderMajor && minor == loaderMinor && patch >= minimumLoaderPatch;
}
inline constexpr Reason compatibility(Definition const& d, Environment env) {
    if (!d.gameSensitive) return Reason::None;
    bool verified = d.verifiedBuild[0] ? d.verifiedBuild == env.gameBuild : env.verifiedGame;
    if (env.forceUnverified || !verified) return Reason::UnverifiedGame;
    return loaderSupported(env.major,env.minor,env.patch) ? Reason::None : Reason::UnsupportedLoader;
}
inline constexpr Reason compatibility(Id id, Environment env) { return compatibility(definition(id),env); }
struct RuntimeStatus {
    Stage stage = Stage::Waiting;
    Reason reason = Reason::None;
    bool operator==(RuntimeStatus const&) const = default;
};
static_assert(std::atomic<RuntimeStatus>::is_always_lock_free);
struct Status {
    Reason compatibility = Reason::None;
    RuntimeStatus runtime;
    constexpr Reason reason() const { return compatibility != Reason::None ? compatibility : runtime.reason; }
    constexpr bool available() const { return compatibility == Reason::None && runtime.stage == Stage::Ready; }
    constexpr bool canProbe() const {
        return compatibility == Reason::None && (runtime.stage == Stage::Waiting || runtime.stage == Stage::Ready);
    }
    constexpr bool paused() const { return reason() != Reason::None; }
    constexpr bool waiting() const { return compatibility == Reason::None && runtime.stage == Stage::Waiting; }
};
inline constexpr Impact combine(Impact a, Impact b) { return a > b ? a : b; }
template<class Read>
Impact featureImpact(std::string_view feature, Read read) {
    Impact result = Impact::None;
    for (auto const& d : definitions)
        if (d.feature == feature && !feature.empty() && read(d.id).paused()) result = combine(result,d.impact);
    return result;
}
inline constexpr std::string_view reasonKey(Reason reason) {
    switch (reason) {
    case Reason::UnverifiedGame: return "capability.reason.game";
    case Reason::UnsupportedLoader: return "capability.reason.loader";
    case Reason::HookUnavailable: return "capability.reason.hook";
    case Reason::NativeContract: return "capability.reason.contract";
    case Reason::TargetChanged: return "capability.reason.target";
    case Reason::OperationFailed: return "capability.reason.operation";
    case Reason::RemovalFailed: return "capability.reason.removal";
    case Reason::Stopped: return "capability.reason.stopped";
    default: return {};
    }
}
inline constexpr std::string_view reasonName(Reason reason) {
    switch (reason) {
    case Reason::UnverifiedGame: return "unverified Minecraft executable";
    case Reason::UnsupportedLoader: return "unsupported loader series or below minimum";
    case Reason::HookUnavailable: return "native hook unavailable";
    case Reason::NativeContract: return "native contract check failed";
    case Reason::TargetChanged: return "native target changed";
    case Reason::OperationFailed: return "native operation failed";
    case Reason::RemovalFailed: return "native hook removal failed";
    case Reason::Stopped: return "adapter stopped";
    default: return "none";
    }
}
// Pure state transitions, also exercised by the standalone tests. Atomic
// owned values let render/chunk/network readers query without a mutex.
class Registry {
    Environment environment_;
    std::array<std::atomic<RuntimeStatus>,count> states;
    std::atomic<unsigned> revision_{0};
public:
    explicit Registry(Environment env) : environment_(env) { reset(); }
    void reset() {
        for (auto const& d : definitions) begin(d.id);
        ++revision_;
    }
    Status status(Id id) const { return {compatibility(id,environment_),states[static_cast<size_t>(id)].load()}; }
    unsigned revision() const { return revision_.load(); }
    bool begin(Id id) {
        auto& cell = states[static_cast<size_t>(id)];
        RuntimeStatus next{definition(id).direct ? Stage::Ready : Stage::Waiting,Reason::None};
        auto previous = cell.load();
        do { if (previous.reason == Reason::RemovalFailed || previous == next) return false; }
        while (!cell.compare_exchange_weak(previous,next));
        ++revision_;
        return true;
    }
    bool ready(Id id) {
        auto& cell = states[static_cast<size_t>(id)];
        RuntimeStatus expected{Stage::Waiting,Reason::None};
        if (!cell.compare_exchange_strong(expected,{Stage::Ready,Reason::None})) return false;
        ++revision_;
        return true;
    }
    bool fail(Id id, Reason reason) {
        if (reason == Reason::None || reason == Reason::Stopped) reason = Reason::NativeContract;
        auto& cell = states[static_cast<size_t>(id)];
        auto previous = cell.load();
        do { if (previous.stage == Stage::Failed && (reason != Reason::RemovalFailed || previous.reason == reason)) return false; }
        while (!cell.compare_exchange_weak(previous,{Stage::Failed,reason}));
        ++revision_;
        return true;
    }
    bool stop(Id id) {
        auto& cell = states[static_cast<size_t>(id)];
        auto previous = cell.load();
        do { if (previous.stage == Stage::Failed || previous.stage == Stage::Stopped) return false; }
        while (!cell.compare_exchange_weak(previous,{Stage::Stopped,Reason::Stopped}));
        ++revision_;
        return true;
    }
};
// Game glue lives in Versions.cpp; these functions never keep game pointers.
Registry& registry();
void begin(Id) noexcept;
void ready(Id) noexcept;
void fail(Id, Reason, char const* detail = nullptr) noexcept;
void stop(Id) noexcept;
void hooks(Id, bool available) noexcept;
}
