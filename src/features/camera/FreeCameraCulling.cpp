#include "features/camera/FreeCameraCulling.h"
#include "features/camera/CameraSessions.h"
#include "app/Runtime.h"
#include "ll/api/Versions.h"
#include "ll/api/memory/Hook.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/client/renderer/game/LevelRenderer.h"
#include "mc/client/renderer/game/LevelRendererPlayer.h"
#include "mc/legacy/ActorRuntimeID.h"
#include "mc/world/phys/AABB.h"
#include <mutex>
#include <optional>
#include <Windows.h>
#include <atomic>
#include <vector>
#pragma comment(lib, "version.lib")

namespace lamium::camera {
namespace {
std::atomic<bool> enabled{false}, verified{false};
std::atomic<DWORD> rendererThread{0};
std::uintptr_t cullerTarget = 0;
bool renderInstalled = false, cullerInstalled = false, bodyInstalled = false;
// The body's box while FreeCamera runs (L-123). Kept visible to the camera's
// culling so the body still renders off screen: the inventory's player model
// reuses the pose (worn elytra) that only the world render of the body updates.
std::mutex bodyMutex;
std::optional<AABB> bodyBox;
std::atomic<bool> bodyLogged{false}, pointLogged{false}, statsLogged{false};
std::atomic<int> boxCalls{0}, boxOverlaps{0}, pointCalls{0}, pointOverlaps{0};
// Round 2 (2026-10-10): which visibility test the body goes through, once.
void logStats() {
    if (boxCalls + pointCalls < 2000 || statsLogged.exchange(true)) return;
    try {
        Runtime::instance().self().getLogger().info(
            "FreeCamera body: isAABBVisible {} calls ({} near the body), cullerIsVisible {} calls ({} near the body)",
            boxCalls.load(), boxOverlaps.load(), pointCalls.load(), pointOverlaps.load());
    } catch (...) {}
}
std::atomic<bool> warned{false}, appliedLogged{false};

void unavailable(char const* reason) noexcept {
    enabled = false;
    if (warned.exchange(true)) return;
    try { Runtime::instance().self().getLogger().warn("FreeCamera terrain: vanilla visibility retained ({})", reason); }
    catch (...) {}
}

bool supportedGameVersion() {
    wchar_t path[32768];
    DWORD length = GetModuleFileNameW(nullptr, path, 32768);
    if (!length || length == 32768) return false;
    DWORD bytes = GetFileVersionInfoSizeW(path, nullptr);
    if (!bytes || bytes > 1024 * 1024) return false;
    std::vector<std::byte> data(bytes);
    if (!GetFileVersionInfoW(path, 0, bytes, data.data())) return false;
    VS_FIXEDFILEINFO* info = nullptr;
    UINT size = 0;
    if (!VerQueryValueW(data.data(), L"\\", reinterpret_cast<void**>(&info), &size)
        || size < sizeof(*info) || info->dwSignature != 0xfeef04bd) return false;
    return info->dwFileVersionMS == ((1u << 16) | 26u)
        && info->dwFileVersionLS == ((51u << 16) | 1u);
}

bool gameCode(void* address) {
    MEMORY_BASIC_INFORMATION page{};
    if (!VirtualQuery(address, &page, sizeof(page)) || page.State != MEM_COMMIT
        || page.AllocationBase != GetModuleHandleW(nullptr)
        || (page.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
    auto protection = page.Protect & 0xff;
    return protection == PAGE_EXECUTE || protection == PAGE_EXECUTE_READ
        || protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY;
}

LL_TYPE_INSTANCE_HOOK(TerrainCullerRequest, ll::memory::HookPriority::Normal, LevelRendererCameraListeners,
    cullerTarget, void, LevelCullerType requested) {
    bool own = false, free = false;
    try {
        if (enabled && GetCurrentThreadId() == rendererThread.load()) {
            auto& renderer = static_cast<LevelRenderer&>(mLevelRenderer);
            std::shared_ptr<LevelRendererPlayer> const& primary = renderer.mLevelRendererPlayer;
            own = primary && static_cast<LevelRendererCamera*>(primary.get()) == static_cast<LevelRendererCamera*>(this);
            auto& client = static_cast<IClientInstance&>(renderer.mClientInstance);
            auto* player = own ? client.getLocalPlayer() : nullptr;
            free = player && player->hasRuntimeID()
                && CameraSessions::instance().freeCameraFor(client, player->getRuntimeID().rawID);
        }
    } catch (...) { unavailable("native request ownership could not be verified"); }
    auto native = static_cast<std::uint8_t>(requested);
    auto selected = terrainCuller(native, free && enabled.load() && verified.load());
    origin(static_cast<LevelCullerType>(selected));
    if (!own || !enabled || native != 3) return;
    // Observe the unmodified call first; never guess an ABI from the slot alone.
    if (static_cast<std::uint8_t>(static_cast<LevelCullerType const&>(mLastCullerType)) != selected) {
        unavailable("native culler update did not retain its requested type");
        return;
    }
    verified = true;
    if (selected != native && !appliedLogged.exchange(true)) {
        try { Runtime::instance().self().getLogger().info("FreeCamera terrain: native request 3 -> 5 retained"); }
        catch (...) {}
    }
}

bool bindCuller(LevelRendererPlayer& camera) {
    static auto const index = ll::memory::getVtableIndex(&LevelRendererCamera::updateLevelCullerType);
    if (!index || *index > 64) { unavailable("unsupported virtual-call thunk"); return false; }
    auto* base = static_cast<LevelRendererCamera*>(&camera);
    auto** table = *reinterpret_cast<void***>(base);
    void* target = table[*index];
    if (!gameCode(target)) { unavailable("culler entry is outside game executable code"); return false; }
    auto address = reinterpret_cast<std::uintptr_t>(target);
    if (cullerInstalled) {
        if (address != cullerTarget) { unavailable("renderer culler implementation changed"); return false; }
        return true;
    }
    // Hook the implementation obtained from the SDK virtual method, not a
    // missing exported thunk or a hard-coded slot/address.
    if (cullerTarget && cullerTarget != address) { unavailable("culler entry changed after restart"); return false; }
    cullerTarget = address;
    if (TerrainCullerRequest::hook(true) != 0) { unavailable("culler hook unavailable"); return false; }
    cullerInstalled = true;
    Runtime::instance().self().getLogger().info("FreeCamera terrain: culler hook bound at virtual slot {}", *index);
    return true;
}

LL_TYPE_INSTANCE_HOOK(BodyVisibility, ll::memory::HookPriority::Normal, LevelRendererCamera,
    &LevelRendererCamera::isAABBVisible, bool, AABB const& box, bool useFastCulling) {
    bool visible = origin(box, useFastCulling);
    // Any thread: entity queuing may run off the render thread.
    std::lock_guard lock{bodyMutex};
    if (!bodyBox) return visible;
    auto const& body = *bodyBox;
    bool overlaps = box.min.x <= body.max.x && box.max.x >= body.min.x && box.min.y <= body.max.y
        && box.max.y >= body.min.y && box.min.z <= body.max.z && box.max.z >= body.min.z;
    ++boxCalls;
    if (overlaps) ++boxOverlaps;
    logStats();
    if (visible || !overlaps) return visible;
    if (!bodyLogged.exchange(true))
        try { Runtime::instance().self().getLogger().info("FreeCamera body: kept visible to culling off screen (box)"); } catch (...) {}
    return true;
}
LL_TYPE_INSTANCE_HOOK(BodyPointVisibility, ll::memory::HookPriority::Normal, LevelRendererCamera,
    &LevelRendererCamera::cullerIsVisible, bool, Vec3 const& point, float radius) {
    bool visible = origin(point, radius);
    std::lock_guard lock{bodyMutex};
    if (!bodyBox) return visible;
    auto const& body = *bodyBox;
    auto clamp = [](float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; };
    float dx = point.x - clamp(point.x, body.min.x, body.max.x), dy = point.y - clamp(point.y, body.min.y, body.max.y),
          dz = point.z - clamp(point.z, body.min.z, body.max.z);
    bool overlaps = dx * dx + dy * dy + dz * dz <= radius * radius;
    ++pointCalls;
    if (overlaps) ++pointOverlaps;
    logStats();
    if (visible || !overlaps) return visible;
    if (!pointLogged.exchange(true))
        try { Runtime::instance().self().getLogger().info("FreeCamera body: kept visible to culling off screen (point)"); } catch (...) {}
    return true;
}

LL_TYPE_INSTANCE_HOOK(TerrainPreRender, ll::memory::HookPriority::Normal, LevelRenderer,
    &LevelRenderer::preRenderUpdate, void,
    ScreenContext& context, LevelRenderPreRenderUpdateParameters& parameters) {
    // Bind from the real primary renderer. Later native requests may come
    // from another render stage; only this render thread can override them.
    try {
        std::shared_ptr<LevelRendererPlayer> const& camera = mLevelRendererPlayer;
        auto& client = static_cast<IClientInstance&>(mClientInstance);
        auto* player = client.getLocalPlayer();
        bool free = enabled && camera && player && player->hasRuntimeID()
            && CameraSessions::instance().freeCameraFor(client, player->getRuntimeID().rawID);
        if (free) {
            rendererThread = GetCurrentThreadId();
            bindCuller(*camera);
        }
        std::lock_guard lock{bodyMutex};
        if (free) bodyBox = player->getAABB();
        else bodyBox.reset();
    } catch (...) { unavailable("renderer binding failed"); }
    origin(context, parameters);
}
}

void startTerrainCulling() noexcept {
    if (renderInstalled || cullerInstalled) { unavailable("previous terrain hooks could not be removed"); return; }
    warned = false;
    appliedLogged = false;
    verified = false;
    try {
        auto loader = ll::getLoaderVersion();
        Runtime::instance().self().getLogger().info("FreeCamera terrain: detected LeviLamina {}.{}.{}",
            loader.major, loader.minor, loader.patch);
        if (!terrainLoaderSupported(loader.major, loader.minor, loader.patch)) {
            unavailable("unsupported loader version"); return;
        }
        if (!supportedGameVersion()) { unavailable("unverified game executable version"); return; }
        if (TerrainPreRender::hook(true) != 0) { unavailable("pre-render hook unavailable"); return; }
        renderInstalled = true;
        bodyLogged = false;
        pointLogged = false;
        statsLogged = false;
        bodyInstalled = BodyVisibility::hook(true) == 0 && BodyPointVisibility::hook(true) == 0;
        if (!bodyInstalled) Runtime::instance().self().getLogger().warn("FreeCamera body: culling hook unavailable");
        enabled = true;
        Runtime::instance().self().getLogger().info("FreeCamera terrain: adapter armed; awaiting FreeCamera renderer");
    } catch (...) { unavailable("initialization failed"); }
}

void stopTerrainCulling() noexcept {
    enabled = false;
    rendererThread = 0;
    {
        std::lock_guard lock{bodyMutex};
        bodyBox.reset();
    }
    try {
        if (bodyInstalled && BodyVisibility::unhook(true) && BodyPointVisibility::unhook(true)) bodyInstalled = false;
    } catch (...) {}
    try {
        if (renderInstalled) {
            if (TerrainPreRender::unhook(true)) renderInstalled = false;
            else unavailable("pre-render hook removal failed");
        }
    } catch (...) { unavailable("pre-render hook removal failed"); }
    try {
        if (cullerInstalled) {
            if (TerrainCullerRequest::unhook(true)) cullerInstalled = false;
            else unavailable("culler hook removal failed");
        }
    } catch (...) { unavailable("culler hook removal failed"); }
}
}
