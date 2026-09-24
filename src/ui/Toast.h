#pragma once
#include <algorithm>
#include <chrono>
#include <cmath>
#include <optional>
#include <string>

namespace lamium::ui {
// Toggle toast timing: "<feature>" shows for 1.5 s, dimming over the last
// 0.3 s. A new toast replaces the current one. Timing only; InfoHud owns the
// shared instance and the drawing.
class Toast {
    double shownAt = std::numeric_limits<double>::quiet_NaN();
    std::string message;
    bool state = false;
    bool armed = false;
public:
    static constexpr double duration = 1.5;
    static constexpr double fade = 0.3;
    struct Visible { std::string text; bool on; float opacity; };
    void show(std::string text, bool on, double now) {
        if (!std::isfinite(now)) return;
        message = std::move(text);
        state = on;
        shownAt = now;
        armed = true;
    }
    void clear() { armed = false; }
    std::optional<Visible> current(double now) const {
        if (!armed || !std::isfinite(now)) return {};
        double elapsed = now - shownAt;
        if (elapsed < 0 || elapsed >= duration) return {};
        float opacity = elapsed <= duration - fade ? 1.f
            : static_cast<float>((duration - elapsed) / fade);
        return Visible{message, state, std::clamp(opacity, 0.f, 1.f)};
    }
};
inline double toastNow() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
// Process-wide toast fed by hotkey toggles (Actions) and drawn by InfoHud.
void showToggleToast(std::string feature, bool on);
std::optional<Toast::Visible> currentToggleToast(double now);
}
