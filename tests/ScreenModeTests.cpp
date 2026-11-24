#include "ui/ScreenMode.h"
void check(bool, char const*);
void screenModeTests() {
    using namespace lamium::ui::screen_mode;
    check(wheel({}) == Wheel::Table, "with nothing else open the wheel scrolls the settings table");
    check(wheel({.tool = true}) == Wheel::Tool && wheel({.hudEditor = true, .tool = true}) == Wheel::HudEditor,
          "a tool view takes the wheel, the HUD editor before it");
    check(wheel({.rowPage = true, .calculator = true, .schematicMenu = true, .worldMap = true, .hudEditor = true, .tool = true})
              == Wheel::RowPage, "a row page takes the wheel before everything");
    check(wheel({.calculator = true, .schematicMenu = true, .worldMap = true, .tool = true}) == Wheel::Calculator,
          "then the calculator");
    check(wheel({.schematicMenu = true, .worldMap = true, .tool = true}) == Wheel::SchematicMenu,
          "the schematic menu takes the wheel before the world map");
    check(wheel({.worldMap = true, .hudEditor = true, .tool = true}) == Wheel::WorldMap, "the world map zooms over what lies under it");
    // As the screen behaves today: the prompt does not take the wheel itself.
    check(wheel({.worldMap = true, .prompt = true, .tool = true}) == Wheel::Tool && wheel({.worldMap = true, .prompt = true}) == Wheel::Table,
          "under the waypoint prompt the world map does not zoom; the wheel goes on to what the navigation shows");
    check(wheel({.prompt = true}) == Wheel::Table && wheel({.calculator = true, .prompt = true}) == Wheel::Calculator,
          "the prompt alone changes nobody else's turn");
}
