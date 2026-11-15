#pragma once
#include "ui/HudEditorLayout.h"
#include <optional>
#include <string_view>
class MinecraftUIRenderContext;

namespace lamium::information::scoreboardPosition {
// Places the game's scoreboard sidebar from the HUD layout (L-321), the way
// Chat position places the chat: translation only, the game's measured size
// and all of its styling stay.
// The vanilla hud_screen names the sidebar panel `sidebar`; its parts have
// generic names (`main`, the factory-made rows), so a part is recognized by
// that ancestor.
inline bool sidebarControl(std::string_view name) { return name == "sidebar"; }
// The editor's stand-in size until the game has drawn a sidebar.
inline constexpr float sampleWidth = 76, sampleHeight = 40;
void start() noexcept;
void stop();
// A sample in the editor; only owned geometry from play is remembered.
// `outline` measures and places it without drawing.
std::optional<ui::hud_editor::Box> preview(MinecraftUIRenderContext&, float width, float height,
                                           ui::HudElement const&, bool outline);
}
