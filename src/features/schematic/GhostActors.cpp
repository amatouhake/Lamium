#include "features/schematic/GhostActors.h"
#include "features/schematic/GhostCommon.h"
#include "features/schematic/GhostRenderer.h"
#include "features/schematic/SchematicRegion.h"
#include "ll/api/memory/Hook.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/renderer/ActorShaderManager.h"
#include "mc/client/renderer/BaseActorRenderContext.h"
#include "mc/client/renderer/RenderMaterialGroup.h"
#include "mc/deps/minecraft_renderer/framebuilder/dragon/RenderMetadata.h"
#include "mc/deps/minecraft_renderer/renderer/TexturePtr.h"
#include "mc/deps/minecraft_renderer/resources/ClientTexture.h"
#include "mc/deps/minecraft_renderer/resources/ServerTexture.h"
#include "mc/client/renderer/blockactor/BlockActorRenderDispatcher.h"
#include "mc/dataloadhelper/DefaultDataLoadHelper.h"
#include "mc/deps/minecraft_renderer/renderer/MaterialPtr.h"
#include "mc/deps/nbt/CompoundTag.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/ILevel.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/BrightnessPair.h"
#include "mc/world/level/block/actor/BlockActor.h"
#include "mc/world/level/block/actor/VanillaBlockActorFactory.h"
#include "mc/world/level/block/actor/PistonBlockActor.h"
#include "mc/world/level/block/actor/BlockActorType.h"
#include "app/Versions.h"
#include "app/CapabilityHooks.h"
#include <format>
#include <set>
#include <atomic>

namespace lamium::schematic::ghosts {
namespace {
bool shaderInstalled = false, lightPairInstalled = false;
// The block source ghost block actors draw against, while they draw: their
// renderers light themselves from it (GhostActorShader turns that into full
// brightness), so a ghost chest or bed reads the same at night and
// underground as the other ghosts.
std::atomic<BlockSource const*> ghostActorSource{nullptr};

// An extended piston ghost drew retracted (L-178, 2026-10-14). What load
// left in the actor is logged once per kind, and the file's progress and
// state are put back where they differ; the head is drawn from them.
void keepPistonState(PistonBlockActor& piston, nbt::Compound const& data) {
    float progress = 0, last = 0;
    std::int64_t state = 0, next = 0;
    auto number = [&](char const* key, float& out) {
        if (auto const* tag = data.find(key); tag && tag->as<float>()) { out = *tag->as<float>(); return true; }
        return false;
    };
    auto integer = [&](char const* key, std::int64_t& out) { auto const* tag = data.find(key); return tag && tag->integer(out); };
    bool known = number("Progress", progress) & number("LastProgress", last) & integer("State", state) & integer("NewState", next);
    auto was = std::format("file {}/{} state {}/{}, loaded {}/{} state {}/{}", progress, last, state, next,
                           static_cast<float>(piston.mProgress), static_cast<float>(piston.mLastProgress),
                           static_cast<int>(static_cast<PistonState>(piston.mState)),
                           static_cast<int>(static_cast<PistonState>(piston.mNewState)));
    static std::set<std::string> seen;
    if (seen.size() < 16 && seen.insert(was).second) log("piston ghost: " + was);
    if (!known || state < 0 || state > 3 || next < 0 || next > 3) return;
    if (!versionSensitiveAllowed(capabilities::Id::SchematicPiston)) return;
    piston.mProgress = progress;
    piston.mLastProgress = last;
    piston.mState = static_cast<PistonState>(state);
    piston.mOldState = static_cast<PistonState>(state);
    piston.mNewState = static_cast<PistonState>(next);
}
// Loads the file's block entity data into a ghost's block actor: at its
// world cell, a skull's rotation and a shulker box's opening turned with the
// placement (bed parts and standing banners and signs turn through their
// block states).
void loadBlockEntity(BlockActor& actor, nbt::Compound data, BlockPos const& pos, Placement const& placement, BlockSource& region) {
    data.set("x", {std::int32_t{pos.x}});
    data.set("y", {std::int32_t{pos.y}});
    data.set("z", {std::int32_t{pos.z}});
    if (auto* rotation = data.find("Rotation"); rotation && rotation->as<float>())
        data.set("Rotation", {toWorldYaw(*rotation->as<float>(), placement)});
    auto const* id = data.find("id");
    if (auto* facing = data.find("facing"); facing && facing->as<std::int8_t>() && id && id->as<std::string>()
        && *id->as<std::string>() == "ShulkerBox")
        data.set("facing", {static_cast<std::int8_t>(toWorldFace(*facing->as<std::int8_t>(), placement))});
    nbt::Root root;
    root.compound = std::move(data);
    auto tag = CompoundTag::fromBinaryNbt(nbt::write(root));
    if (!tag) return;
    DefaultDataLoadHelper helper;
    actor.load(region.getILevel(), *tag, helper);
    if (actor.getType() == BlockActorType::PistonArm) keepPistonState(static_cast<PistonBlockActor&>(actor), root.compound);
}

BrightnessPair fullBrightness() {
    BrightnessPair full;
    full.sky->mValue = 15;
    full.block->mValue = 15;
    return full;
}
// Block-actor renderers set up their light from the block source and cell;
// for a ghost's source, the fully bright setup the other ghosts use. (Not a
// hook on BlockSource::getLightColor: its Brightness argument travels by
// value in the game but by reference in this SDK's type, and that hook
// crashed at start; getBrightnessPair is not what these renderers read.)
LL_TYPE_STATIC_HOOK(GhostActorShader, ll::memory::HookPriority::Normal, ActorShaderManager, &ActorShaderManager::setupShaderParameters,
                    void, ScreenContext& screen, BlockSource& source, BlockPos const& pos, float a, bool ignoreLighting,
                    LightTexture& lightTexture, std::weak_ptr<LightPropagation::LightVolumeManager> const& lightVolumes,
                    Vec2 const& uvScale, Vec4 const& uvAnim, shaderpacking::BlockLightingFlags flags) {
    if (&source == ghostActorSource.load(std::memory_order_relaxed) && versionSensitiveAllowed(capabilities::Id::SchematicActorLight)) {
        ActorShaderManager::setupShaderParameters(screen, source, fullBrightness(), glm::vec4{1, 1, 1, 1}, 1.f, true, lightTexture, uvScale, uvAnim);
        return;
    }
    origin(screen, source, pos, a, ignoreLighting, lightTexture, lightVolumes, uvScale, uvAnim, flags);
}
// Chests and beds compute their light first and pass it in.
LL_TYPE_STATIC_HOOK(GhostActorLightPair, ll::memory::HookPriority::Normal, ActorShaderManager, &ActorShaderManager::setupShaderParameters,
                    void, ScreenContext& screen, BlockSource& source, BrightnessPair const& light, glm::vec4 const& blockLightColor,
                    float a, bool ignoreLighting, LightTexture& lightTexture, Vec2 const& uvScale, Vec4 const& uvAnim) {
    if (&source == ghostActorSource.load(std::memory_order_relaxed) && versionSensitiveAllowed(capabilities::Id::SchematicActorLight)) {
        origin(screen, source, fullBrightness(), glm::vec4{1, 1, 1, 1}, 1.f, true, lightTexture, uvScale, uvAnim);
        return;
    }
    origin(screen, source, light, blockLightColor, a, ignoreLighting, lightTexture, uvScale, uvAnim);
}
}

std::shared_ptr<BlockActor> makeBlockActor(Block const& block, BlockPos const& pos, nbt::Compound const* data, Placement const& placement,
                                           BlockSource& region) {
    auto actor = VanillaBlockActorFactory::createBlockActor(pos, block.getBlockType());
    if (actor && data) loadBlockEntity(*actor, *data, pos, placement, region);
    return actor;
}
void renderBlockActor(BaseActorRenderContext& context, SchematicRegion& view, BlockActor& actor, Block const& block, Vec3 const& renderPos,
                      BlockPos const& worldPos) {
    auto* component = actor._getRenderComponent();
    if (!component) return;
    auto& dispatcher = context.mClientInstance.getBlockEntityRenderDispatcher();
    mce::MaterialPtr none(mce::RenderMaterialGroup::common(), HashedString{"lamium_no_forced_material"});
    // Its renderer lights it from this source: full brightness (GhostActorShader).
    ghostActorSource.store(&view, std::memory_order_relaxed);
    struct Restore { ~Restore() { ghostActorSource.store(nullptr, std::memory_order_relaxed); } } restore;
    dispatcher.render(context, view, *component, block, renderPos, worldPos, false, none, nullptr, 0, std::nullopt);
}
bool startActorLight() {
    bool shader = capabilities::attachHook<GhostActorShader>(shaderInstalled,{capabilities::Id::SchematicActorLight});
    bool pair = capabilities::attachHook<GhostActorLightPair>(lightPairInstalled,{capabilities::Id::SchematicActorLight});
    return shader && pair;
}
void stopActorLight() {
    capabilities::detachHook<GhostActorShader>(shaderInstalled,{capabilities::Id::SchematicActorLight});
    capabilities::detachHook<GhostActorLightPair>(lightPairInstalled,{capabilities::Id::SchematicActorLight});
}
}
