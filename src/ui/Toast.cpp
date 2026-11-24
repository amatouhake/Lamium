#include "ui/Toast.h"
#include <mutex>
#include <optional>
#include <string>
#include <utility>

namespace lamium::ui {
namespace {
Toast activeToast;
unsigned toastGroups = 0, toastGroup = 0;
std::mutex holdMutex;
ToastHold toastHold;
bool toastsHeld = false;
thread_local int screenDepth = 0;
// Holds a toast from outside while the settings screen is open; true when it was held.
bool held(std::string& message, double seconds, bool warning) {
    if (screenDepth > 0) return false;
    std::lock_guard lock(holdMutex);
    if (!toastsHeld) return false;
    toastHold.hold({std::move(message), seconds, warning, toastNow()});
    return true;
}
}
void showToggleToast(std::string feature, bool on) { activeToast.showToggle(std::move(feature), on, toastNow(), toastGroup); }
void beginToastGroup() { if (!++toastGroups) ++toastGroups; toastGroup = toastGroups; }
void endToastGroup() { toastGroup = 0; }
void showMessageToast(std::string message, double seconds) {
    if (held(message, seconds, false)) return;
    activeToast.show(std::move(message), false, toastNow(), true, seconds);
}
void showWarningToast(std::string message, double seconds) {
    if (held(message, seconds, true)) return;
    activeToast.show(std::move(message), false, toastNow(), true, seconds, true);
}
ScreenToasts::ScreenToasts() { ++screenDepth; }
ScreenToasts::~ScreenToasts() { --screenDepth; }
void holdToasts(bool on) {
    std::lock_guard lock(holdMutex);
    toastsHeld = on;
}
void releaseHeldToast(double now) {
    std::optional<ToastHold::Held> next;
    {
        std::lock_guard lock(holdMutex);
        if (toastsHeld || toastHold.empty() || activeToast.current(now)) return;
        next = toastHold.next(now);
    }
    if (next) activeToast.show(std::move(next->text), false, now, true, next->seconds, next->warning);
}
std::optional<Toast::Visible> currentToggleToast(double now) { return activeToast.current(now); }
}
