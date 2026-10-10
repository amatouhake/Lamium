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
    case Action::ToggleShapes: field = &value.overlays.shapes; break;
    case Action::HideOffhand: field = &value.visuals.hideOffhand; break;
    case Action::HideWeather: field = &value.visuals.hideWeather; break;
    case Action::HideParticles: field = &value.visuals.hideParticles; break;
    case Action::HideBossBars: field = &value.visuals.hideBossBars; break;
    case Action::HideNausea: field = &value.visuals.hideNausea; break;
    case Action::HideWater: field = &value.visuals.hideWater; break;
    case Action::HideLava: field = &value.visuals.hideLava; break;
    case Action::HidePowderSnow: field = &value.visuals.hidePowderSnow; break;
    case Action::HideDistanceFog: field = &value.visuals.hideDistanceFog; break;
    case Action::EdgeGuard: field = &value.interaction.edgeGuard; break;
    case Action::ToolGuard: field = &value.interaction.toolGuard; break;
    case Action::ElytraSwap: field = &value.interaction.elytraSwap; break;
    case Action::Hitboxes: field = &value.overlays.hitboxes; break;
    case Action::LightOverlay: field = &value.overlays.light; break;
    case Action::ToolSwitch: field = &value.inventory.toolSwitch; break;
    case Action::WeaponSwitch: field = &value.inventory.weaponSwitch; break;
    case Action::HandRestock: field = &value.inventory.handRestock; break;
    case Action::FakeOffhand: field = &value.inventory.fakeOffhand; break;
    case Action::Transfer: field = &value.inventory.transfer; break;
    case Action::Minimap: field = &value.map.minimap; break;
    case Action::TogglePreviews: field = &value.inspection.containerPreviews; break;
    case Action::ToggleDurability: field = &value.inspection.durability; break;
    case Action::ToggleSorting: field = &value.inventory.sorting; break;
    case Action::ToggleOffhandSwap: field = &value.inventory.offhandSwap; break;
    case Action::ToggleDeathRestore: field = &value.inventory.deathRestore; break;
    case Action::ToggleHideEffects: field = &value.visuals.hideEffects; break;
    case Action::ToggleDurabilityHud: field = &value.information.durabilityHud; break;
    case Action::ToggleOffhandSlot: field = &value.information.offhandSlot; break;
    case Action::ToggleSaturation: field = &value.information.saturation; break;
    case Action::ToggleFoodValues: field = &value.inspection.foodValues; break;
    case Action::ToggleLockedTrades: field = &value.inspection.lockedTrades; break;
    case Action::ToggleAutomationStatus: field = &value.ui.automationStatus; break;
    case Action::ToggleRadar: field = &value.map.radar; break;
    case Action::ToggleWaypoints: field = &value.map.waypoints; break;
    case Action::ToggleWorldMap: field = &value.map.worldMap; break;
    case Action::ToggleSchematic: field = &value.schematic.enabled; break;
    case Action::ToggleSchematicHud: field = &value.schematic.hud; break;
    case Action::PeriodicAttack: field = &value.interaction.autoAttack; break;
    case Action::PeriodicUse: field = &value.interaction.autoUse; break;
    case Action::AttackHeldOnly: field = &value.interaction.attackHeldOnly; break;
    case Action::UseHeldOnly: field = &value.interaction.useHeldOnly; break;
    default: return false;
    }
    *field = !*field;
    return true;
}
}
