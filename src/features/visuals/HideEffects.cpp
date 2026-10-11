#include "features/visuals/HideEffects.h"
#include "features/visuals/EffectVisibility.h"
#include "app/Runtime.h"
#include "app/Versions.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/service/TargetedBedrock.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/options/IOptionRegistry.h"
#include "mc/options/GraphicsMode.h"
#include "mc/client/gui/controls/SpriteComponent.h"
#include "mc/client/gui/controls/TextComponent.h"
#include "mc/client/gui/controls/UIControl.h"
#include "mc/client/gui/screens/InGamePlayScreen.h"
#include "mc/client/particle/ParticleEngine.h"
#include "mc/client/particle/Particle.h"
#include "mc/client/particlesystem/particle/ParticleEmitterActual.h"
#include "mc/client/particlesystem/particle/ParticleRenderer.h"
#include "mc/client/renderer/game/LevelRendererPlayer.h"
#include "mc/client/renderer/game/LevelRendererCamera.h"
#include "mc/deps/minecraft_renderer/objects/ViewRenderObject.h"
#include "mc/deps/core/renderer/RenderMaterialInfo.h"
#include "mc/deps/core/resource/ResourceLocation.h"
#include "mc/deps/minecraft_renderer/renderer/MaterialPtr.h"
#include "mc/deps/minecraft_renderer/renderer/Mesh.h"
#include "mc/deps/minecraft_renderer/renderer/TexturePtr.h"
#include "mc/deps/minecraft_renderer/resources/ClientTexture.h"
#include "mc/deps/minecraft_renderer/resources/ServerTexture.h"
#include "mc/deps/minecraft_renderer/framebuilder/dragon/RenderMetadata.h"
#include <atomic>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>

namespace lamium::visuals::effects {
namespace {
std::atomic<unsigned> configured{0}, available{0};
std::atomic<std::shared_ptr<std::string const>> rainEffectName;
static_assert(static_cast<int>(WeatherRenderObject::PrecipitationType::Rain) == 0
    && static_cast<int>(WeatherRenderObject::PrecipitationType::Snow) == 1
    && static_cast<int>(WeatherRenderObject::PrecipitationType::Plankton) == 2
    && static_cast<int>(WeatherRenderObject::PrecipitationType::RedSpores) == 3
    && static_cast<int>(WeatherRenderObject::PrecipitationType::BlueSpores) == 4
    && static_cast<int>(WeatherRenderObject::PrecipitationType::Ash) == 5
    && static_cast<int>(WeatherRenderObject::PrecipitationType::WhiteAsh) == 6
    && static_cast<int>(WeatherRenderObject::PrecipitationType::Count) == 7);
bool weatherInstalled = false, legacyInstalled = false, dataInstalled = false;
bool rainLegacyInstalled = false, rainMappingInstalled = false, rainDataInstalled = false;
bool bossSpriteInstalled = false, bossTextInstalled = false;
bool screenInstalled = false, nauseaMeshInstalled = false, nauseaMetadataInstalled = false;
bool fogInstalled = false;
// One log line the first time each effect's draw route is hidden: runtime
// evidence of which observed route a switch actually reached.
std::atomic<unsigned> reported{0};
void reportReached(unsigned bit, std::string_view detail) noexcept {
    try {
        if (reported.fetch_or(bit) & bit) return;
        Runtime::instance().self().getLogger().info("Hide effects: route {} reached ({})", bit, detail);
    } catch (...) {}
}
thread_local unsigned screenMask = 0;
struct ScreenScope {
    unsigned previous = screenMask;
    explicit ScreenScope(unsigned mask) { screenMask = mask; }
    ~ScreenScope() { screenMask = previous; }
};
unsigned active() noexcept {
    if (!Runtime::instance().enabled()) return 0;
    return configured.load() & available.load();
}
LL_TYPE_INSTANCE_HOOK(EffectLocalScreen, ll::memory::HookPriority::Normal, InGamePlayScreen,
    &InGamePlayScreen::$render, void, ScreenContext& context, FrameRenderObject const& frame) {
    unsigned mask = 0;
    try {
        auto current = ll::service::getClientInstance();
        if (current && mClient.get().get() == current.as_ptr() && current->getLocalPlayer()) mask = active();
    } catch (...) {}
    ScreenScope scope{mask};
    origin(context,frame);
}
using MeshTexture = std::variant<std::monostate,mce::TexturePtr,mce::ClientTexture,mce::ServerTexture>;
bool overlayMesh(mce::MaterialPtr const& material, MeshTexture const& texture) noexcept {
    try {
        // Every mesh passes here; test the screen scope before Runtime state.
        if (!(screenMask & overlayBits)) return false;
        auto mask = screenMask & active();
        if (!(mask & overlayBits)) return false;
        auto* pointer = std::get_if<mce::TexturePtr>(&texture);
        auto const& info = material.mRenderMaterialInfoPtr;
        if (!pointer || !pointer->mResourceLocationPtr || !info) return false;
        auto const& name = info->mHashedName->getString();
        Core::PathBuffer<std::string> const& path = pointer->mResourceLocationPtr->mPath;
        if (name.size() > 192 || path.value.size() > 192) return false;
        if (!hideOverlayMesh(mask,true,name,path.value)) return false;
        reportReached(overlayMeshBit(name,path.value), name + " " + path.value);
        return true;
    } catch (...) { return false; }
}
using MeshRender = void (mce::Mesh::*)(mce::MeshContext&, mce::MaterialPtr const&, MeshTexture const&,
    uint, uint, OffscreenCaptureDescription const&, mce::IndexBufferContainer const*) const;
using MetadataMeshRender = void (mce::Mesh::*)(mce::MeshContext&, dragon::RenderMetadata const&,
    mce::MaterialPtr const&, MeshTexture const&, uint, uint, mce::IndexBufferContainer const*) const;
LL_TYPE_INSTANCE_HOOK(NauseaMeshVisibility, ll::memory::HookPriority::Normal, mce::Mesh,
    static_cast<MeshRender>(&mce::Mesh::renderMesh), void, mce::MeshContext& context,
    mce::MaterialPtr const& material, MeshTexture const& texture, uint startOffset, uint count,
    OffscreenCaptureDescription const& capture, mce::IndexBufferContainer const* indices) {
    if (overlayMesh(material,texture) && versionSensitiveAllowed("Hide nausea (overlay mesh)")) return;
    origin(context,material,texture,startOffset,count,capture,indices);
}
LL_TYPE_INSTANCE_HOOK(NauseaMetadataVisibility, ll::memory::HookPriority::Normal, mce::Mesh,
    static_cast<MetadataMeshRender>(&mce::Mesh::renderMesh), void, mce::MeshContext& context,
    dragon::RenderMetadata const& metadata, mce::MaterialPtr const& material, MeshTexture const& texture,
    uint startOffset, uint count, mce::IndexBufferContainer const* indices) {
    if (overlayMesh(material,texture) && versionSensitiveAllowed("Hide nausea (overlay mesh)")) return;
    origin(context,metadata,material,texture,startOffset,count,indices);
}
bool bossControl(UIControl& owner) noexcept {
    try {
        BossBarRoute route;
        UIControl* control = &owner;
        std::shared_ptr<UIControl> parent;
        for (unsigned depth = 0; depth < 32 && control; ++depth) {
            std::string const& name = *control->mName;
            if (name.size() > 192) return false;
            if (route.visit(name)) return true;
            if (name == "hud_screen") return false;
            parent = control->mParent.lock();
            control = parent.get();
        }
    } catch (...) {}
    return false;
}
LL_TYPE_INSTANCE_HOOK(BossBarSpriteVisibility, ll::memory::HookPriority::Normal, SpriteComponent,
    &SpriteComponent::render, void, UIRenderContext& context) {
    if ((active() & bossBarsBit) && bossControl(mOwner)) return;
    origin(context);
}
LL_TYPE_INSTANCE_HOOK(BossBarTextVisibility, ll::memory::HookPriority::Normal, TextComponent,
    &TextComponent::$render, void, UIRenderContext& context) {
    if ((active() & bossBarsBit) && bossControl(mOwner)) return;
    origin(context);
}
LL_STATIC_HOOK(LegacyParticleVisibility, ll::memory::HookPriority::Normal,
    &ParticleEngine::render, void, ScreenContext& context, ParticleLayerRenderObject const& particles) {
    if (active() & particlesBit) return;
    origin(context, particles);
}
LL_TYPE_INSTANCE_HOOK(DataParticleVisibility, ll::memory::HookPriority::Normal, ParticleRenderer,
    &ParticleRenderer::renderParticles, void, ScreenContext& context, Vec3 const& target,
    Vec3 const& camera, ParticleRenderData const& particles) {
    if (active() & particlesBit) return;
    origin(context, target, camera, particles);
}
LL_TYPE_INSTANCE_HOOK(RainParticleVisibility, ll::memory::HookPriority::Normal, Particle,
    &Particle::$tessellate, void, ParticleRenderContext const& context) {
    if (hideParticle(active(),mType == ParticleType::RainSplash)) return;
    origin(context);
}
LL_TYPE_INSTANCE_HOOK(RainEffectMapping, ll::memory::HookPriority::Normal, ParticleEngine,
    &ParticleEngine::_emitParticleNew, void, ParticleSystemEngine& engine, ParticleType type,
    Vec3 const& pos, Vec3 const& direction, int data) {
    if (type == ParticleType::RainSplash) {
        try {
            // Learn the current pack's rain identifier from the game's own
            // mapping, rather than guessing names or suppressing WaterSplash.
            auto found = mNewParticleSystemJsonLookup->find(type);
            auto previous = rainEffectName.load();
            bool sharedWithWater = false;
            if (found != mNewParticleSystemJsonLookup->end()) {
                for (auto other : {ParticleType::WaterSplash, ParticleType::WaterSplashManual, ParticleType::WaterWake}) {
                    auto water = mNewParticleSystemJsonLookup->find(other);
                    if (water != mNewParticleSystemJsonLookup->end()
                        && water->second.getString() == found->second.getString()) sharedWithWater = true;
                }
            }
            if (found == mNewParticleSystemJsonLookup->end() || found->second.empty()
                || found->second.getString().size() > 192 || sharedWithWater) rainEffectName.store(nullptr);
            else if (!previous || *previous != found->second.getString())
                rainEffectName.store(std::make_shared<std::string const>(found->second.getString()));
        } catch (...) { rainEffectName.store(nullptr); }
    }
    origin(engine,type,pos,direction,data);
}
LL_TYPE_INSTANCE_HOOK(RainEmitterVisibility, ll::memory::HookPriority::Normal, ParticleSystem::ParticleEmitterActual,
    &ParticleSystem::ParticleEmitterActual::$extractForRendering, void, ParticleRenderData& particles, float alpha) {
    if (active() & weatherBit) {
        try {
            auto rain = rainEffectName.load();
            if (rain && rainEffectMatches(*rain,mEffectName->getString())) return;
        } catch (...) {}
    }
    origin(particles,alpha);
}
// Vanilla blends its distance fog from last frame's resolved value, so the far
// value written after setup is swapped back for vanilla's before the next one.
// The owner is an identity only and is never dereferenced.
struct HeldFog { std::uintptr_t owner = 0; FogRange vanilla; };
HeldFog heldFog;
void hideDistanceFog(LevelRendererPlayer& self, unsigned mask, CameraMedium shown) noexcept {
    try {
        // Vibrant Visuals (graphics mode Advanced and up) keeps vanilla fog.
        bool vibrant = static_cast<int>(self.mClientInstance.getOptions().getGraphicsMode())
            >= static_cast<int>(GraphicsMode::Advanced);
        if (!hidesDistanceFog(mask, shown, vibrant)) return;
        auto& fog = *self.mCurrentDistanceFog;
        FogRange vanilla{fog.mStart, fog.mEnd};
        auto far = farFog(vanilla);
        if (!far) return;
        heldFog = {reinterpret_cast<std::uintptr_t>(&self), vanilla};
        fog.mStart = far->start;
        fog.mEnd = far->end;
        reportReached(distanceFogBit, "distance fog");
    } catch (...) {}
}
// Fog setup reads the camera-medium flags; for a hidden medium it sees the
// camera outside it, so vanilla resolves its own air or weather fog. The flags
// are restored before anything else can read them.
LL_TYPE_INSTANCE_HOOK(MediumFogVisibility, ll::memory::HookPriority::Normal, LevelRendererPlayer,
    &LevelRendererPlayer::$setupFog, void, ScreenContext& context, float intensity) {
    unsigned mask = 0;
    CameraMedium before{};
    try {
        if (heldFog.owner == reinterpret_cast<std::uintptr_t>(this)) {
            mCurrentDistanceFog->mStart = heldFog.vanilla.start;
            mCurrentDistanceFog->mEnd = heldFog.vanilla.end;
        }
        heldFog.owner = 0;
        before = {mCameraUnderWater, mCameraUnderLiquid, mCameraUnderLava, mCameraUnderPowderSnow};
        if (ll::service::getClientInstance() == &mClientInstance) mask = active();
        // Fog values and the camera-medium flags are written in place.
        if (mask && !versionSensitiveAllowed("Hide effects (fog and camera medium)")) mask = 0;
    } catch (...) { mask = 0; }
    auto shown = visibleMedium(before, mask & mediumBits);
    if (!(mask & mediumBits) || shown == before) {
        origin(context, intensity);
        hideDistanceFog(*this, mask, shown);
        return;
    }
    mCameraUnderWater = shown.water;
    mCameraUnderLiquid = shown.liquid;
    mCameraUnderLava = shown.lava;
    mCameraUnderPowderSnow = shown.powderSnow;
    struct Restore {
        LevelRendererPlayer& self; CameraMedium medium;
        ~Restore() {
            self.mCameraUnderWater = medium.water;
            self.mCameraUnderLiquid = medium.liquid;
            self.mCameraUnderLava = medium.lava;
            self.mCameraUnderPowderSnow = medium.powderSnow;
        }
    } restore{*this, before};
    reportReached((before.water && !shown.water ? waterBit : 0) | (before.lava && !shown.lava ? lavaBit : 0)
        | (before.powderSnow && !shown.powderSnow ? powderSnowBit : 0) , "fog medium");
    origin(context, intensity);
    hideDistanceFog(*this, mask, shown);
}
LL_TYPE_INSTANCE_HOOK(WeatherVisibility, ll::memory::HookPriority::Normal, LevelRendererPlayer,
    &LevelRendererPlayer::createViewRenderObject, ViewRenderObject, ScreenContext& context, SubClientId id) {
    auto view = origin(context, id);
    try {
        unsigned mask = active();
        if (!mask || ll::service::getClientInstance() != &mClientInstance) return view;
        if (!versionSensitiveAllowed("Hide effects (weather densities)")) return view;
        auto& weather = *view.mWeatherState;
        float* densities[] = {&weather.mDensityRain, &weather.mDensitySnow, &weather.mDensityPlankton,
            &weather.mDensityRedSpores, &weather.mDensityBlueSpores, &weather.mDensityAsh, &weather.mDensityWhiteAsh};
        auto hidden = hiddenWeatherLayers((mask & weatherBit) != 0, (mask & particlesBit) != 0);
        // Validate the whole snapshot before changing it. Never alter rain
        // updates: those also feed sound and splash emission.
        for (size_t i = 0; i < hidden.size(); ++i)
            if (!std::isfinite(*densities[i]) || *densities[i] < 0
                || !std::isfinite((*weather.mParams)[i].fAlpha)) return view;
        for (size_t i = 0; i < hidden.size(); ++i) {
            if (!hidden[i]) continue;
            *densities[i] = 0;
            (*weather.mParams)[i].fAlpha = 0;
        }
    } catch (...) {}
    return view;
}
}
void configure(Settings const& settings) {
    auto const& v = settings.visuals;
    configured = effectMask(v.hideEffects, EffectSelection{v.hideWeather, v.hideParticles, v.hideBossBars,
        v.hideNausea, v.hideWater, v.hideLava, v.hidePowderSnow, v.hideDistanceFog});
}
void start() noexcept {
    try {
        if (!weatherInstalled) weatherInstalled = WeatherVisibility::hook(true) == 0;
        if (!legacyInstalled) legacyInstalled = LegacyParticleVisibility::hook(true) == 0;
        if (!dataInstalled) dataInstalled = DataParticleVisibility::hook(true) == 0;
        if (!rainLegacyInstalled) rainLegacyInstalled = RainParticleVisibility::hook(true) == 0;
        if (!rainMappingInstalled) rainMappingInstalled = RainEffectMapping::hook(true) == 0;
        if (!rainDataInstalled) rainDataInstalled = RainEmitterVisibility::hook(true) == 0;
        if (!bossSpriteInstalled) bossSpriteInstalled = BossBarSpriteVisibility::hook(true) == 0;
        if (!bossTextInstalled) bossTextInstalled = BossBarTextVisibility::hook(true) == 0;
        if (!screenInstalled) screenInstalled = EffectLocalScreen::hook(true) == 0;
        if (!nauseaMeshInstalled) nauseaMeshInstalled = NauseaMeshVisibility::hook(true) == 0;
        if (!nauseaMetadataInstalled) nauseaMetadataInstalled = NauseaMetadataVisibility::hook(true) == 0;
        if (!fogInstalled) fogInstalled = MediumFogVisibility::hook(true) == 0;
        bool weatherReady = weatherInstalled && rainLegacyInstalled && rainMappingInstalled && rainDataInstalled;
        bool nauseaReady = screenInstalled && nauseaMeshInstalled && nauseaMetadataInstalled;
        available = (weatherReady ? weatherBit : 0)
            | (weatherInstalled && legacyInstalled && dataInstalled ? particlesBit : 0)
            | (bossSpriteInstalled && bossTextInstalled ? bossBarsBit : 0)
            | (nauseaReady ? nauseaBit : 0)
            | (fogInstalled ? waterBit | lavaBit | distanceFogBit : 0)
            // Powder snow hides both its fog and its freezing overlay, or neither.
            | (fogInstalled && nauseaReady ? powderSnowBit : 0);
        if (!weatherReady || !legacyInstalled || !dataInstalled || !bossSpriteInstalled || !bossTextInstalled || !nauseaReady
            || !fogInstalled)
            Runtime::instance().self().getLogger().warn("Some effect visibility hooks are unavailable; affected effects stay vanilla");
    } catch (...) { available = 0; }
}
void stop() {
    available = 0;
    rainEffectName.store(nullptr);
    heldFog = {};
    if (fogInstalled && MediumFogVisibility::unhook(true)) fogInstalled = false;
    if (nauseaMetadataInstalled && NauseaMetadataVisibility::unhook(true)) nauseaMetadataInstalled = false;
    if (nauseaMeshInstalled && NauseaMeshVisibility::unhook(true)) nauseaMeshInstalled = false;
    if (screenInstalled && EffectLocalScreen::unhook(true)) screenInstalled = false;
    if (bossTextInstalled && BossBarTextVisibility::unhook(true)) bossTextInstalled = false;
    if (bossSpriteInstalled && BossBarSpriteVisibility::unhook(true)) bossSpriteInstalled = false;
    if (rainDataInstalled && RainEmitterVisibility::unhook(true)) rainDataInstalled = false;
    if (rainMappingInstalled && RainEffectMapping::unhook(true)) rainMappingInstalled = false;
    if (rainLegacyInstalled && RainParticleVisibility::unhook(true)) rainLegacyInstalled = false;
    if (dataInstalled && DataParticleVisibility::unhook(true)) dataInstalled = false;
    if (legacyInstalled && LegacyParticleVisibility::unhook(true)) legacyInstalled = false;
    if (weatherInstalled && WeatherVisibility::unhook(true)) weatherInstalled = false;
}
}
