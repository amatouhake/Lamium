#pragma once
#include "settings/Settings.h"
#include <array>
#include <string_view>
#include <variant>

namespace lamium::settings {
using OptionValue = std::variant<bool, float>;
struct Option {
    std::string_view id;
    std::string_view feature;
    std::string_view label;
    OptionValue (*read)(Settings const&);
    void (*adjust)(Settings&, int);
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
    toggle<&Settings::camera, &Settings::Camera::zoom>("camera.zoom", "zoom", "zoom"),
    {"camera.magnification", "zoom", "magnification",
        [](Settings const& s) -> OptionValue { return s.camera.magnification; },
        [](Settings& s, int direction) { s.camera.magnification += direction * .5f; s.normalize(); }},
    {"camera.wheelStep", "zoom", "wheelStep",
        [](Settings const& s) -> OptionValue { return s.camera.wheelStep; },
        [](Settings& s, int direction) { s.camera.wheelStep += direction * .1f; s.normalize(); }},
    toggle<&Settings::lighting, &Settings::Lighting::nightVision>("lighting.nightVision", "nightVision", "nightVision"),
    toggle<&Settings::inspection, &Settings::Inspection::containerPreviews>("inspection.containerPreviews", "previews", "previews"),
    toggle<&Settings::inspection, &Settings::Inspection::shulkerPreviews>("inspection.shulkerPreviews", "previews", "shulkerPreviews"),
    toggle<&Settings::inspection, &Settings::Inspection::emptyShulkerPreviews>("inspection.emptyShulkerPreviews", "previews", "emptyShulkerPreviews"),
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
