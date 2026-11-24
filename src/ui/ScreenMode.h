#pragma once
// Which part of the settings screen takes an input, as pure decisions with
// tests (L-399). The screen's modes are separate switches, and several can be
// on at once (the waypoint prompt over the world map, a page over a tool
// view), so each kind of input states its own order here.
namespace lamium::ui::screen_mode {
struct Open {
    bool rowPage = false, calculator = false, schematicMenu = false, worldMap = false, prompt = false, hudEditor = false,
         tool = false;
};
enum class Wheel { RowPage, Calculator, SchematicMenu, WorldMap, HudEditor, Tool, Table };
// Who the wheel turns. The world map under the waypoint prompt does not zoom;
// the wheel then goes on to what the navigation shows, as it does under the
// area-save prompt, which is not asked at all (L-399 Open: the frame's own
// order gives a prompt everything).
constexpr Wheel wheel(Open const& open) {
    if (open.rowPage) return Wheel::RowPage;
    if (open.calculator) return Wheel::Calculator;
    if (open.schematicMenu) return Wheel::SchematicMenu;
    if (open.worldMap && !open.prompt) return Wheel::WorldMap;
    if (open.hudEditor) return Wheel::HudEditor;
    if (open.tool) return Wheel::Tool;
    return Wheel::Table;
}
}
