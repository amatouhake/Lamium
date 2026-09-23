#pragma once
#include "settings/Settings.h"
namespace lamium::input {
inline bool toggleAction(Settings& value, Action action) {
    bool* field = nullptr;
    switch (action) {
    case Action::BreakingRestriction: field = &value.interaction.breaking; break;
    case Action::DebugView: field = &value.information.debug; break;
    case Action::TargetInfo: field = &value.information.target; break;
    case Action::InfoHud: field = &value.information.hud; break;
    case Action::NightVision: field = &value.lighting.nightVision; break;
    case Action::ChunkBorders: field = &value.overlays.chunkBorders; break;
    case Action::HideOffhand: field = &value.visuals.hideOffhand; break;
    case Action::Hitboxes: field = &value.overlays.hitboxes; break;
    case Action::LightOverlay: field = &value.overlays.light; break;
    case Action::ToolSwitch: field = &value.inventory.toolSwitch; break;
    default: return false;
    }
    *field = !*field;
    return true;
}
}
