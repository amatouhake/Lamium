#pragma once
#include <cmath>
#include <optional>

namespace lamium::camera {
struct Position {
    double x{}, y{}, z{};
    bool operator==(Position const&) const = default;
};
inline bool finite(Position const& p) { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); }
// The player's eye height above the feet. A pose read mid-teleport can report
// both at one height; the standing default keeps the point usable.
inline constexpr double defaultEyeHeight = 1.62;
inline double eyeHeight(Position eye, Position feet) {
    double height = eye.y - feet.y;
    return height > 0 ? height : defaultEyeHeight;
}
// Where the drawn view stands: the detached camera's virtual feet (its position
// minus the player's eye height) in Freelook or FreeCamera, the body's feet
// otherwise. The schematic placement actions and the shapes view both place at
// this point, so a detached view is honoured everywhere (L-248). Empty when the
// pose cannot be used, so the caller acts on nothing rather than the wrong spot.
inline std::optional<Position> viewFeet(std::optional<Position> camera, Position feet, Position eye) {
    if (!finite(feet) || !finite(eye)) return std::nullopt;
    if (!camera) return feet;
    if (!finite(*camera)) return std::nullopt;
    return Position{camera->x, camera->y - eyeHeight(eye, feet), camera->z};
}
// The heading of a view direction in Minecraft degrees (yaw 0 faces +Z).
// Looking almost straight up or down leaves no horizontal part to read a
// heading from, so the session's own yaw stands in.
inline float viewYaw(double dx, double dy, double dz, float sessionYaw) {
    double length = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (!(std::hypot(dx, dz) > length * 1e-3)) return sessionYaw;
    return static_cast<float>(std::atan2(-dx, dz) * 180 / 3.14159265358979323846);
}
}
