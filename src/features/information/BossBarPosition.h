#pragma once
#include "ui/HudEditorLayout.h"
#include <algorithm>
#include <cmath>
#include <optional>
#include <span>
#include <string_view>
class MinecraftUIRenderContext;

namespace lamium::information::bossBarPosition {
// Places the game's boss bars from the HUD layout (L-323), the way the
// scoreboard is placed: translation and scale for each draw, the game's
// layout and styling stay.
// The vanilla HUD stacks the bars in a grid named `boss_health_grid`
// (182 wide, 30 % of the screen tall, one 182x20 item per bar). Its items
// share their name with the outer full-screen panel, so a part is recognized
// by the grid.
inline bool gridControl(std::string_view name) { return name == "boss_health_grid"; }
// One bar: the name over a 5-unit bar.
inline constexpr float barWidth = 182, barHeight = 20;
// The placed box is what the bars fill, not the grid's fixed 30 %: the top
// of the grid down to the lowest shown item, so more bars grow away from the
// element's anchor. `items` are the shown items' top and bottom edges.
struct Span { float top, bottom; };
inline std::optional<ui::hud_editor::Box> occupied(ui::hud_editor::Box grid, std::span<Span const> items) {
    for (float v : {grid.x, grid.y, grid.w}) if (!std::isfinite(v)) return std::nullopt;
    float bottom = grid.y;
    for (auto item : items)
        if (std::isfinite(item.bottom) && std::isfinite(item.top) && item.bottom > item.top) bottom = std::max(bottom, item.bottom);
    if (bottom <= grid.y || grid.w <= 0) return std::nullopt;
    return ui::hud_editor::Box{grid.x, grid.y, grid.w, bottom - grid.y};
}
void start() noexcept;
void stop();
// A sample in the editor; only owned geometry from play is remembered.
// `outline` measures and places it without drawing.
std::optional<ui::hud_editor::Box> preview(MinecraftUIRenderContext&, float width, float height,
                                           ui::HudElement const&, bool outline);
}
