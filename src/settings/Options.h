#pragma once
#include "settings/Settings.h"
#include <array>
#include <optional>
#include <string_view>
#include <variant>

namespace lamium::settings {
struct ChoiceValue { std::string_view label; bool operator==(ChoiceValue const&) const = default; };
using OptionValue = std::variant<bool, float, ChoiceValue>;
struct NumericOption {
    float minimum, maximum;
    void (*write)(Settings&, float);
    float step = 0; // > 0: shown as a slider that snaps to this step
    float secondary = 0; // > 0: the label also formats value * secondary (ticks -> seconds)
};
struct Option {
    std::string_view id;
    std::string_view feature;
    std::string_view label;
    OptionValue (*read)(Settings const&);
    void (*adjust)(Settings&, int);
    std::optional<NumericOption> numeric = std::nullopt;
};

// Stable identifiers and feature ownership are independent of presentation
// order. Editors use the same accessors rather than maintaining row switches.
template<auto Group, auto Member>
constexpr Option toggle(std::string_view id, std::string_view feature, std::string_view label) {
    return {id, feature, label,
        [](Settings const& value) -> OptionValue { return (value.*Group).*Member; },
        [](Settings& value, int) { auto& field = (value.*Group).*Member; field = !field; }};
}
template<auto Group, auto Member, auto const& Labels>
constexpr Option choice(std::string_view id, std::string_view feature, std::string_view label) {
    return {id, feature, label,
        [](Settings const& value) -> OptionValue {
            auto index = static_cast<size_t>((value.*Group).*Member);
            return ChoiceValue{Labels[index < Labels.size() ? index : 0]};
        },
        [](Settings& value, int direction) {
            auto& field = (value.*Group).*Member;
            auto index = static_cast<size_t>(field);
            if (index >= Labels.size()) index = 0;
            index = (index + (direction < 0 ? Labels.size()-1 : 1)) % Labels.size();
            field = static_cast<std::remove_reference_t<decltype(field)>>(index);
        }};
}
inline constexpr std::array<std::string_view,2> activationLabels{"activation.hold","activation.toggle"};
inline constexpr std::array<std::string_view,3> healthMeterLabels{"meter.hearts","meter.bar","meter.number"};
inline constexpr std::array<std::string_view,2> growthMeterLabels{"meter.bar","meter.number"};
inline constexpr std::array<std::string_view,3> animationLabels{"animations.follow","animations.on","animations.off"};
inline constexpr auto anchorLabels = std::to_array<std::string_view>(
    {"anchor.topLeft", "anchor.topCenter", "anchor.topRight", "anchor.middleLeft", "anchor.center",
     "anchor.middleRight", "anchor.bottomLeft", "anchor.bottomCenter", "anchor.bottomRight"});
inline constexpr auto elementBackgroundLabels = std::to_array<std::string_view>(
    {"hudBackgroundNone", "hudBackgroundCard"});
inline ui::HudElement const& hudElement(Settings const& value, ui::HudElementId id) {
    switch (id) {
    case ui::HudElementId::Info: return value.hud.info;
    case ui::HudElementId::Target: return value.hud.target;
    case ui::HudElementId::Status: return value.hud.status;
    case ui::HudElementId::Magnification: return value.hud.magnification;
    default: return value.hud.toast;
    }
}
inline ui::HudElement& hudElement(Settings& value, ui::HudElementId id) {
    switch (id) {
    case ui::HudElementId::Info: return value.hud.info;
    case ui::HudElementId::Target: return value.hud.target;
    case ui::HudElementId::Status: return value.hud.status;
    case ui::HudElementId::Magnification: return value.hud.magnification;
    default: return value.hud.toast;
    }
}
template<ui::HudElementId Id, auto Field>
constexpr Option hudToggle(std::string_view id, std::string_view feature, std::string_view label) {
    return {id, feature, label,
        [](Settings const& value) -> OptionValue { return hudElement(value, Id).*Field; },
        [](Settings& value, int) { auto& field = hudElement(value, Id).*Field; field = !field; }};
}
template<ui::HudElementId Id, auto Field, auto const& Labels>
constexpr Option hudChoice(std::string_view id, std::string_view feature, std::string_view label) {
    return {id, feature, label,
        [](Settings const& value) -> OptionValue {
            auto index = static_cast<size_t>(hudElement(value, Id).*Field);
            return ChoiceValue{Labels[index < Labels.size() ? index : 0]};
        },
        [](Settings& value, int direction) {
            auto& field = hudElement(value, Id).*Field;
            auto index = static_cast<size_t>(field);
            if (index >= Labels.size()) index = 0;
            index = (index + (direction < 0 ? Labels.size() - 1 : 1)) % Labels.size();
            field = static_cast<std::remove_reference_t<decltype(field)>>(index);
        }};
}
template<ui::HudElementId Id, auto Field, int Step>
constexpr Option hudNumeric(std::string_view id, std::string_view feature, std::string_view label,
                            float minimum, float maximum) {
    return {id, feature, label,
        [](Settings const& value) -> OptionValue { return hudElement(value, Id).*Field; },
        [](Settings& value, int direction) {
            auto& field = hudElement(value, Id).*Field;
            field += direction * Step;
            value.normalize();
        },
        NumericOption{minimum, maximum, [](Settings& value, float number) { hudElement(value, Id).*Field = number; }}};
}
inline constexpr auto options = std::to_array<Option>({
    toggle<&Settings::interaction, &Settings::Interaction::autoAttack>("interaction.autoAttack", "periodicAttack", "autoAttack"),
    choice<&Settings::interaction, &Settings::Interaction::attackMode, interaction::autoModeLabels>("interaction.attackMode", "periodicAttack", "autoModeRow"),
    {"interaction.attackTicks", "periodicAttack", "autoInterval",
        [](Settings const& s) -> OptionValue { return s.interaction.attackTicks; },
        [](Settings& s, int direction) { s.interaction.attackTicks += direction; s.normalize(); },
        NumericOption{1, 1200, [](Settings& s, float v) { s.interaction.attackTicks = v; }, 0, .05f}},
    {"interaction.attackClicks", "periodicAttack", "autoClicks",
        [](Settings const& s) -> OptionValue { return s.interaction.attackClicks; },
        [](Settings& s, int direction) { s.interaction.attackClicks += direction; s.normalize(); },
        NumericOption{1, 10, [](Settings& s, float v) { s.interaction.attackClicks = v; }, 0, 20}},
    toggle<&Settings::interaction, &Settings::Interaction::attackHeldOnly>("interaction.attackHeldOnly", "periodicAttack", "fastHeldOnly"),
    toggle<&Settings::interaction, &Settings::Interaction::autoUse>("interaction.autoUse", "periodicUse", "autoUse"),
    choice<&Settings::interaction, &Settings::Interaction::useMode, interaction::autoModeLabels>("interaction.useMode", "periodicUse", "autoModeRow"),
    {"interaction.useTicks", "periodicUse", "autoInterval",
        [](Settings const& s) -> OptionValue { return s.interaction.useTicks; },
        [](Settings& s, int direction) { s.interaction.useTicks += direction; s.normalize(); },
        NumericOption{1, 1200, [](Settings& s, float v) { s.interaction.useTicks = v; }, 0, .05f}},
    {"interaction.useClicks", "periodicUse", "autoClicks",
        [](Settings const& s) -> OptionValue { return s.interaction.useClicks; },
        [](Settings& s, int direction) { s.interaction.useClicks += direction; s.normalize(); },
        NumericOption{1, 10, [](Settings& s, float v) { s.interaction.useClicks = v; }, 0, 20}},
    toggle<&Settings::interaction, &Settings::Interaction::useHeldOnly>("interaction.useHeldOnly", "periodicUse", "fastHeldOnly"),
    toggle<&Settings::interaction, &Settings::Interaction::breaking>("interaction.breaking", "restrictions", "breakingRestriction"),
    choice<&Settings::interaction, &Settings::Interaction::breakingMode, interaction::restrictionLabels>("interaction.breakingMode", "restrictions", "breakingMode"),
    choice<&Settings::interaction, &Settings::Interaction::placementMode, interaction::restrictionLabels>("interaction.placementMode", "restrictions", "placementMode"),
    toggle<&Settings::information, &Settings::Information::debug>("information.debug", "debugView", "debugView"),
    toggle<&Settings::information, &Settings::Information::target>("information.target", "targetInfo", "targetInfo"),
    toggle<&Settings::information, &Settings::Information::targetIdentifier>("information.targetIdentifier", "targetInfo", "targetIdentifier"),
    toggle<&Settings::information, &Settings::Information::targetIcon>("information.targetIcon", "targetInfo", "targetIcon"),
    choice<&Settings::information, &Settings::Information::targetHealth, healthMeterLabels>("information.targetHealth", "targetInfo", "targetHealth"),
    choice<&Settings::information, &Settings::Information::targetGrowth, growthMeterLabels>("information.targetGrowth", "targetInfo", "targetGrowth"),
    {"information.targetDistance", "targetInfo", "targetDistance",
        [](Settings const& s) -> OptionValue { return s.information.targetDistance; },
        [](Settings& s, int direction) { s.information.targetDistance += direction; s.normalize(); },
        NumericOption{2, 64, [](Settings& s, float v) { s.information.targetDistance = v; }, 1}},
    toggle<&Settings::information, &Settings::Information::targetStates>("information.targetStates", "targetInfo", "targetStates"),
    toggle<&Settings::information, &Settings::Information::targetCoordinates>("information.targetCoordinates", "targetInfo", "targetCoordinates"),
    toggle<&Settings::information, &Settings::Information::hud>("information.hud", "infoHud", "infoHud"),
    toggle<&Settings::information, &Settings::Information::coordinates>("information.coordinates", "infoHud", "hudCoordinates"),
    toggle<&Settings::information, &Settings::Information::dimension>("information.dimension", "infoHud", "hudDimension"),
    toggle<&Settings::information, &Settings::Information::biome>("information.biome", "infoHud", "hudBiome"),
    toggle<&Settings::information, &Settings::Information::facing>("information.facing", "infoHud", "hudFacing"),
    toggle<&Settings::information, &Settings::Information::fps>("information.fps", "infoHud", "hudFps"),
    toggle<&Settings::information, &Settings::Information::frameTime>("information.frameTime", "infoHud", "hudFrameTime"),
    toggle<&Settings::information, &Settings::Information::light>("information.light", "infoHud", "hudLight"),
    toggle<&Settings::information, &Settings::Information::ping>("information.ping", "infoHud", "hudPing"),
    toggle<&Settings::information, &Settings::Information::rotation>("information.rotation", "infoHud", "hudRotation"),
    toggle<&Settings::information, &Settings::Information::block>("information.block", "infoHud", "hudBlock"),
    toggle<&Settings::information, &Settings::Information::chunk>("information.chunk", "infoHud", "hudChunk"),
    toggle<&Settings::information, &Settings::Information::speed>("information.speed", "infoHud", "hudSpeed"),
    toggle<&Settings::information, &Settings::Information::time>("information.time", "infoHud", "hudTime"),
    toggle<&Settings::information, &Settings::Information::weather>("information.weather", "infoHud", "hudWeather"),
    toggle<&Settings::information, &Settings::Information::moon>("information.moon", "infoHud", "hudMoon"),
    toggle<&Settings::inventory, &Settings::Inventory::toolSwitch>("inventory.toolSwitch", "toolSwitch", "toolSwitch"),
    toggle<&Settings::inventory, &Settings::Inventory::handRestock>("inventory.handRestock", "handRestock", "handRestock"),
    toggle<&Settings::overlays, &Settings::Overlays::hitboxes>("overlays.hitboxes", "hitboxes", "hitboxes"),
    toggle<&Settings::overlays, &Settings::Overlays::light>("overlays.light", "lightOverlay", "lightOverlay"),
    choice<&Settings::overlays, &Settings::Overlays::lightValue, overlay::lightValueLabels>("overlays.lightValue", "lightOverlay", "lightValueRow"),
    {"overlays.lightRange", "lightOverlay", "lightRange",
        [](Settings const& s) -> OptionValue { return s.overlays.lightRange; },
        [](Settings& s, int direction) { s.overlays.lightRange += direction; s.normalize(); },
        NumericOption{4, 64, [](Settings& s, float v) { s.overlays.lightRange = v; }, 1}},
    choice<&Settings::overlays, &Settings::Overlays::lightFacing, overlay::lightFacingLabels>("overlays.lightFacing", "lightOverlay", "lightFacingRow"),
    {"overlays.hitboxDistance", "hitboxes", "hitboxDistance",
        [](Settings const& s) -> OptionValue { return s.overlays.hitboxDistance; },
        [](Settings& s, int direction) { s.overlays.hitboxDistance += direction * 8.f; s.normalize(); },
        NumericOption{8, 128, [](Settings& s, float v) { s.overlays.hitboxDistance = v; }, 8}},
    toggle<&Settings::visuals, &Settings::Visuals::hideOffhand>("visuals.hideOffhand", "hideOffhand", "hideOffhand"),
    toggle<&Settings::overlays, &Settings::Overlays::chunkBorders>("overlays.chunkBorders", "chunkBorders", "chunkBorders"),
    toggle<&Settings::overlays, &Settings::Overlays::shapes>("overlays.shapes", "shapes", "shapeRendering"),
    choice<&Settings::camera, &Settings::Camera::zoomToggle, activationLabels>("camera.zoomActivation", "zoom", "zoomActivation"),
    choice<&Settings::camera, &Settings::Camera::freelookToggle, activationLabels>("camera.freelookActivation", "freelook", "freelookActivation"),
    {"camera.magnification", "zoom", "magnification",
        [](Settings const& s) -> OptionValue { return s.camera.magnification; },
        [](Settings& s, int direction) { s.camera.magnification += direction * .5f; s.normalize(); },
        NumericOption{1, 50, [](Settings& s, float v) { s.camera.magnification = v; }, .5f}},
    toggle<&Settings::camera, &Settings::Camera::showMagnification>("camera.showMagnification", "zoom", "showMagnification"),
    toggle<&Settings::lighting, &Settings::Lighting::nightVision>("lighting.nightVision", "nightVision", "nightVision"),
    toggle<&Settings::inspection, &Settings::Inspection::containerPreviews>("inspection.containerPreviews", "previews", "previews"),
    toggle<&Settings::inspection, &Settings::Inspection::shulkerPreviews>("inspection.shulkerPreviews", "previews", "shulkerPreviews"),
    toggle<&Settings::inspection, &Settings::Inspection::emptyShulkerPreviews>("inspection.emptyShulkerPreviews", "previews", "emptyShulkerPreviews"),
    toggle<&Settings::inspection, &Settings::Inspection::hideShulkerContents>("inspection.hideShulkerContents", "previews", "hideShulkerContents"),
    toggle<&Settings::inspection, &Settings::Inspection::bundlePreviews>("inspection.bundlePreviews", "previews", "bundlePreviews"),
    toggle<&Settings::inspection, &Settings::Inspection::emptyBundlePreviews>("inspection.emptyBundlePreviews", "previews", "emptyBundlePreviews"),
    toggle<&Settings::inspection, &Settings::Inspection::durability>("inspection.durability", "durability", "durability"),
    toggle<&Settings::inventory, &Settings::Inventory::sorting>("inventory.sorting", "sorting", "sorting"),
    toggle<&Settings::inventory, &Settings::Inventory::sortContainers>("inventory.sortContainers", "sorting", "storage"),
    toggle<&Settings::ui, &Settings::Interface::toggleToasts>("interface.toggleToasts", "settings", "toggleToasts"),
    choice<&Settings::ui, &Settings::Interface::animations, animationLabels>("interface.animations", "settings", "animations"),
    toggle<&Settings::ui, &Settings::Interface::automationStatus>("interface.automationStatus", "automationStatus", "automationStatus"),
    hudNumeric<ui::HudElementId::Info, &ui::HudElement::scale, 25>("hud.info.scale", "infoHud", "hudScale", 75, 150),
    hudChoice<ui::HudElementId::Info, &ui::HudElement::background, elementBackgroundLabels>("hud.info.background", "infoHud", "hudBackground"),
    hudToggle<ui::HudElementId::Info, &ui::HudElement::shadow>("hud.info.shadow", "infoHud", "hudShadow"),
    hudNumeric<ui::HudElementId::Target, &ui::HudElement::scale, 25>("hud.target.scale", "targetInfo", "hudScale", 75, 150),
    hudChoice<ui::HudElementId::Target, &ui::HudElement::background, elementBackgroundLabels>("hud.target.background", "targetInfo", "hudBackground"),
    hudToggle<ui::HudElementId::Target, &ui::HudElement::shadow>("hud.target.shadow", "targetInfo", "hudShadow"),
    hudNumeric<ui::HudElementId::Status, &ui::HudElement::scale, 25>("hud.status.scale", "automationStatus", "hudScale", 75, 150),
    hudChoice<ui::HudElementId::Status, &ui::HudElement::background, elementBackgroundLabels>("hud.status.background", "automationStatus", "hudBackground"),
    hudToggle<ui::HudElementId::Status, &ui::HudElement::shadow>("hud.status.shadow", "automationStatus", "hudShadow"),
    hudNumeric<ui::HudElementId::Toast, &ui::HudElement::scale, 25>("hud.toast.scale", "settings", "hudScale", 75, 150),
    hudChoice<ui::HudElementId::Toast, &ui::HudElement::background, elementBackgroundLabels>("hud.toast.background", "settings", "hudBackground"),
    hudToggle<ui::HudElementId::Toast, &ui::HudElement::shadow>("hud.toast.shadow", "settings", "hudShadow"),
    hudNumeric<ui::HudElementId::Magnification, &ui::HudElement::scale, 25>("hud.magnification.scale", "zoom", "hudScale", 75, 150),
    hudChoice<ui::HudElementId::Magnification, &ui::HudElement::background, elementBackgroundLabels>("hud.magnification.background", "zoom", "hudBackground"),
    hudToggle<ui::HudElementId::Magnification, &ui::HudElement::shadow>("hud.magnification.shadow", "zoom", "hudShadow"),
});
inline Option const* find(std::string_view id) {
    for (auto const& option : options) if (option.id == id) return &option;
    return nullptr;
}
// Set one option back to its value in `defaults` through its own accessors:
// numbers are written, switches and choices are stepped until they match.
inline void resetOption(Settings& value, Option const& option, Settings const& defaults) {
    auto target = option.read(defaults);
    if (auto const* number = std::get_if<float>(&target)) {
        if (option.numeric) option.numeric->write(value, *number);
        return;
    }
    for (int i = 0; i < 16 && option.read(value) != target; ++i) option.adjust(value, 1);
}
}
