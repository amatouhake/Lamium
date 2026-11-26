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
    check(wheel({.schematicMenu = true, .tool = true}) == Wheel::SchematicMenu && wheel({.schematicMenu = true, .savePrompt = true}) == Wheel::SchematicMenu,
          "the schematic menu takes the wheel before the area-save prompt and the views, as the frame orders them");
    check(wheel({.worldMap = true, .hudEditor = true, .tool = true}) == Wheel::WorldMap, "the world map zooms over what lies under it");
    check(wheel({.worldMap = true, .prompt = true, .tool = true}) == Wheel::Prompt && wheel({.prompt = true}) == Wheel::Prompt,
          "the waypoint prompt takes the wheel, over the world map and over a view, so nothing hidden moves");
    check(wheel({.savePrompt = true, .tool = true}) == Wheel::Prompt && wheel({.savePrompt = true, .hudEditor = true}) == Wheel::Prompt
              && wheel({.savePrompt = true}) == Wheel::Prompt, "so does the area-save prompt");
    check(wheel({.calculator = true, .prompt = true}) == Wheel::Calculator && wheel({.rowPage = true, .savePrompt = true}) == Wheel::RowPage,
          "a row page and the calculator still come before a prompt");
}
