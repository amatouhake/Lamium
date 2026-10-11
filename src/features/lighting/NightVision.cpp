#include "features/lighting/NightVision.h"
#include "app/Runtime.h"
#include "app/Versions.h"
#include "ll/api/memory/Hook.h"
#include "mc/client/renderer/ptexture/BaseLightData.h"
#include "mc/client/renderer/ptexture/BaseLightTextureImageBuilder.h"
#include "mc/client/world/level/dimension/NetherLightTextureImageBuilder.h"

namespace lamium {
namespace {
LL_TYPE_INSTANCE_HOOK(BaseLightHook, ll::memory::HookPriority::Normal, BaseLightTextureImageBuilder,
    &BaseLightTextureImageBuilder::$createBaseLightTextureData, std::unique_ptr<BaseLightData>,
    IClientInstance* client, BaseLightData const& previous) {
    auto data = origin(client, previous);
    NightVision::instance().apply(data.get());
    return data;
}
LL_TYPE_INSTANCE_HOOK(NetherLightHook, ll::memory::HookPriority::Normal, NetherLightTextureImageBuilder,
    &NetherLightTextureImageBuilder::$createBaseLightTextureData, std::unique_ptr<BaseLightData>,
    IClientInstance* client, BaseLightData const& previous) {
    auto data = origin(client, previous);
    NightVision::instance().apply(data.get());
    return data;
}
bool baseInstalled = false;
bool netherInstalled = false;
}
NightVision& NightVision::instance() { static NightVision instance; return instance; }
bool NightVision::start() {
    if (running) return true;
    try {
        if (!baseInstalled) baseInstalled = BaseLightHook::hook() == 0;
        if (!baseInstalled) { stop(); return false; }
        if (!netherInstalled) netherInstalled = NetherLightHook::hook() == 0;
        if (!netherInstalled) { stop(); return false; }
        running = true;
        return true;
    } catch (std::exception const& error) {
        Runtime::instance().self().getLogger().error("Lighting initialization failed: {}", error.what());
        stop();
        return false;
    }
}
void NightVision::stop() {
    running = false;
    if (netherInstalled && NetherLightHook::unhook()) netherInstalled = false;
    if (baseInstalled && BaseLightHook::unhook()) baseInstalled = false;
    if (baseInstalled || netherInstalled)
        Runtime::instance().self().getLogger().error("Could not remove a lighting hook");
}
void NightVision::apply(BaseLightData* data) const {
    if (!data || !enabled() || !versionSensitiveAllowed("Night Vision (light texture data)")) return;
    // Modify newly produced render data so the game's light-texture cache sees
    // the change on both enable and disable. No player effect is added.
    data->mNightvisionActive = true;
    data->mNightvisionScale = 1.0f;
    if (data->mUnderwaterVision) data->mUnderwaterScale = 1.0f;
    data->mDarkenWorldAmount = 0.0f;
    data->mPreviousDarkenWorldAmount = 0.0f;
    data->mDarknessFactor = 0.0f;
    data->mDarknessFactorPreviousFrame = 0.0f;
    // A low Brightness darkens the light curve's low end, which Night Vision
    // shows as dark rings around light sources (L-121). The texture is built
    // with full gamma; the Brightness option itself stays as the player set it.
    float gamma = data->mGamma;
    if (gamma != loggedGamma.exchange(gamma))
        try { Runtime::instance().self().getLogger().info("Night Vision: light gamma {} ({})", gamma, even.load() ? "even -> 1" : "vanilla"); } catch (...) {}
    if (even.load() && gamma < 1.0f) data->mGamma = 1.0f;
}
}
