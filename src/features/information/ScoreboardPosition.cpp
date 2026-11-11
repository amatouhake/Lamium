#include "features/information/ScoreboardPosition.h"
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
#include <memory>
#include <mutex>
#include <utility>

namespace lamium::information::scoreboardPosition {
namespace {
bool installed = false, installedText = false, installedSprite = false;
struct Seen { float width, height, screenW, screenH; };
std::optional<Seen> seen;
std::mutex seenMutex;
std::atomic<bool> warnedClip{false};
struct Frame { float width, height; ui::HudElement element; };
thread_local Frame const* active = nullptr;
// No game references or control changes: layout stays entirely vanilla.
struct RenderScope {
    Frame const* previous = std::exchange(active, nullptr);
    std::optional<Frame> frame;
    ~RenderScope() { active = previous; }
    void prepare(ScreenView const& view) {
        if (!installedText || !installedSprite) return;
        auto& runtime = Runtime::instance();
        auto settings = runtime.snapshot();
        if (!runtime.enabled() || !settings->information.placesScoreboard()) return;
        auto* tree = view.mVisualTree.get();
        if (!tree || !std::string_view(*tree->mRootControlName).ends_with(".hud_screen")) return;
        auto client = ll::service::getClientInstance();
        if (!client || !client->getLocalPlayer()) return;
        DWORD foregroundProcess = 0;
        GetWindowThreadProcessId(GetForegroundWindow(), &foregroundProcess);
        if (foregroundProcess != GetCurrentProcessId()) return;
        if (!versionSensitiveAllowed(capabilities::Id::ScoreboardPosition)) return;
        glm::vec2 size = *view.mSize;
        frame = Frame{size.x, size.y, settings->hud.scoreboard};
        active = &*frame;
    }
};
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
        // The sidebar's parts sit a few panels under it; a clipping panel on
        // the way would cut a moved part, so that leaves the sidebar vanilla.
        auto panel = control.mParent.lock();
        for (int depth = 0; panel && depth < 16; ++depth) {
            if (sidebarControl(*panel->mName)) break;
            if (panel->mClipsChildren) return;
            panel = panel->mParent.lock();
        }
        if (!panel || !sidebarControl(*panel->mName)) return;
        if (panel->mClipsChildren) {
            if (!warnedClip.exchange(true))
                Runtime::instance().self().getLogger().warn("Scoreboard position: the sidebar clips its parts; it stays vanilla");
            return;
        }
        if (panel->mCachedPositionDirty) panel->_setCachedPosition();
        if (control.mCachedPositionDirty) control._setCachedPosition();
        glm::vec2 at = *panel->mCachedPosition, size = *panel->mSize;
        position = *control.mCachedPosition;
        auto next = chatPosition::translated(active->width, active->height, {at.x, at.y, size.x, size.y},
                                             {position.x, position.y}, active->element);
        if (!next) return;
        {
            std::lock_guard lock(seenMutex);
            seen = Seen{size.x, size.y, active->width, active->height};
        }
        // The element's scale: the control's size for this draw, and for a
        // text its font scale and line height (the hook), which keeps the
        // layout's line breaks and spacing (checked in game, L-321).
        scale = chatPosition::zoom(active->element);
        // The part's own size, not the panel's (`size` above).
        this->size = *control.mSize;
        owner = &control;
        control.mCachedPosition = glm::vec2{next->x, next->y};
        if (scale != 1) control.mSize = this->size * scale;
    }
};
}
LL_TYPE_INSTANCE_HOOK(ScoreboardHudRender, ll::memory::HookPriority::Normal, ScreenView,
    &ScreenView::render, void, UIRenderContext& context) {
    RenderScope scope;
    try { scope.prepare(*this); } catch (...) {}
    origin(context);
}
LL_TYPE_INSTANCE_HOOK(ScoreboardTextRender, ll::memory::HookPriority::Normal, TextComponent,
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
LL_TYPE_INSTANCE_HOOK(ScoreboardBackgroundRender, ll::memory::HookPriority::Normal, SpriteComponent,
    &SpriteComponent::render, void, UIRenderContext& context) {
    Translation translation;
    try { translation.prepare(mOwner); } catch (...) {}
    origin(context);
}
std::optional<ui::hud_editor::Box> preview(MinecraftUIRenderContext& context, float width, float height,
                                           ui::HudElement const& element, bool outline) {
    float w = sampleWidth, h = sampleHeight;
    {
        std::lock_guard lock(seenMutex);
        if (seen && seen->screenW == width && seen->screenH == height) { w = seen->width; h = seen->height; }
    }
    auto box = chatPosition::placed(width, height, w, h, element);
    if (!box || outline) return box;
    // Sample only; the gameplay sidebar is still rendered by vanilla.
    float z = chatPosition::zoom(element);
    ui::fill(context, box->x, box->y, box->w, box->h, ui::palette::panel, .5f);
    ui::labelScaled(context, box->x, box->y + z, box->w, ui::translated("scoreboardPosition.sample"), z,
                    ui::palette::text, ui::Align::Center, false);
    int rows = std::min(3, static_cast<int>((box->h - 10 * z) / (10 * z)));
    for (int i = 0; i < rows; ++i) {
        float y = box->y + (11 + 10 * static_cast<float>(i)) * z;
        ui::labelScaled(context, box->x + 2 * z, y, std::max(0.f, box->w - 14 * z), i == 0 ? "Steve" : i == 1 ? "Alex" : "Noor", z,
                        ui::palette::text, ui::Align::Left, false);
        ui::labelScaled(context, box->x, y, std::max(0.f, box->w - 2 * z), std::to_string(12 - 4 * i), z,
                        ui::palette::warning, ui::Align::Right, false);
    }
    context.flushText(0, std::nullopt);
    return box;
}
void start() noexcept {
    capabilities::begin(capabilities::Id::ScoreboardPosition);
    try {
        capabilities::attachHook<ScoreboardHudRender>(installed,{capabilities::Id::ScoreboardPosition});
        capabilities::attachHook<ScoreboardTextRender>(installedText,{capabilities::Id::ScoreboardPosition});
        capabilities::attachHook<ScoreboardBackgroundRender>(installedSprite,{capabilities::Id::ScoreboardPosition});
        capabilities::hooks(capabilities::Id::ScoreboardPosition,installed && installedText && installedSprite);
        if (!installed || !installedText || !installedSprite)
            Runtime::instance().self().getLogger().warn("Scoreboard position cannot hook the HUD components; the scoreboard stays vanilla");
    } catch (...) { capabilities::hooks(capabilities::Id::ScoreboardPosition,false); }
}
void stop() {
    capabilities::stop(capabilities::Id::ScoreboardPosition);
    capabilities::detachHook<ScoreboardHudRender>(installed,{capabilities::Id::ScoreboardPosition});
    capabilities::detachHook<ScoreboardTextRender>(installedText,{capabilities::Id::ScoreboardPosition});
    capabilities::detachHook<ScoreboardBackgroundRender>(installedSprite,{capabilities::Id::ScoreboardPosition});
    std::lock_guard lock(seenMutex);
    seen.reset();
}
}
