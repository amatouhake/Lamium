#pragma once
#include "overlay/Geometry.h"
namespace lamium::overlay {
// Java F3+B eye marker: a fixed-size red box centered on the eye position.
// The size approximates the Java look; confirm against a screenshot in game.
inline std::array<Line, 12> eyeBox(Point eye) {
    constexpr double half = .2;
    return wireBox({eye.x - half, eye.y - half, eye.z - half}, {eye.x + half, eye.y + half, eye.z + half});
}
// Blue look line: 2 blocks from the eyes along the view direction.
inline Line lookLine(Point eye, double dx, double dy, double dz) {
    double length = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (!(length > 0) || !std::isfinite(length)) return {eye, eye};
    double scale = 2 / length;
    return {eye, {eye.x + dx * scale, eye.y + dy * scale, eye.z + dz * scale}};
}
inline bool hitboxInRange(Point min, Point max, Point camera, double range) {
    if (!finite(min) || !finite(max) || !finite(camera) || !std::isfinite(range) || range < 0
        || min.x >= max.x || min.y >= max.y || min.z >= max.z) return false;
    double distanceSquared = 0;
    for (auto axis : {std::array{min.x,max.x,camera.x}, std::array{min.y,max.y,camera.y}, std::array{min.z,max.z,camera.z}}) {
        double delta = axis[2] - std::clamp(axis[2],axis[0],axis[1]);
        distanceSquared += delta*delta;
    }
    return distanceSquared <= range*range;
}
}
