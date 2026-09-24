#pragma once
#include <algorithm>
#include <cmath>
#include "input/Binding.h"
#include "features/interaction/RestrictionMode.h"
#include "ui/HudElement.h"

namespace lamium {
struct Settings {
    int version = 1;
    input::Bindings bindings;
    struct Interaction {
        float attackInterval = 0.5f;
        float useInterval = 0.5f;
        bool breaking = false;
        interaction::RestrictionMode breakingMode = interaction::RestrictionMode::Plane;
        interaction::RestrictionMode placementMode = interaction::RestrictionMode::Plane;
    } interaction;
    struct Camera {
        bool zoom = true;
        bool freelook = false;
        bool freelookToggle = false; // Activation: false = hold the key, true = press to switch
        bool freecamera = false; // Experimental flying camera; shares Freelook's session
        float magnification = 3.0f;
        float wheelStep = 0.5f;
        bool operator==(Camera const&) const = default;
    } camera;
    struct Lighting {
        bool nightVision = false;
    } lighting;
    struct Inspection {
        bool containerPreviews = true;
        bool shulkerPreviews = true;
        bool emptyShulkerPreviews = true;
        bool hideShulkerContents = false;
        bool bundlePreviews = true;
        bool emptyBundlePreviews = true;
        bool durability = true;
    } inspection;
    struct Inventory {
        bool sorting = true;
        bool sortContainers = true;
        bool toolSwitch = false;
        bool handRestock = false;
    } inventory;
    struct Interface {
        bool toggleToasts = true;
        bool automationStatus = true;
    } ui;
    struct Hud {
        ui::HudElement info = ui::defaultHudElement(ui::HudElementId::Info);
        ui::HudElement target = ui::defaultHudElement(ui::HudElementId::Target);
        ui::HudElement status = ui::defaultHudElement(ui::HudElementId::Status);
        ui::HudElement toast = ui::defaultHudElement(ui::HudElementId::Toast);
    } hud;
    struct Overlays {
        bool chunkBorders = false;
        bool shapes = true;
        bool hitboxes = false;
        bool light = false;
        bool skyLight = false;
        float hitboxDistance = 64.f;
    } overlays;
    struct Visuals {
        bool hideOffhand = false;
    } visuals;
    struct Information {
        bool debug = false;
        bool target = false;
        bool targetIdentifier = true;
        bool targetStates = false;
        bool targetCoordinates = false;
        float targetHorizontal = 50.f;
        float targetVertical = 2.f;
        bool hud = false;
        bool coordinates = true;
        bool dimension = true;
        bool biome = false;
        bool facing = false;
        bool fps = false;
        bool frameTime = false;
        bool light = false;
        bool ping = false;
        float horizontal = 2.f;
        float vertical = 15.f;
    } information;

    void normalize() {
        for (auto* interval : {&interaction.attackInterval, &interaction.useInterval}) {
            if (!std::isfinite(*interval)) *interval = 0.5f;
            *interval = std::clamp(*interval, 0.1f, 60.f);
        }
        auto normalizeMode = [](auto& mode) { if (static_cast<unsigned>(mode) >= 4) mode = lamium::interaction::RestrictionMode::Plane; };
        normalizeMode(interaction.breakingMode);
        normalizeMode(interaction.placementMode);
        if (!std::isfinite(information.targetHorizontal)) information.targetHorizontal = 50.f;
        if (!std::isfinite(information.targetVertical)) information.targetVertical = 2.f;
        information.targetHorizontal = std::clamp(information.targetHorizontal,0.f,100.f);
        information.targetVertical = std::clamp(information.targetVertical,0.f,100.f);
        if (!std::isfinite(information.horizontal)) information.horizontal = 2.f;
        if (!std::isfinite(information.vertical)) information.vertical = 15.f;
        information.horizontal = std::clamp(information.horizontal, 0.f, 100.f);
        information.vertical = std::clamp(information.vertical, 0.f, 100.f);
        if (!std::isfinite(overlays.hitboxDistance)) overlays.hitboxDistance = 64.f;
        overlays.hitboxDistance = std::clamp(overlays.hitboxDistance, 8.f, 128.f);
        auto normalizeElement = [](ui::HudElement& element, ui::HudElement defaultValue) {
            if (!std::isfinite(element.dx)) element.dx = defaultValue.dx;
            if (!std::isfinite(element.dy)) element.dy = defaultValue.dy;
            element.dx = std::clamp(element.dx, -512.f, 512.f);
            element.dy = std::clamp(element.dy, -512.f, 512.f);
            if (!std::isfinite(element.scale)) element.scale = defaultValue.scale;
            element.scale = std::clamp(element.scale, 75.f, 150.f);
            if (static_cast<unsigned>(element.anchor) > 8) element.anchor = defaultValue.anchor;
            if (static_cast<unsigned>(element.background) > 1) element.background = defaultValue.background;
        };
        normalizeElement(hud.info, ui::defaultHudElement(ui::HudElementId::Info));
        normalizeElement(hud.target, ui::defaultHudElement(ui::HudElementId::Target));
        normalizeElement(hud.status, ui::defaultHudElement(ui::HudElementId::Status));
        normalizeElement(hud.toast, ui::defaultHudElement(ui::HudElementId::Toast));
        if (!std::isfinite(camera.magnification)) camera.magnification = 3.0f;
        if (!std::isfinite(camera.wheelStep)) camera.wheelStep = 0.5f;
        camera.magnification = std::clamp(camera.magnification, 1.0f, 10.0f);
        camera.wheelStep = std::clamp(camera.wheelStep, 0.1f, 2.0f);
    }
};
}
