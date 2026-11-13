#pragma once
#include <algorithm>
#include <chrono>
#include <cmath>
#include <optional>
#include <string>
#include <vector>

namespace lamium::ui {
// Toggle toast timing: "<feature>" shows for 1.5 s, dimming over the last
// 0.3 s. A new toast replaces the current one, except that the toggles of one
// key press share it, a row each. Timing only; InfoHud owns the shared
// instance and the drawing.
class Toast {
    double shownAt = std::numeric_limits<double>::quiet_NaN();
    std::string message;
    bool state = false;
    bool plain = false;
    bool warning = false;
    bool armed = false;
    double length = duration;
    unsigned group = 0;
public:
    struct Row { std::string text; bool on; };
    static constexpr size_t maxRows = 4;
private:
    std::vector<Row> rows; // The other toggles of the same key press
    int dropped = 0;
public:
    static constexpr double duration = 1.5;
    static constexpr double fade = 0.3;
    // A plain toast is a message without the on/off switch.
    struct Visible {
        std::string text; bool on; float opacity; bool plain = false; bool warning = false;
        std::vector<Row> more = {}; // Further toggles of the same press, under the first
        int dropped = 0;            // And how many did not fit
    };
    void show(std::string text, bool on, double now, bool asMessage = false, double seconds = duration, bool asWarning = false) {
        if (!std::isfinite(now)) return;
        length = std::isfinite(seconds) && seconds > fade ? seconds : duration;
        message = std::move(text);
        state = on;
        plain = asMessage;
        warning = asWarning;
        shownAt = now;
        armed = true;
        rows.clear();
        dropped = 0;
        group = 0;
    }
    // A toggle fired by key press `press` (0: none). Keys bound alike fire together; their toggles share the toast.
    void showToggle(std::string text, bool on, double now, unsigned press) {
        if (press && press == group && !plain && current(now)) {
            if (rows.size() + 1 < maxRows) rows.push_back({std::move(text), on});
            else ++dropped;
            shownAt = now;
            return;
        }
        show(std::move(text), on, now);
        group = press;
    }
    void clear() { armed = false; }
    std::optional<Visible> current(double now) const {
        if (!armed || !std::isfinite(now)) return {};
        double elapsed = now - shownAt;
        if (elapsed < 0 || elapsed >= length) return {};
        float opacity = elapsed <= length - fade ? 1.f
            : static_cast<float>((length - elapsed) / fade);
        return Visible{message, state, std::clamp(opacity, 0.f, 1.f), plain, warning, rows, dropped};
    }
};
inline double toastNow() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
// Process-wide toast fed by hotkey toggles (Actions) and drawn by InfoHud.
void showToggleToast(std::string feature, bool on);
// Around the actions of one key press: the toggle toasts between them share the toast.
void beginToastGroup();
void endToastGroup();
void showMessageToast(std::string message, double seconds = Toast::duration);
void showWarningToast(std::string message, double seconds = 6);
std::optional<Toast::Visible> currentToggleToast(double now);
}
