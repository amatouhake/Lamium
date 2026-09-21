#pragma once
#include <algorithm>
#include <cmath>

namespace lamium {
struct Settings {
    int version = 1;
    struct Camera {
        bool zoom = true;
        float magnification = 3.0f;
        float wheelStep = 0.5f;
    } camera;

    void normalize() {
        if (!std::isfinite(camera.magnification)) camera.magnification = 3.0f;
        if (!std::isfinite(camera.wheelStep)) camera.wheelStep = 0.5f;
        camera.magnification = std::clamp(camera.magnification, 1.0f, 10.0f);
        camera.wheelStep = std::clamp(camera.wheelStep, 0.1f, 2.0f);
    }
};
}
