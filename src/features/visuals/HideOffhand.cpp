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
    return runtime.enabled() && runtime.preferences().visuals.hideOffhand;
}
// Flat offhand items (totems, maps, ...) go through the item-in-hand renderer.
LL_TYPE_INSTANCE_HOOK(OffhandVisibility, ll::memory::HookPriority::Normal, ItemInHandRenderer,
    &ItemInHandRenderer::renderOffhandItem, void, BaseActorRenderContext& context,
    Player& player, ItemContextFlags flags) {
    bool firstPerson = (static_cast<unsigned>(flags) & static_cast<unsigned>(ItemContextFlags::FirstPersonPass)) != 0;
    bool otherPass = (static_cast<unsigned>(flags) & (static_cast<unsigned>(ItemContextFlags::WorldPass)
        | static_cast<unsigned>(ItemContextFlags::UIPass))) != 0;
    if (firstPerson && !otherPass && hidden()) return;
    // Never change equipped stacks, item use, or renderer-owned cached items.
    origin(context, player, flags);
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
struct Hook { int (*install)(bool); bool (*remove)(bool); };
Hook hooks[] = {{OffhandVisibility::hook, OffhandVisibility::unhook}, {OffhandAttachable::hook, OffhandAttachable::unhook},
    {OffhandAttachableNoChecks::hook, OffhandAttachableNoChecks::unhook}};
}
void start() {
    if (installed) return;
    for (auto& hook : hooks)
        if (hook.install(true) != 0) {
            stop();
            throw std::runtime_error("Could not install offhand visibility hook");
        }
    installed = true;
}
void stop() {
    for (auto it = std::rbegin(hooks); it != std::rend(hooks); ++it) it->remove(true);
    installed = false;
}
}
