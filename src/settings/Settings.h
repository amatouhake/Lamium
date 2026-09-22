#pragma once
#include <algorithm>
#include <cmath>
#include "input/Binding.h"

namespace lamium {
struct Settings {
    int version = 1;
    input::Bindings bindings;
    struct Camera {
        bool zoom = true;
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
    } inventory;
    struct Interface {
        bool gameplayHints = true;
    } ui;
    struct Overlays {
        bool chunkBorders = false;
        bool hitboxes = false;
        float hitboxDistance = 64.f;
    } overlays;
    struct Visuals {
        bool hideOffhand = false;
    } visuals;
    struct Information {
        bool target = false;
        bool targetIdentifier = true;
        bool targetStates = false;
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
        if (!std::isfinite(camera.magnification)) camera.magnification = 3.0f;
        if (!std::isfinite(camera.wheelStep)) camera.wheelStep = 0.5f;
        camera.magnification = std::clamp(camera.magnification, 1.0f, 10.0f);
        camera.wheelStep = std::clamp(camera.wheelStep, 0.1f, 2.0f);
    }
};
}
