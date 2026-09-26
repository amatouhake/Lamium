#include "features/visuals/HideOffhand.h"
#include "app/Runtime.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/service/TargetedBedrock.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/item/ItemStack.h"
#include "mc/client/renderer/game/ItemInHandRenderer.h"
#include <array>
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
bool hideOn() noexcept {
    try { return Runtime::instance().enabled() && Runtime::instance().preferences().visuals.hideOffhand; }
    catch (...) { return false; }
}
#ifdef LAMIUM_RESEARCH_TRACE
#include <atomic>
// L-14: bounded per-decision log (the combo-once log cannot show dynamics).
void traceDecision(char const* what, int a, int b) noexcept {
    try {
        static std::atomic<unsigned> budget{};
        if (budget.fetch_add(1) >= 24) return;
        Runtime::instance().self().getLogger().info("research L-14 decision {} a={} b={}", what, a, b);
    } catch (...) {}
}
#else
inline void traceDecision(char const*, int, int) noexcept {}
#endif
// L-14: 3D-model items (shield) reach renderObject, which carries neither the
// item nor the hand. Map each frame's render calls back to their items in
// getRenderCallAtFrame, then skip the offhand one. Entries are rebuilt on
// every renderFirstPerson entry; a call shared by both hands is never skipped.
std::array<std::pair<ItemRenderCall const*, bool>, 4> frameCalls{};
std::size_t frameCallCount = 0;
bool isOffhandStack(ItemStack const& item) {
    auto client = ll::service::getClientInstance();
    auto* player = client ? client->getLocalPlayer() : nullptr;
    if (!player || item.isNull()) return false;
    auto const& offhand = player->getOffhandSlot();
    return !offhand.isNull() && item.matchesItem(offhand);
}
LL_TYPE_INSTANCE_HOOK(OffhandFrameReset, ll::memory::HookPriority::Normal, ItemInHandRenderer,
    &ItemInHandRenderer::renderFirstPerson, void, BaseActorRenderContext& context, Matrix const& prevProj,
    ItemContextFlags flags) {
    frameCallCount = 0;
    origin(context, prevProj, flags);
}
LL_TYPE_INSTANCE_HOOK(OffhandCallMap, ll::memory::HookPriority::Normal, ItemInHandRenderer,
    &ItemInHandRenderer::getRenderCallAtFrame, ItemRenderCall const&, BaseActorRenderContext& context,
    ItemStack const& item, int frame) {
    auto const& call = origin(context, item, frame);
    try {
        if (!hideOn()) return call;
        bool offhand = isOffhandStack(item);
        traceDecision("callmap", offhand ? 1 : 0, static_cast<int>(frameCallCount));
        for (std::size_t i = 0; i < frameCallCount; ++i)
            if (frameCalls[i].first == &call) {
                frameCalls[i].second = frameCalls[i].second && offhand;
                return call;
            }
        if (frameCallCount < frameCalls.size()) frameCalls[frameCallCount++] = {&call, offhand};
    } catch (...) {}
    return call;
}
LL_TYPE_INSTANCE_HOOK(OffhandRenderObject, ll::memory::HookPriority::Normal, ItemInHandRenderer,
    &ItemInHandRenderer::renderObject, void, BaseActorRenderContext& context,
    ItemRenderCall const& renderObject, dragon::RenderMetadata const& renderMetadata, ItemContextFlags flags) {
    bool firstPerson = (static_cast<unsigned>(flags) & static_cast<unsigned>(ItemContextFlags::FirstPersonPass)) != 0;
    bool otherPass = (static_cast<unsigned>(flags) & (static_cast<unsigned>(ItemContextFlags::WorldPass)
        | static_cast<unsigned>(ItemContextFlags::UIPass))) != 0;
    if (firstPerson && !otherPass && hideOn()) {
        bool known = false;
        for (std::size_t i = 0; i < frameCallCount; ++i)
            if (frameCalls[i].first == &renderObject) {
                known = true;
                traceDecision(frameCalls[i].second ? "robject-skip" : "robject-pass-main", 0, 0);
                if (frameCalls[i].second) return;
                break;
            }
        if (!known) traceDecision("robject-pass-unknown", 0, static_cast<int>(frameCallCount));
    }
    origin(context, renderObject, renderMetadata, flags);
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
struct Hook { int (*install)(bool); bool (*remove)(bool); };
Hook hooks[] = {{OffhandVisibility::hook, OffhandVisibility::unhook}, {OffhandWorldItem::hook, OffhandWorldItem::unhook},
    {OffhandFrameReset::hook, OffhandFrameReset::unhook}, {OffhandCallMap::hook, OffhandCallMap::unhook},
    {OffhandRenderObject::hook, OffhandRenderObject::unhook}};
void start() {
    if (installed) return;
    for (auto& hook : hooks)
        if (hook.install(true) != 0) {
            stop();
            throw std::runtime_error("Could not install offhand visibility hook");
        }
    installed = true;
#ifdef LAMIUM_RESEARCH_TRACE
    for (auto& hook : traceHooks)
        if (hook.install(true) != 0) throw std::runtime_error("Could not install offhand trace hook");
#endif
}
void stop() {
#ifdef LAMIUM_RESEARCH_TRACE
    for (auto it = std::rbegin(traceHooks); it != std::rend(traceHooks); ++it) it->remove(true);
#endif
    for (auto it = std::rbegin(hooks); it != std::rend(hooks); ++it) it->remove(true);
    installed = false;
}
}
