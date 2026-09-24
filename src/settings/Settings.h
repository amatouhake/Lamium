#pragma once
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include "input/Binding.h"
#include "features/interaction/RestrictionMode.h"
#include "features/information/InfoLines.h"
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
        int animations = 0; // 0 follow Minecraft's Screen Animations, 1 on, 2 off
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
        bool targetIcon = true;
        int targetHealth = 0; // 0 hearts, 1 bar, 2 number
        int targetGrowth = 0; // 0 bar, 1 number
        int targetRange = 0; // 0 the game's reach, then 8/16/32/64 blocks (TargetCard.h)
        bool targetStates = false; // Other details
        bool targetCoordinates = false;
        bool hud = false;
        bool coordinates = true;
        bool dimension = true;
        bool biome = false;
        bool facing = false;
        bool fps = false;
        bool frameTime = false;
        bool light = false;
        bool ping = false;
        bool rotation = false;
        bool block = false;
        bool chunk = false;
        bool speed = false;
        bool time = false;
        bool weather = false;
        bool moon = false;
        std::vector<std::string> lineOrder;
    } information;

    void normalize() {
        for (auto* interval : {&interaction.attackInterval, &interaction.useInterval}) {
            if (!std::isfinite(*interval)) *interval = 0.5f;
            *interval = std::clamp(*interval, 0.1f, 60.f);
        }
        auto normalizeMode = [](auto& mode) { if (static_cast<unsigned>(mode) >= 4) mode = lamium::interaction::RestrictionMode::Plane; };
        normalizeMode(interaction.breakingMode);
        information.targetHealth = std::clamp(information.targetHealth, 0, 2);
        ui.animations = std::clamp(ui.animations, 0, 2);
        information.targetGrowth = std::clamp(information.targetGrowth, 0, 1);
        information.targetRange = std::clamp(information.targetRange, 0, 4);
        normalizeMode(interaction.placementMode);
        information.lineOrder = information::mergeLineOrder(information.lineOrder);
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
