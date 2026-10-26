#include "features/map/WaypointMarkers.h"
#include "features/camera/FreeCameraHide.h"
#include "features/camera/CameraSessions.h"
#include "features/map/WaypointSession.h"
#include "features/map/Waypoints.h"
#include "app/Runtime.h"
#include "ui/Localization.h"
#include "ui/Widgets.h"
#include "ll/api/memory/Hook.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/gui/GuiData.h"
#include "mc/client/options/IOptionRegistry.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/client/renderer/game/LevelRendererPlayer.h"
#include "mc/client/renderer/BaseActorRenderContext.h"
#include "mc/client/renderer/screen/MinecraftUIRenderContext.h"
#include "mc/deps/renderer/Camera.h"
#include "mc/deps/renderer/MatrixStack.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <format>
#include <mutex>

namespace lamium::map::markers {
namespace {
std::mutex mutex;
std::optional<CameraView> camera; // Owned copy of the last rendered camera.
std::atomic<bool> held{false};
// The world position of the camera for the frame. setupCamera works in
// camera-relative space (its position reads 0, 0, 0), so the entity pass
// supplies it.
std::optional<Vec3> cameraWorld;
bool hooked = false;
bool logged = false;

// Runs outside the other camera hooks (FreeCamera, Zoom), so it copies the
// camera they produced.
LL_TYPE_INSTANCE_HOOK(CameraCopyHook, ll::memory::HookPriority::Highest, LevelRendererPlayer,
    &LevelRendererPlayer::setupCamera, void, mce::Camera& renderCamera, float alpha) {
    origin(renderCamera, alpha);
    try {
        if (renderCamera.viewMatrixStack->stack->empty() || renderCamera.projectionMatrixStack->stack->empty()) return;
        auto view = *renderCamera.viewMatrixStack->top()._m;
        auto projection = *renderCamera.projectionMatrixStack->top()._m;
        auto eye = *renderCamera.mPosition;
        CameraView copy;
        copy.x = eye.x;
        copy.y = eye.y;
        copy.z = eye.z;
        // The view matrix's rotation rows are the camera axes; it looks down -z.
        copy.right = {view[0][0], view[1][0], view[2][0]};
        copy.up = {view[0][1], view[1][1], view[2][1]};
        copy.forward = {-view[0][2], -view[1][2], -view[2][2]};
        copy.scaleX = projection[0][0];
        copy.scaleY = projection[1][1];
        bool finite = std::isfinite(copy.x) && std::isfinite(copy.y) && std::isfinite(copy.z)
            && std::isfinite(copy.scaleX) && std::isfinite(copy.scaleY) && copy.scaleX > 0 && copy.scaleY > 0;
        std::lock_guard lock(mutex);
        camera = finite ? std::optional(copy) : std::nullopt;
        if (finite && cameraWorld && !logged) {
            logged = true;
            Runtime::instance().self().getLogger().info(
                "Waypoint markers: camera at {:.1f} {:.1f} {:.1f} (setup {:.1f} {:.1f} {:.1f}), forward {:.2f} {:.2f} {:.2f}, scale {:.3f} x {:.3f}",
                cameraWorld->x, cameraWorld->y, cameraWorld->z, copy.x, copy.y, copy.z, copy.forward[0], copy.forward[1],
                copy.forward[2], copy.scaleX, copy.scaleY);
        }
    } catch (...) {}
}

LL_TYPE_INSTANCE_HOOK(CameraPositionHook, ll::memory::HookPriority::Normal, LevelRendererPlayer,
    &LevelRendererPlayer::$renderEntityEffects, void, BaseActorRenderContext& context) {
    origin(context);
    try {
        if (!context.mImpl) return;
        Vec3 position = context.mImpl->mCameraPosition;
        if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z)) return;
        std::lock_guard lock(mutex);
        cameraWorld = position;
    } catch (...) {}
}
bool positionHooked = false;
ui::Rgb rgb(std::uint32_t color) { return {channel(color, 0) / 255.f, channel(color, 1) / 255.f, channel(color, 2) / 255.f}; }
// A diamond from rows of rectangles: black, then the color one unit inside;
// hollow (a temporary waypoint, L-261) with black again two units further in.
void diamond(MinecraftUIRenderContext& context, float cx, float cy, int size, ui::Rgb color, bool hollow = false) {
    auto rows = diamondRows(size);
    float top = cy - static_cast<float>(rows.size()) / 2;
    for (size_t i = 0; i < rows.size(); ++i) {
        float half = static_cast<float>(rows[i]) + .5f;
        ui::fill(context, cx - half, top + i, 2 * half, 1, ui::Rgb{0, 0, 0}, .85f);
    }
    auto inner = diamondRows(size - 2);
    top = cy - static_cast<float>(inner.size()) / 2;
    for (size_t i = 0; i < inner.size(); ++i) {
        float half = static_cast<float>(inner[i]) + .5f;
        ui::fill(context, cx - half, top + i, 2 * half, 1, color);
    }
    if (!hollow) return;
    auto hole = diamondRows(size - 4);
    top = cy - static_cast<float>(hole.size()) / 2;
    for (size_t i = 0; i < hole.size(); ++i) {
        float half = static_cast<float>(hole[i]) + .5f;
        ui::fill(context, cx - half, top + i, 2 * half, 1, ui::Rgb{0, 0, 0}, .85f);
    }
}
void cross(MinecraftUIRenderContext& context, float cx, float cy) {
    for (int pass = 0; pass < 2; ++pass)
        for (int i = -2; i <= 2; ++i)
            for (int sign : {1, -1}) {
                float x = cx + i - .5f, y = cy + sign * i - .5f;
                if (pass == 0) ui::fill(context, x - 1, y - 1, 3, 3, ui::Rgb{0, 0, 0}, .85f);
                else ui::fill(context, x, y, 1, 1, rgb(deathColor));
            }
}
}

std::optional<CameraView> lastCamera() {
    std::lock_guard lock(mutex);
    if (!camera || !cameraWorld) return std::nullopt;
    auto view = *camera;
    view.x = cameraWorld->x;
    view.y = cameraWorld->y;
    view.z = cameraWorld->z;
    return view;
}
void draw(MinecraftUIRenderContext& context, float width, float height, Settings::Map const& settings) {
    if (!settings.waypoints || !worldMarkersShown(settings.waypointsWorld, held.load())) return;
    if (context.mClient.getOptions().getHideHud() || camera::freeCameraHide::hudHidden()) return;
    auto* player = context.mClient.getLocalPlayer();
    if (!player) return;
    auto view = lastCamera();
    if (!view) return;
    auto feet = player->getFeetPos();
    bool free = CameraSessions::instance().blocksPerspective();
    double distanceX = free ? view->x : feet.x, distanceZ = free ? view->z : feet.z;
    int dimension = static_cast<int>(player->getDimensionId());
    auto set = waypoints::current();
    struct Marker { double sx, sy, distance; std::string name; int color; bool temporary; };
    std::vector<Marker> shownMarkers;
    auto consider = [&](double x, double y, double z, std::string name, int color, bool temporary = false) {
        double distance = std::hypot(x - distanceX, z - distanceZ);
        if (settings.waypointDistance > 0 && distance > settings.waypointDistance) return;
        // Aim at the middle of the block above the marked one, at eye level.
        if (auto p = project(*view, x, y + 1.5, z, width, height, 12))
            shownMarkers.push_back({p->x, p->y, distance, std::move(name), color, temporary});
    };
    for (auto const& w : set.waypoints) {
        if (!w.visible) continue;
        if (auto at = shownPosition(w.x, w.y, w.z, w.dimension, dimension, settings.waypointsCrossScale))
            consider(at->x, at->y, at->z, w.name, w.color, w.temporary);
    }
    if (set.death && set.death->dimension == dimension)
        consider(set.death->x + .5, set.death->y, set.death->z + .5, ui::translated("waypoint.death"), -1);
    // Far ones first, so nearer markers sit on top.
    std::sort(shownMarkers.begin(), shownMarkers.end(), [](Marker const& a, Marker const& b) { return a.distance > b.distance; });
    constexpr float textScale = .75f;
    float pixel = context.mClient.getGuiData()->mInvGuiScale;
    if (!(pixel > 0) || !std::isfinite(pixel)) pixel = 1;
    auto snap = [pixel](float v) { return std::round(v / pixel) * pixel; };
    for (auto const& m : shownMarkers) {
        // Whole screen pixels, not GUI units: a unit is several pixels, and
        // rounding to it made markers jump while the player moved.
        float x = snap(static_cast<float>(m.sx)), y = snap(static_cast<float>(m.sy));
        if (m.color < 0) cross(context, x, y);
        else diamond(context, x, y, 7, rgb(waypointColors[static_cast<size_t>(clampColor(m.color))]), m.temporary);
        float below = y + 5;
        if (nearCrosshair(m.sx, m.sy, width, height)) {
            float w = ui::textWidthScaled(context, m.name, textScale);
            ui::labelScaled(context, x - w / 2, below, w + 2, m.name, textScale, ui::palette::text, ui::Align::Left, true);
            below += 8;
        }
        auto distance = std::format("{} m", static_cast<long long>(std::lround(m.distance)));
        float w = ui::textWidthScaled(context, distance, textScale);
        ui::labelScaled(context, x - w / 2, below, w + 2, distance, textScale, ui::Rgb{.84f, .86f, .85f}, ui::Align::Left, true);
    }
    context.flushText(0, std::nullopt);
}
void setHidden(bool down) { held = down; }
void start() {
    if (!hooked) hooked = CameraCopyHook::hook(true) == 0;
    if (!positionHooked) positionHooked = CameraPositionHook::hook(true) == 0;
    if (!positionHooked && hooked && CameraCopyHook::unhook(true)) hooked = false;
    // Without the camera copy the world markers stay off; the rest works.
    if (!hooked) Runtime::instance().self().getLogger().warn("Waypoint world markers unavailable");
}
void stop() {
    if (hooked && CameraCopyHook::unhook(true)) hooked = false;
    if (positionHooked && CameraPositionHook::unhook(true)) positionHooked = false;
    std::lock_guard lock(mutex);
    camera.reset();
    cameraWorld.reset();
}
}
