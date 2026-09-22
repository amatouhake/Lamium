#pragma once
#include <algorithm>
#include <cmath>
#include <mutex>
#include <optional>

namespace lamium {
// Degrees, supplied by the integration layer after converting native input units.
// No player/camera pointers or saved transforms are retained here.
class DetachedLookState {
public:
    struct Angles {
        float pitch = 0;
        float yaw = 0;
    };
private:
    mutable std::mutex mutex;
    std::optional<Angles> pose;
public:
    bool begin(float pitch, float yaw) {
        std::lock_guard lock{mutex};
        if (pose) return false; // Key repeat must not reset the detached view.
        if (!std::isfinite(pitch) || !std::isfinite(yaw)) return false;
        pose = Angles{std::clamp(pitch, -90.f, 90.f), std::remainder(yaw, 360.f)};
        return true;
    }
    bool turn(float pitchDelta, float yawDelta) {
        std::lock_guard lock{mutex};
        if (!pose) return false;
        if (!std::isfinite(pitchDelta) || !std::isfinite(yawDelta)) {
            pose.reset();
            return false;
        }
        // Promote before addition so even finite float deltas cannot overflow.
        pose->pitch = static_cast<float>(std::clamp(
            static_cast<double>(pose->pitch) + pitchDelta, -90.0, 90.0));
        pose->yaw = static_cast<float>(std::remainder(
            static_cast<double>(pose->yaw) + yawDelta, 360.0));
        return true;
    }
    std::optional<Angles> snapshot() const {
        std::lock_guard lock{mutex};
        return pose;
    }
    void cancel() {
        std::lock_guard lock{mutex};
        pose.reset(); // Remove the override; never restore an old player pose.
    }
};
}
