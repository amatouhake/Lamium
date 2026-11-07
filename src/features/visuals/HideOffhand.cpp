#include "features/visuals/HideOffhand.h"
#include "app/Runtime.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/service/TargetedBedrock.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/model/models/DataDrivenModel.h"
#include "mc/client/options/IOptionRegistry.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/client/renderer/game/ItemInHandRenderer.h"
#include "mc/world/actor/animation/AttachableSlotIndex.h"
#include <stdexcept>

namespace lamium::visuals {
namespace {
bool installed = false;
bool hidden() {
    auto& runtime = Runtime::instance();
    return runtime.enabled() && runtime.snapshot()->visuals.hideOffhand;
}
thread_local int firstPersonDepth = 0, itemDepth = 0;
struct Depth {
    int& value;
    explicit Depth(int& depth) : value(depth) { ++value; }
    ~Depth() { --value; }
};
// The first-person pass draws the main hand through renderItem and a flat
// offhand item (a totem, ...) straight through renderObject, so a render object
// outside renderItem during that pass is the off hand (L-305, 26.60).
LL_TYPE_INSTANCE_HOOK(OffhandFirstPerson, ll::memory::HookPriority::Normal, ItemInHandRenderer,
    &ItemInHandRenderer::renderFirstPerson, void, BaseActorRenderContext& context, Matrix const& previous, ItemContextFlags flags) {
    Depth depth(firstPersonDepth);
    origin(context, previous, flags);
}
LL_TYPE_INSTANCE_HOOK(OffhandItem, ll::memory::HookPriority::Normal, ItemInHandRenderer,
    &ItemInHandRenderer::renderItem, void, BaseActorRenderContext& context, Actor& entity, ItemStack const& item,
    bool posAndRotSetByJson, ItemContextFlags flags, bool useMatrixAsIs, bool renderingMainHand) {
    Depth depth(itemDepth);
    origin(context, entity, item, posAndRotSetByJson, flags, useMatrixAsIs, renderingMainHand);
}
LL_TYPE_INSTANCE_HOOK(OffhandVisibility, ll::memory::HookPriority::Normal, ItemInHandRenderer,
    &ItemInHandRenderer::renderObject, void, BaseActorRenderContext& context, ItemRenderCall const& call,
    dragon::RenderMetadata const& metadata, ItemContextFlags flags) {
    bool firstPerson = (static_cast<unsigned>(flags) & static_cast<unsigned>(ItemContextFlags::FirstPersonPass)) != 0;
    if (firstPersonDepth > 0 && itemDepth == 0 && firstPerson && hidden()) return;
    // Never change equipped stacks, item use, or renderer-owned cached items.
    origin(context, call, metadata, flags);
}
// A map held in the off hand is drawn by its own first-person calls, outside
// renderObject (L-305, 26.60, traced 2026-11-07): the map, then the arm
// holding it.
LL_TYPE_INSTANCE_HOOK(OffhandMap, ll::memory::HookPriority::Normal, ItemInHandRenderer,
    &ItemInHandRenderer::_renderPhotoMapItem, void, BaseActorRenderContext& context, Player& player,
    float const frameAlpha, bool const isMainHand) {
    if (!isMainHand && hidden()) return;
    origin(context, player, frameAlpha, isMainHand);
}
LL_TYPE_INSTANCE_HOOK(OffhandMapHand, ll::memory::HookPriority::Normal, ItemInHandRenderer,
    &ItemInHandRenderer::_renderMiniMapHand, void, BaseActorRenderContext& context, Player& player, bool inOffhand) {
    if (inOffhand && hidden()) return;
    origin(context, player, inOffhand);
}
// Attachables (the shield and other resource-pack 3D items) are drawn by the
// data-driven model instead (L-14, verified 2026-09-27). Skip the local
// player's offhand slot only while the view is first person, so other
// players and the third-person views keep it.
bool skipAttachable(AttachableSlotIndex slot, Actor& actor) {
    if (slot != AttachableSlotIndex::OffhandItem || !hidden()) return false;
    auto client = ll::service::getClientInstance();
    if (!client || client->getLocalPlayer() != &actor) return false;
    try { return client->getOptions().getPlayerViewPerspective() == 0; } catch (...) { return false; }
}
LL_TYPE_INSTANCE_HOOK(OffhandAttachable, ll::memory::HookPriority::Normal, DataDrivenModel,
    &DataDrivenModel::renderAttachable, void, ItemStack const& item, AttachableSlotIndex const& slot,
    RenderParams& params, Actor& actor) {
    if (skipAttachable(slot, actor)) return;
    origin(item, slot, params, actor);
}
LL_TYPE_INSTANCE_HOOK(OffhandAttachableNoChecks, ll::memory::HookPriority::Normal, DataDrivenModel,
    &DataDrivenModel::renderAttachableNoChecks, void, ItemStack const& item, AttachableSlotIndex const& slot,
    RenderParams& params, Actor& actor) {
    if (skipAttachable(slot, actor)) return;
    origin(item, slot, params, actor);
}
struct Hook { int (*install)(bool); bool (*remove)(bool); bool installed = false; };
Hook hooks[] = {
    {OffhandFirstPerson::hook, OffhandFirstPerson::unhook}, {OffhandItem::hook, OffhandItem::unhook},
    {OffhandVisibility::hook, OffhandVisibility::unhook}, {OffhandAttachable::hook, OffhandAttachable::unhook},
    {OffhandAttachableNoChecks::hook, OffhandAttachableNoChecks::unhook}, {OffhandMap::hook, OffhandMap::unhook},
    {OffhandMapHand::hook, OffhandMapHand::unhook}};
}
void start() {
    if (installed) return;
    for (auto& hook : hooks) if (!hook.installed) {
        if (hook.install(true) != 0) {
            stop();
            throw std::runtime_error("Could not install offhand visibility hook");
        }
        hook.installed = true;
    }
    installed = true;
}
void stop() {
    // Remove only what this start installed, newest first.
    for (auto it = std::rbegin(hooks); it != std::rend(hooks); ++it)
        if (it->installed && it->remove(true)) it->installed = false;
    installed = false;
}
}
