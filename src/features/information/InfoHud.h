#pragma once
#include "settings/Settings.h"
#include "ui/HudEditorLayout.h"
class MinecraftUIRenderContext;
class ScreenView;
namespace lamium::information {
// Layout editor preview: draw with this layout instead of the saved one, show
// every element whether or not its feature is on, and fill empty elements
// with sample content so they can be placed.
struct HudPreview { Settings::Hud layout; };
// Returns where each element was drawn this frame (for editor hit tests).
ui::hud_editor::Boxes drawHud(MinecraftUIRenderContext&, float width, float height, Settings::Information const&,
                              HudPreview const* preview = nullptr);
// The offhand item beside the game's hotbar (L-75); `view` is the HUD view.
void drawOffhandSlot(MinecraftUIRenderContext&, ScreenView const& view, Settings::Information const&);
// Remembers where the game laid out the hotbar on the HUD view, for the
// layout editor, which draws it there (it opens over another screen).
void noteHotbar(ScreenView const& view);
struct HotbarPlace { float x, y, w, h; };
// Whether the Info HUD drawn this frame covers the game's paper doll or its
// position line, for hiding them there (L-145, option on).
void noteCoveredVanilla(ScreenView const& view, ui::hud_editor::Boxes const& drawn, Settings::Information const&);
HotbarPlace editorHotbar(float width, float height);
// A biome's name in the game's language, or its id when it has none.
std::string biomeName(std::string const& identifier);
// Forget the status effect row order (L-247): the next frame sorts afresh.
// Called when the effects feature stops and on world exit, so an effect in a
// new world cannot inherit a row order kept from the old one.
void forgetEffectRows();
}
