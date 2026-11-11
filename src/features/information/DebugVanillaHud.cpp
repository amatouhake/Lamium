#include "features/information/DebugVanillaHud.h"
#include "app/CapabilityHooks.h"
#include "app/Runtime.h"
#include "app/Versions.h"
#include "ll/api/memory/Hook.h"
#include "mc/client/gui/controls/SpriteComponent.h"
#include "mc/client/gui/controls/TextComponent.h"
#include "mc/client/gui/controls/UIControl.h"
#include "mc/client/gui/controls/renderers/HudPlayerRenderer.h"
#include "settings/Settings.h"
#ifdef LAMIUM_DOLL_TRACE
#include "mc/client/gui/screens/ScreenContext.h"
#include "mc/client/renderer/screen/MinecraftUIRenderContext.h"
#include "mc/deps/renderer/Camera.h"
#include "mc/deps/renderer/MatrixStack.h"
#endif
#include <atomic>
#include <string>

namespace lamium::information::debugVanilla {
namespace {
bool installedSprite = false, installedText = false, installedDoll = false;
std::atomic<bool> dollCovered{false}, positionCovered{false};
std::atomic<bool> dollPlaced{false};
std::atomic<float> dollX{0}, dollY{0}, dollScale{1};
// The doll's control is drawn where and as large as the HUD layout says, for
// this one call: its cached position and size are restored at once, without
// invalidating the tree or touching its layout (as Chat position does,
// L-309). The game centers the doll in the control and sizes it by it.
struct DollPlacement {
    UIControl* owner = nullptr;
    glm::vec2 position{}, size{};
    ~DollPlacement() {
        if (!owner) return;
        owner->mCachedPosition = position;
        owner->mSize = size;
    }
    void prepare(UIControl& control) {
        if (!dollPlaced.load()) return;
        auto& runtime = Runtime::instance();
        if (!runtime.enabled() || !runtime.snapshot()->information.placesPaperDoll()) return;
        std::string const& name = *control.mName;
        if (name != "hud_player" || control.mCachedPositionDirty) return;
        if (!versionSensitiveAllowed(capabilities::Id::PaperDollPosition)) return;
        float scale = dollScale.load();
        if (!std::isfinite(scale) || scale < .5f || scale > 2) return;
        glm::vec2 current = *control.mSize;
        glm::vec2 scaled = current * scale;
        auto next = paperDoll::controlPosition({dollX.load(), dollY.load()}, scaled.x, scaled.y);
        if (!next) return;
        position = *control.mCachedPosition;
        size = current;
        owner = &control;
        control.mCachedPosition = glm::vec2{next->x, next->y};
        if (scale != 1) control.mSize = scaled;
    }
};
bool hiding() noexcept {
    try {
        auto& runtime = Runtime::instance();
        if (!runtime.enabled()) return false;
        auto settings = runtime.snapshot();
        return settings->information.debug && settings->information.debugHideVanilla;
    } catch (...) { return false; }
}
bool hidingCovered(std::atomic<bool> const& covered) noexcept {
    try { return hiding() || (covered.load() && Runtime::instance().enabled()); } catch (...) { return false; }
}
bool position(UIControl& owner) noexcept {
    try {
        std::string const& name = *owner.mName;
        return name.size() <= 32 && positionControl(name);
    } catch (...) { return false; }
}
}
LL_TYPE_INSTANCE_HOOK(DebugPositionSprite, ll::memory::HookPriority::Normal, SpriteComponent,
    &SpriteComponent::render, void, UIRenderContext& context) {
    if (position(mOwner) && hidingCovered(positionCovered)) return;
    origin(context);
}
LL_TYPE_INSTANCE_HOOK(DebugPositionText, ll::memory::HookPriority::Normal, TextComponent,
    &TextComponent::$render, void, UIRenderContext& context) {
    if (position(mOwner) && hidingCovered(positionCovered)) return;
    origin(context);
}
LL_TYPE_INSTANCE_HOOK(DebugPaperDoll, ll::memory::HookPriority::Normal, HudPlayerRenderer,
    &HudPlayerRenderer::$render, void, MinecraftUIRenderContext& context, IClientInstance& client, UIControl& owner,
    int pass) {
    if (hidingCovered(dollCovered)) return;
    DollPlacement placement;
    try { placement.prepare(owner); } catch (...) {}
    origin(context, client, owner, pass);
#ifdef LAMIUM_DOLL_TRACE
    // L-309 experiment: three more dolls in the same frame, one per hypothesis.
    try {
        if (owner.mCachedPositionDirty) owner._setCachedPosition();
        glm::vec2 at = *owner.mCachedPosition, size = *owner.mSize;
        auto& stack = *context.mScreenContext.camera.worldMatrixStack;
        static std::atomic<unsigned> logged{0};
        if (pass >= 0 && pass < 8 && !(logged.fetch_or(1u << pass) & (1u << pass)))
            Runtime::instance().self().getLogger().info("Doll trace: pass={} at={},{} size={},{} worldDepth={}", pass, at.x,
                at.y, size.x, size.y, stack.stack->size());
        // Below: the cached position moved down.
        owner.mCachedPosition = at + glm::vec2{0, 70};
        origin(context, client, owner, pass);
        // Right: the control twice as large.
        owner.mCachedPosition = at + glm::vec2{70, 0};
        owner.mSize = size * 2.f;
        origin(context, client, owner, pass);
        owner.mCachedPosition = at;
        owner.mSize = size;
        // Far right: twice as large about the control's corner through the world matrix.
        glm::mat4 model{1.f};
        model[0][0] = model[1][1] = 2;
        model[3] = {at.x + 160 - 2 * at.x, at.y - 2 * at.y, 0, 1};
        auto ref = stack.push(false);
        ref.stack->_isDirty = true;
        ref.mat->_m = ref.mat->_m.get() * model;
        origin(context, client, owner, pass);
        ref.stack->_isDirty = true;
        if (ref.stack->sortOrigin->has_value() && (ref.stack->stack->size() - 1) <= ref.stack->sortOrigin->value())
            ref.stack->sortOrigin->reset();
        ref.stack->stack->pop_back();
        ref.mat = nullptr;
        ref.stack = nullptr;
    } catch (...) {}
#endif
}
void infoCovers(bool doll, bool position) {
    dollCovered = doll;
    positionCovered = position;
}
void placeDoll(std::optional<paperDoll::Point> center, float scale) {
    if (center) { dollX = center->x; dollY = center->y; dollScale = scale; }
    dollPlaced = center.has_value();
}
void start() noexcept {
    capabilities::begin(capabilities::Id::PaperDollPosition);
    try {
        if (!installedSprite) installedSprite = DebugPositionSprite::hook(true) == 0;
        if (!installedText) installedText = DebugPositionText::hook(true) == 0;
        if (!installedDoll) installedDoll = DebugPaperDoll::hook(true) == 0;
        if (!installedSprite || !installedText || !installedDoll)
            Runtime::instance().self().getLogger().warn("Debug View cannot hide the game's position or paper doll; they stay drawn");
        capabilities::hooks(capabilities::Id::PaperDollPosition, installedDoll);
    } catch (...) { capabilities::hooks(capabilities::Id::PaperDollPosition, false); }
}
void stop() {
    capabilities::stop(capabilities::Id::PaperDollPosition);
    dollPlaced = false;
    dollCovered = positionCovered = false;
    if (installedDoll && DebugPaperDoll::unhook(true)) installedDoll = false;
    if (installedText && DebugPositionText::unhook(true)) installedText = false;
    if (installedSprite && DebugPositionSprite::unhook(true)) installedSprite = false;
}
}
