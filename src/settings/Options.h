#pragma once
#include "settings/Settings.h"
#include <array>
#include <optional>
#include <string_view>
#include <variant>

namespace lamium::settings {
using OptionValue = std::variant<bool, float>;
struct NumericOption {
    float minimum, maximum;
    void (*write)(Settings&, float);
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
inline constexpr auto options = std::to_array<Option>({
    toggle<&Settings::information, &Settings::Information::hud>("information.hud", "infoHud", "infoHud"),
    toggle<&Settings::information, &Settings::Information::coordinates>("information.coordinates", "infoHud", "hudCoordinates"),
    toggle<&Settings::information, &Settings::Information::dimension>("information.dimension", "infoHud", "hudDimension"),
    toggle<&Settings::information, &Settings::Information::biome>("information.biome", "infoHud", "hudBiome"),
    toggle<&Settings::information, &Settings::Information::facing>("information.facing", "infoHud", "hudFacing"),
    toggle<&Settings::information, &Settings::Information::fps>("information.fps", "infoHud", "hudFps"),
    toggle<&Settings::information, &Settings::Information::frameTime>("information.frameTime", "infoHud", "hudFrameTime"),
    toggle<&Settings::information, &Settings::Information::light>("information.light", "infoHud", "hudLight"),
    toggle<&Settings::information, &Settings::Information::ping>("information.ping", "infoHud", "hudPing"),
    {"information.horizontal", "infoHud", "hudHorizontal",
        [](Settings const& s) -> OptionValue { return s.information.horizontal; },
        [](Settings& s, int direction) { s.information.horizontal += direction * 5.f; s.normalize(); },
        NumericOption{0,100,[](Settings& s, float v) { s.information.horizontal = v; }}},
    {"information.vertical", "infoHud", "hudVertical",
        [](Settings const& s) -> OptionValue { return s.information.vertical; },
        [](Settings& s, int direction) { s.information.vertical += direction * 5.f; s.normalize(); },
        NumericOption{0,100,[](Settings& s, float v) { s.information.vertical = v; }}},
    toggle<&Settings::inventory, &Settings::Inventory::toolSwitch>("inventory.toolSwitch", "toolSwitch", "toolSwitch"),
    toggle<&Settings::overlays, &Settings::Overlays::hitboxes>("overlays.hitboxes", "hitboxes", "hitboxes"),
    {"overlays.hitboxDistance", "hitboxes", "hitboxDistance",
        [](Settings const& s) -> OptionValue { return s.overlays.hitboxDistance; },
        [](Settings& s, int direction) { s.overlays.hitboxDistance += direction * 8.f; s.normalize(); },
        NumericOption{8, 128, [](Settings& s, float v) { s.overlays.hitboxDistance = v; }}},
    toggle<&Settings::visuals, &Settings::Visuals::hideOffhand>("visuals.hideOffhand", "hideOffhand", "hideOffhand"),
    toggle<&Settings::overlays, &Settings::Overlays::chunkBorders>("overlays.chunkBorders", "chunkBorders", "chunkBorders"),
    toggle<&Settings::camera, &Settings::Camera::zoom>("camera.zoom", "zoom", "zoom"),
    {"camera.magnification", "zoom", "magnification",
        [](Settings const& s) -> OptionValue { return s.camera.magnification; },
        [](Settings& s, int direction) { s.camera.magnification += direction * .5f; s.normalize(); },
        NumericOption{1, 10, [](Settings& s, float v) { s.camera.magnification = v; }}},
    {"camera.wheelStep", "zoom", "wheelStep",
        [](Settings const& s) -> OptionValue { return s.camera.wheelStep; },
        [](Settings& s, int direction) { s.camera.wheelStep += direction * .1f; s.normalize(); },
        NumericOption{.1f, 2, [](Settings& s, float v) { s.camera.wheelStep = v; }}},
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
    toggle<&Settings::ui, &Settings::Interface::gameplayHints>("interface.gameplayHints", "gameplayHints", "gameplayHints"),
});
inline Option const* find(std::string_view id) {
    for (auto const& option : options) if (option.id == id) return &option;
    return nullptr;
}
}
