#pragma once
#include "ui/HudEditorLayout.h"
#include <optional>
class MinecraftUIRenderContext;

namespace lamium::information::chatPosition {
// Translation and a uniform scale only: the game's measured layout (line
// breaks, spacing) and all of its styling stay. `chatW`, `chatH` are the
// game's measured size; the placed box has the element's scale applied.
// Shared with the scoreboard (L-321).
inline float zoom(ui::HudElement const& element) { return ui::clampHudScale(element.scale) / 100; }
inline std::optional<ui::hud_editor::Box> placed(float width, float height, float chatW, float chatH,
                                               ui::HudElement const& element) {
    for (float v : {width, height, chatW, chatH}) if (!std::isfinite(v)) return std::nullopt;
    float w = chatW * zoom(element), h = chatH * zoom(element);
    if (width <= 0 || height <= 0 || chatW <= 0 || chatH <= 0 || w > width || h > height)
        return std::nullopt;
    auto at = ui::placeElement(width, height, w, h, element);
    return ui::hud_editor::Box{at.x, at.y, w, h};
}
inline std::optional<ui::hud_editor::Point> translated(float width, float height, ui::hud_editor::Box panel,
                                                     ui::hud_editor::Point position, ui::HudElement const& element) {
    for (float v : {panel.x, panel.y, position.x, position.y}) if (!std::isfinite(v)) return std::nullopt;
    auto box = placed(width, height, panel.w, panel.h, element);
    if (!box) return std::nullopt;
    // A part keeps its place inside the panel, scaled with it.
    float z = zoom(element);
    ui::hud_editor::Point point{box->x + (position.x - panel.x) * z, box->y + (position.y - panel.y) * z};
    if (!std::isfinite(point.x) || !std::isfinite(point.y)) return std::nullopt;
    return point;
}
void start() noexcept;
void stop();
// A sample in the editor; only owned geometry from play is remembered.
std::optional<ui::hud_editor::Box> preview(MinecraftUIRenderContext&, float width, float height,
                                         ui::HudElement const&);
}
