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
        return settings->information.debug;
    } catch (...) { return false; }
}
bool hidingCovered(std::atomic<bool> const& covered) noexcept {
    try { return hiding() || (covered.load() && Runtime::instance().enabled()); } catch (...) { return false; }
}
// The doll hidden from Game HUD placement (L-334), while Lamium places it.
bool dollHidden() noexcept {
    try {
        if (!dollPlaced.load()) return false;
        auto& runtime = Runtime::instance();
        return runtime.enabled() && runtime.snapshot()->information.hidesPaperDoll()
            && versionSensitiveAllowed(capabilities::Id::PaperDollPosition);
    } catch (...) { return false; }
}
VanillaLine lineOf(UIControl& owner) noexcept {
    try {
        std::string const& name = *owner.mName;
        return name.size() <= 32 ? vanillaLine(name) : VanillaLine::None;
    } catch (...) { return VanillaLine::None; }
}
bool lineHidden(UIControl& owner) noexcept {
    auto line = lineOf(owner);
    if (line == VanillaLine::None) return false;
    try {
        auto& runtime = Runtime::instance();
        if (!runtime.enabled()) return false;
        auto settings = runtime.snapshot();
        auto const& info = settings->information;
        if (line == VanillaLine::Days) return info.hidesGameDays();
        return info.hidesGamePosition() || hidingCovered(positionCovered);
    } catch (...) { return false; }
}
}
LL_TYPE_INSTANCE_HOOK(DebugPositionSprite, ll::memory::HookPriority::Normal, SpriteComponent,
    &SpriteComponent::render, void, UIRenderContext& context) {
    if (lineHidden(mOwner)) return;
    origin(context);
}
LL_TYPE_INSTANCE_HOOK(DebugPositionText, ll::memory::HookPriority::Normal, TextComponent,
    &TextComponent::$render, void, UIRenderContext& context) {
    if (lineHidden(mOwner)) return;
    origin(context);
}
LL_TYPE_INSTANCE_HOOK(DebugPaperDoll, ll::memory::HookPriority::Normal, HudPlayerRenderer,
    &HudPlayerRenderer::$render, void, MinecraftUIRenderContext& context, IClientInstance& client, UIControl& owner,
    int pass) {
    if (hidingCovered(dollCovered) || dollHidden()) return;
    DollPlacement placement;
    try { placement.prepare(owner); } catch (...) {}
    origin(context, client, owner, pass);
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
            Runtime::instance().self().getLogger().warn("Cannot hide the game's position, days played or paper doll; they stay drawn");
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
