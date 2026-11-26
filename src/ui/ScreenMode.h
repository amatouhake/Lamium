#pragma once
// Which part of the settings screen takes an input, as pure decisions with
// tests (L-399). The screen's modes are separate switches, and several can be
// on at once (the waypoint prompt over the world map, a prompt over a tool
// view). Every input follows the frame's order: row page, calculator, world
// map without the waypoint prompt, waypoint prompt, schematic menu, area-save
// prompt, HUD editor, tool view, table (maintainer 2026-11-26).
namespace lamium::ui::screen_mode {
struct Open {
    bool rowPage = false, calculator = false, schematicMenu = false, worldMap = false, prompt = false, savePrompt = false,
         hudEditor = false, tool = false;
};
enum class Wheel { RowPage, Calculator, SchematicMenu, WorldMap, Prompt, HudEditor, Tool, Table };
// Who the wheel turns. A prompt (waypoint or area-save) takes it and does
// nothing with it, so the view hidden under the prompt does not move unseen.
// The schematic menu and the world map are never open together.
constexpr Wheel wheel(Open const& open) {
    if (open.rowPage) return Wheel::RowPage;
    if (open.calculator) return Wheel::Calculator;
    if (open.worldMap && !open.prompt) return Wheel::WorldMap;
    if (open.prompt) return Wheel::Prompt;
    if (open.schematicMenu) return Wheel::SchematicMenu;
    if (open.savePrompt) return Wheel::Prompt;
    if (open.hudEditor) return Wheel::HudEditor;
    if (open.tool) return Wheel::Tool;
    return Wheel::Table;
}
}
