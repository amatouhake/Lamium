#pragma once
#include "overlay/Geometry.h"
namespace lamium::overlay {
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
