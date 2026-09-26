#include "features/visuals/HideOffhand.h"
#include "app/Runtime.h"
#include "ll/api/memory/Hook.h"
#include "mc/client/renderer/game/ItemInHandRenderer.h"
#include <stdexcept>
#ifdef LAMIUM_RESEARCH_TRACE
#include "mc/common/Brightness.h"
#include <atomic>
#include <utility>
#include <vector>
#endif

namespace lamium::visuals {
namespace {
bool installed = false;
#ifdef LAMIUM_RESEARCH_TRACE
void logRenderSite(int site, ItemContextFlags flags, int mainHand) noexcept;
#endif
LL_TYPE_INSTANCE_HOOK(OffhandVisibility, ll::memory::HookPriority::Normal, ItemInHandRenderer,
    &ItemInHandRenderer::renderOffhandItem, void, BaseActorRenderContext& context,
    Player& player, ItemContextFlags flags) {
    auto& runtime = Runtime::instance();
    bool firstPerson = (static_cast<unsigned>(flags) & static_cast<unsigned>(ItemContextFlags::FirstPersonPass)) != 0;
    bool otherPass = (static_cast<unsigned>(flags) & (static_cast<unsigned>(ItemContextFlags::WorldPass)
        | static_cast<unsigned>(ItemContextFlags::UIPass))) != 0;
    if (firstPerson && !otherPass && runtime.enabled() && runtime.preferences().visuals.hideOffhand) {
#ifdef LAMIUM_RESEARCH_TRACE
        logRenderSite(0, flags, -1);
#endif
        return;
    }
#ifdef LAMIUM_RESEARCH_TRACE
    // L-14 follow-up: log every renderOffhandItem call that is NOT skipped,
    // to catch a shield drawn through this call with other flags.
    logRenderSite(4, flags, -1);
#endif
    // Never change equipped stacks, item use, or renderer-owned cached items.
    origin(context, player, flags);
}
// L-14: a shield bypasses renderOffhandItem. The 2026-09-27 trace showed it
// reaching renderItem with WorldPass|InHand and renderingMainHand=false while
// the totem never does, so skip that world-anchored offhand render as well.
// The main hand (renderingMainHand=true) and other passes are never skipped.
LL_TYPE_INSTANCE_HOOK(OffhandWorldItem, ll::memory::HookPriority::Normal, ItemInHandRenderer,
    &ItemInHandRenderer::renderItem, void, BaseActorRenderContext& context, Actor& entity,
    ItemStack const& item, bool posAndRotSetByJSON, ItemContextFlags flags, bool useMatrixAsIs,
    bool renderingMainHand) {
    auto& runtime = Runtime::instance();
    bool worldOnly = (static_cast<unsigned>(flags) & static_cast<unsigned>(ItemContextFlags::WorldPass)) != 0
        && (static_cast<unsigned>(flags)
            & (static_cast<unsigned>(ItemContextFlags::FirstPersonPass)
                | static_cast<unsigned>(ItemContextFlags::UIPass))) == 0;
    if (!renderingMainHand && worldOnly && runtime.enabled() && runtime.preferences().visuals.hideOffhand) return;
    origin(context, entity, item, posAndRotSetByJSON, flags, useMatrixAsIs, renderingMainHand);
}
#ifdef LAMIUM_RESEARCH_TRACE
// L-14: which ItemInHandRenderer call draws the shield while Hide Offhand is
// on? Each (call site, flags, main-hand) combination is logged once, so the
// user compares the totem run against the blocking-shield run. Read-only.
void logRenderSite(int site, ItemContextFlags flags, int mainHand) noexcept {
    try {
        auto& runtime = Runtime::instance();
        if (!runtime.enabled() || !runtime.preferences().visuals.hideOffhand) return;
        static std::vector<std::tuple<int, unsigned, int>> seen;
        auto key = std::make_tuple(site, static_cast<unsigned>(flags), mainHand);
        for (auto const& entry : seen) if (entry == key) return;
        if (seen.size() >= 16) return;
        seen.push_back(key);
        runtime.self().getLogger().info(
            "research L-14 render site={} flags={} mainhand={}", site, static_cast<unsigned>(flags), mainHand);
    } catch (...) {}
}
LL_TYPE_INSTANCE_HOOK(OffhandFirstPersonTrace, ll::memory::HookPriority::Low, ItemInHandRenderer,
    &ItemInHandRenderer::renderFirstPerson, void, BaseActorRenderContext& context, Matrix const& prevProj,
    ItemContextFlags flags) {
    logRenderSite(1, flags, -1);
    origin(context, prevProj, flags);
}
LL_TYPE_INSTANCE_HOOK(OffhandItemTrace, ll::memory::HookPriority::Low, ItemInHandRenderer,
    &ItemInHandRenderer::renderItem, void, BaseActorRenderContext& context, Actor& entity,
    ItemStack const& item, bool posAndRotSetByJSON, ItemContextFlags flags, bool useMatrixAsIs,
    bool renderingMainHand) {
    logRenderSite(2, flags, renderingMainHand ? 1 : 0);
    origin(context, entity, item, posAndRotSetByJSON, flags, useMatrixAsIs, renderingMainHand);
}
LL_TYPE_INSTANCE_HOOK(OffhandItemNewTrace, ll::memory::HookPriority::Low, ItemInHandRenderer,
    &ItemInHandRenderer::renderItemNew, void, BaseActorRenderContext& context, Actor& entity,
    ItemStack const& item, ItemContextFlags flags, Brightness lightEmission) {
    // L-14 follow-up: the blocking shield may use the new item pipeline.
    try {
        auto& runtime = Runtime::instance();
        if (runtime.enabled() && runtime.preferences().visuals.hideOffhand) logRenderSite(3, flags, -1);
    } catch (...) {}
    origin(context, entity, item, flags, lightEmission);
}
LL_TYPE_INSTANCE_HOOK(OffhandRenderObjectTrace, ll::memory::HookPriority::Low, ItemInHandRenderer,
    &ItemInHandRenderer::renderObject, void, BaseActorRenderContext& context,
    ItemRenderCall const& renderObject, dragon::RenderMetadata const& renderMetadata, ItemContextFlags flags) {
    // L-14 follow-up: 3D-model items may bypass renderItem through cached
    // render objects.
    try {
        auto& runtime = Runtime::instance();
        if (runtime.enabled() && runtime.preferences().visuals.hideOffhand) logRenderSite(5, flags, -1);
    } catch (...) {}
    origin(context, renderObject, renderMetadata, flags);
}
LL_TYPE_INSTANCE_HOOK(OffhandTessellateTrace, ll::memory::HookPriority::Low, ItemInHandRenderer,
    &ItemInHandRenderer::tessellateAtFrame, void, BaseActorRenderContext& context, Mob* mob,
    ItemStack const& item, int frame) {
    try {
        auto& runtime = Runtime::instance();
        if (runtime.enabled() && runtime.preferences().visuals.hideOffhand)
            logRenderSite(6, ItemContextFlags::None, -1);
    } catch (...) {}
    origin(context, mob, item, frame);
}
struct TraceHook { int (*install)(bool); bool (*remove)(bool); };
TraceHook traceHooks[] = {{OffhandFirstPersonTrace::hook, OffhandFirstPersonTrace::unhook},
    {OffhandItemTrace::hook, OffhandItemTrace::unhook}, {OffhandItemNewTrace::hook, OffhandItemNewTrace::unhook},
    {OffhandRenderObjectTrace::hook, OffhandRenderObjectTrace::unhook},
    {OffhandTessellateTrace::hook, OffhandTessellateTrace::unhook}};
#endif
}
void start() {
    if (installed) return;
    installed = OffhandVisibility::hook(true) == 0;
    if (!installed) throw std::runtime_error("Could not install offhand visibility hook");
    if (OffhandWorldItem::hook(true) != 0) {
        stop();
        throw std::runtime_error("Could not install offhand world-item hook");
    }
#ifdef LAMIUM_RESEARCH_TRACE
    for (auto& hook : traceHooks)
        if (hook.install(true) != 0) throw std::runtime_error("Could not install offhand trace hook");
#endif
}
void stop() {
#ifdef LAMIUM_RESEARCH_TRACE
    for (auto it = std::rbegin(traceHooks); it != std::rend(traceHooks); ++it) it->remove(true);
#endif
    OffhandWorldItem::unhook(true);
    if (installed && OffhandVisibility::unhook(true)) installed = false;
}
}
