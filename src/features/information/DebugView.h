#pragma once
#include "settings/Settings.h"
namespace lamium::information {
// Rendering profile only: enabling debug never overwrites normal HUD choices.
inline Settings::Information debugProfile(Settings::Information value) {
    if (!value.debug) return value;
    value.hud = value.coordinates = value.dimension = value.biome = value.facing = true;
    value.fps = value.frameTime = value.light = value.ping = true;
    value.target = value.targetIdentifier = value.targetStates = true;
    value.targetCoordinates = true;
    value.horizontal = value.vertical = value.targetVertical = 0;
    value.targetHorizontal = 100;
    return value;
}
}
