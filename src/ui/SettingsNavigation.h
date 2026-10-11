#pragma once
#include "ui/SettingsRows.h"

namespace lamium::ui {
struct SettingsNavigation {
    int current = 0;
    int remembered = 0;

    void reopenNormal() { current = remembered; }
    void select(int index, bool temporary = false) {
        current = index;
        if (!temporary) remembered = index;
    }
};
// Navigation items: All, each section, then the tools pinned to the
// sidebar's bottom (Hotkeys, Shapes, Waypoints, Schematics, the world map,
// the HUD layout).
namespace nav {
inline constexpr int count = static_cast<int>(sections.size()) + 7;
inline constexpr int hotkeys = count - 6;
inline constexpr int shapes = count - 5;
inline constexpr int waypoints = count - 4;
inline constexpr int schematics = count - 3;
// The world map is never the current item: choosing it opens the map over
// the panel, and closing the map returns to where the settings were.
inline constexpr int worldMap = count - 2;
// The HUD layout editor replaces the whole panel.
inline constexpr int hud = count - 1;
}
}
