#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <optional>

namespace lamium {
// Session-owned displacement from the activation eye, never a player transform.
// The native adapter supplies the observed camera basis and elapsed seconds.
class DetachedCameraMotion {
public:
    using Vector = std::array<double, 3>;
private:
    mutable std::mutex mutex;
    std::optional<Vector> displacement;
    std::uint64_t owner = 0;
    static bool finite(Vector const& value) {
        return std::all_of(value.begin(), value.end(), [](double v) { return std::isfinite(v); });
    }
public:
    bool begin(std::uint64_t ownerId) {
        std::lock_guard lock{mutex};
        if (displacement || !ownerId) return false;
        displacement = Vector{};
        owner = ownerId;
        return true;
    }
    bool advance(std::uint64_t ownerId, Vector input, Vector const& right,
                 Vector const& up, Vector const& forward, double speed, double seconds) {
        std::lock_guard lock{mutex};
        if (!displacement) return false;
        if (owner != ownerId || !finite(input) || !finite(right) || !finite(up) || !finite(forward)
            || !std::isfinite(speed) || speed < 0 || !std::isfinite(seconds) || seconds < 0) {
            displacement.reset();
            return false;
        }
        // Clamp analog axes before combining them. Normalize in world space so
        // diagonals (and imperfect native basis lengths) never increase speed.
        for (auto& axis : input) axis = std::clamp(axis, -1.0, 1.0);
        Vector direction{};
        for (size_t i = 0; i < 3; ++i)
            direction[i] = input[0] * right[i] + input[1] * up[i] + input[2] * forward[i];
        double length = std::hypot(direction[0], direction[1], direction[2]);
        if (!std::isfinite(length)) { displacement.reset(); return false; }
        double distance = std::min(speed, 100.0) * std::min(seconds, .1);
        auto next = *displacement;
        for (size_t i = 0; i < 3; ++i) next[i] += direction[i] / std::max(1.0, length) * distance;
        if (!finite(next)) { displacement.reset(); return false; }
        displacement = next;
        return true;
    }
    std::optional<Vector> snapshot() const {
        std::lock_guard lock{mutex};
        return displacement;
    }
    void cancel() {
        std::lock_guard lock{mutex};
        displacement.reset();
    }
};
}
