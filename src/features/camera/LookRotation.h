#pragma once
#include <array>
#include <cmath>

namespace lamium {
// Row-major camera-view rotation. Remove the initial pitch before yawing, then
// apply the target pitch, so yaw is not about an already-tilted camera axis.
inline std::array<float, 9> lookRotation(float initialPitch, float targetPitch, float yaw) {
    constexpr float radians = 0.01745329252f;
    float b = initialPitch * radians, p = targetPitch * radians, y = yaw * radians;
    float cb = std::cos(b), sb = std::sin(b), cp = std::cos(p), sp = std::sin(p);
    float cy = std::cos(y), sy = std::sin(y);
    // Rx(targetPitch) * Ry(yaw) * Rx(-initialPitch).
    return {cy, -sy * sb, sy * cb,
            sp * sy, cp * cb + sp * cy * sb, cp * sb - sp * cy * cb,
            -cp * sy, sp * cb - cp * cy * sb, sp * sb + cp * cy * cb};
}
}
