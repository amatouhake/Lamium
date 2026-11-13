#include "features/information/BossBarPosition.h"
#include "features/information/ChatPosition.h"
#include "app/Runtime.h"
#include "app/Versions.h"
#include "app/CapabilityHooks.h"
#include "ui/Localization.h"
#include "ui/Widgets.h"
#include "ll/api/memory/Hook.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/gui/controls/UIControl.h"
#include "mc/client/gui/controls/SpriteComponent.h"
#include "mc/client/gui/controls/TextComponent.h"
#include "mc/client/gui/controls/VisualTree.h"
#include "mc/client/gui/screens/ScreenView.h"
#include "mc/client/renderer/screen/MinecraftUIRenderContext.h"
#include "ll/api/service/TargetedBedrock.h"
#include <Windows.h>
#include <atomic>
#include <format>
#include <mutex>
#include <utility>
#include <vector>

namespace lamium::information::bossBarPosition {
namespace {
bool installed = false, installedText = false, installedSprite = false;
struct Seen { float width, height, screenW, screenH; };
std::optional<Seen> seen;
std::mutex seenMutex;
std::atomic<bool> warnedClip{false}, loggedVanilla{false};
struct Frame {
    float width, height;
    ui::HudElement element;
    // Measured from the grid by the first part drawn this frame, so every
    // part of the frame moves with the same box.
    UIControl const* grid = nullptr;
    std::optional<ui::hud_editor::Box> box;
};
thread_local Frame* active = nullptr;
bool shown(UIControl const& control) { return (static_cast<int>(control.mVisible) & 1) != 0; }
// No game references kept or control changes: layout stays entirely vanilla.
struct RenderScope {
    Frame* previous = std::exchange(active, nullptr);
    std::optional<Frame> frame;
    ~RenderScope() { active = previous; }
    void prepare(ScreenView const& view) {
        if (!installedText || !installedSprite) return;
        auto& runtime = Runtime::instance();
        auto settings = runtime.snapshot();
        if (!runtime.enabled() || !settings->information.placesBossBars()) return;
        auto* tree = view.mVisualTree.get();
        if (!tree || !std::string_view(*tree->mRootControlName).ends_with(".hud_screen")) return;
        auto client = ll::service::getClientInstance();
        if (!client || !client->getLocalPlayer()) return;
        DWORD foregroundProcess = 0;
        GetWindowThreadProcessId(GetForegroundWindow(), &foregroundProcess);
        if (foregroundProcess != GetCurrentProcessId()) return;
        if (!versionSensitiveAllowed(capabilities::Id::BossBarPosition)) return;
        glm::vec2 size = *view.mSize;
        frame = Frame{size.x, size.y, settings->hud.bossBars};
        active = &*frame;
    }
};
std::optional<ui::hud_editor::Box> measure(UIControl& grid) {
    if (grid.mCachedPositionDirty) grid._setCachedPosition();
    glm::vec2 at = *grid.mCachedPosition, size = *grid.mSize;
    std::vector<Span> items;
    for (auto const& child : *grid.mChildren) {
        if (!child || !shown(*child)) continue;
        if (child->mCachedPositionDirty) child->_setCachedPosition();
        float top = child->mCachedPosition->y;
        items.push_back({top, top + child->mSize->y});
    }
    return occupied({at.x, at.y, size.x, size.y}, items);
}
// A component render occurs after native layout. Restore its cache immediately,
// without invalidating the tree or overwriting positions computed by re-layout.
struct Translation {
    UIControl* owner = nullptr;
    glm::vec2 position{}, size{};
    float scale = 1;
    ~Translation() {
        if (!owner) return;
        owner->mCachedPosition = position;
        owner->mSize = size;
    }
    void prepare(UIControl& control) {
        if (!active) return;
        // The bars' parts sit a few panels under the grid; a clipping panel
        // on the way would cut a moved part, so that leaves the bars vanilla.
        auto panel = control.mParent.lock();
        for (int depth = 0; panel && depth < 16; ++depth) {
            if (gridControl(*panel->mName)) break;
            if (panel->mClipsChildren) return;
            panel = panel->mParent.lock();
        }
        if (!panel || !gridControl(*panel->mName)) return;
        if (panel->mClipsChildren) {
            if (!warnedClip.exchange(true))
                Runtime::instance().self().getLogger().warn("Boss bar position: the grid clips its bars; they stay vanilla");
            return;
        }
        if (active->grid != panel.get()) {
            active->grid = panel.get();
            active->box = measure(*panel);
            if (active->box) {
                std::lock_guard lock(seenMutex);
                seen = Seen{active->box->w, active->box->h, active->width, active->height};
            }
            // Where the game puts its bars, to check the default place against.
            if (active->box && !loggedVanilla.exchange(true))
                Runtime::instance().self().getLogger().info(std::format(
                    "Boss bar position: the game's bars at {:.1f},{:.1f} size {:.1f}x{:.1f} on a {:.1f}x{:.1f} screen",
                    active->box->x, active->box->y, active->box->w, active->box->h, active->width, active->height));
        }
        if (!active->box) return;
        if (control.mCachedPositionDirty) control._setCachedPosition();
        position = *control.mCachedPosition;
        auto next = chatPosition::translated(active->width, active->height, *active->box,
                                             {position.x, position.y}, active->element);
        if (!next) return;
        scale = chatPosition::zoom(active->element);
        this->size = *control.mSize;
        owner = &control;
        control.mCachedPosition = glm::vec2{next->x, next->y};
        if (scale != 1) control.mSize = this->size * scale;
    }
};
}
LL_TYPE_INSTANCE_HOOK(BossBarHudRender, ll::memory::HookPriority::Normal, ScreenView,
    &ScreenView::render, void, UIRenderContext& context) {
    RenderScope scope;
    try { scope.prepare(*this); } catch (...) {}
    origin(context);
}
LL_TYPE_INSTANCE_HOOK(BossBarTextRender, ll::memory::HookPriority::Normal, TextComponent,
    &TextComponent::$render, void, UIRenderContext& context) {
    Translation translation;
    try { translation.prepare(mOwner); } catch (...) {}
    float const font = mFontScaleFactor, line = mLineHeight;
    bool const scaled = translation.owner && translation.scale != 1;
    if (scaled) {
        mFontScaleFactor = font * translation.scale;
        mLineHeight = line * translation.scale;
    }
    origin(context);
    if (scaled) {
        mFontScaleFactor = font;
        mLineHeight = line;
    }
}
LL_TYPE_INSTANCE_HOOK(BossBarSpriteRender, ll::memory::HookPriority::Normal, SpriteComponent,
    &SpriteComponent::render, void, UIRenderContext& context) {
    Translation translation;
    try { translation.prepare(mOwner); } catch (...) {}
    origin(context);
}
std::optional<ui::hud_editor::Box> preview(MinecraftUIRenderContext& context, float width, float height,
                                           ui::HudElement const& element, bool outline) {
    float w = barWidth, h = barHeight;
    {
        std::lock_guard lock(seenMutex);
        if (seen && seen->screenW == width && seen->screenH == height) { w = seen->width; h = seen->height; }
    }
    auto box = chatPosition::placed(width, height, w, h, element);
    if (!box || outline) return box;
    // Sample only; the bars in play are still drawn by the game.
    float z = chatPosition::zoom(element);
    int bars = std::max(1, static_cast<int>(box->h / (barHeight * z) + .5f));
    // The game's own bar textures, tinted as its pink and purple bars are
    // (sampled from a screenshot); the first one part empty.
    constexpr ui::Rgb tints[] = {{.86f, .1f, .86f}, {.42f, .24f, .66f}};
    for (int i = 0; i < bars; ++i) {
        float y = box->y + barHeight * z * static_cast<float>(i);
        ui::labelScaled(context, box->x, y, box->w, ui::translated("bossBarPosition.sample"), z,
                        ui::palette::text, ui::Align::Center, true);
        ui::Rgb tint = tints[i % 2];
        float filled = i % 2 ? 1.f : .85f;
        ui::ImageRect bar{box->x, y + 10 * z, box->w, 5 * z};
        ui::imageUv(context, "textures/ui/empty_progress_bar", bar, 0, 0, 1, 1, 1, tint);
        ui::imageUv(context, "textures/ui/filled_progress_bar", {bar.x, bar.y, bar.w * filled, bar.h}, 0, 0, filled, 1, 1, tint);
    }
    context.flushText(0, std::nullopt);
    return box;
}
void start() noexcept {
    capabilities::begin(capabilities::Id::BossBarPosition);
    try {
        capabilities::attachHook<BossBarHudRender>(installed, {capabilities::Id::BossBarPosition});
        capabilities::attachHook<BossBarTextRender>(installedText, {capabilities::Id::BossBarPosition});
        capabilities::attachHook<BossBarSpriteRender>(installedSprite, {capabilities::Id::BossBarPosition});
        capabilities::hooks(capabilities::Id::BossBarPosition, installed && installedText && installedSprite);
        if (!installed || !installedText || !installedSprite)
            Runtime::instance().self().getLogger().warn("Boss bar position cannot hook the HUD components; the boss bars stay vanilla");
    } catch (...) { capabilities::hooks(capabilities::Id::BossBarPosition, false); }
}
void stop() {
    capabilities::stop(capabilities::Id::BossBarPosition);
    capabilities::detachHook<BossBarHudRender>(installed, {capabilities::Id::BossBarPosition});
    capabilities::detachHook<BossBarTextRender>(installedText, {capabilities::Id::BossBarPosition});
    capabilities::detachHook<BossBarSpriteRender>(installedSprite, {capabilities::Id::BossBarPosition});
    std::lock_guard lock(seenMutex);
    seen.reset();
}
}
