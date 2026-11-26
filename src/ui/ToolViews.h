#pragma once
// The tool views chosen from the sidebar (Shapes, Waypoints, Schematics): one
// shape for what the screen's core does with them (L-399), as RowPages.h is
// for the pages that replace the settings rows. Input, the wheel, editing
// keys and drawing go to the view shown; a typed number looks for the view
// editing one; ending an edit reaches every view.
#include "features/schematic/SchematicsView.h"
#include "ui/SettingsNavigation.h"
#include "ui/ShapesView.h"
#include "features/map/WaypointsView.h"
#include <array>
namespace lamium::ui::tool_views {
struct View {
    int nav; // its navigation item (SettingsNavigation.h nav::)
    void (*refresh)(); // its data as it is now, before each frame's input and drawing
    void (*click)(float, float, bool);
    void (*key)(int);
    void (*wheel)(int step, glm::vec2 pointer, bool shift);
    bool (*editing)(); // a name or a number is being typed
    bool (*editingNumber)();
    void (*applyNumber)();
    void (*endEditing)();
    glm::vec2 (*caret)(); // where the native keyboard's caret goes
    bool (*docked)();
    void (*renderDocked)(MinecraftUIRenderContext&, glm::vec2 size, glm::vec2 pointer);
    void (*renderContent)(MinecraftUIRenderContext&, glm::vec2 size, glm::vec2 pointer, SettingsTable const&);
};
// In the order a typed number looks for its view.
inline constexpr std::array<View, 3> views{{
    {nav::waypoints, waypoints_view::refresh, waypoints_view::click, waypoints_view::key,
     [](int step, glm::vec2 pointer, bool) { waypoints_view::wheel(step, pointer); },
     [] { return waypoints_view::editingName() || waypoints_view::editingNumber(); }, waypoints_view::editingNumber,
     waypoints_view::applyNumber, waypoints_view::endEditing, waypoints_view::caret, waypoints_view::docked,
     waypoints_view::renderDocked, waypoints_view::renderContent},
    {nav::schematics, [] { schematics_view::refresh(false); }, schematics_view::click, schematics_view::key, schematics_view::wheel,
     schematics_view::editingNumber, schematics_view::editingNumber, schematics_view::applyNumber, schematics_view::endEditing,
     schematics_view::caret, schematics_view::docked, schematics_view::renderDocked, schematics_view::renderContent},
    {nav::shapes, shapes_view::refresh, shapes_view::click, shapes_view::key,
     [](int step, glm::vec2 pointer, bool) { shapes_view::wheel(step, pointer); },
     [] { return shapes_view::editingName() || shapes_view::editingNumber(); }, shapes_view::editingNumber,
     shapes_view::applyNumber, shapes_view::endEditing, shapes_view::caret, shapes_view::docked,
     shapes_view::renderDocked, shapes_view::renderContent},
}};
// The view of a navigation item, or null when the item is not a tool view.
inline View const* at(int navItem) {
    for (auto const& view : views) if (view.nav == navItem) return &view;
    return nullptr;
}
// The view a number is being typed into, if any.
inline View const* editingNumber() {
    for (auto const& view : views) if (view.editingNumber()) return &view;
    return nullptr;
}
inline void endEditing() { for (auto const& view : views) view.endEditing(); }
}
