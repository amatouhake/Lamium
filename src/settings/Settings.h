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
    } inventory;
    struct Interface {
        bool gameplayHints = true;
    } ui;
    struct Overlays {
        bool chunkBorders = false;
    } overlays;
    struct Visuals {
        bool hideOffhand = false;
    } visuals;

    void normalize() {
        if (!std::isfinite(camera.magnification)) camera.magnification = 3.0f;
        if (!std::isfinite(camera.wheelStep)) camera.wheelStep = 0.5f;
        camera.magnification = std::clamp(camera.magnification, 1.0f, 10.0f);
        camera.wheelStep = std::clamp(camera.wheelStep, 0.1f, 2.0f);
    }
};
}
