#include "ui/Toast.h"
#include <limits>
void check(bool, char const*);
void toastTests() {
    using namespace lamium::ui;
    Toast toast;
    check(!toast.current(0), "empty toast shows nothing");
    toast.show("NightVision", true, 10.0);
    auto shown = toast.current(10.0);
    check(shown && shown->text == "NightVision" && shown->on && shown->opacity == 1.f, "toast appears at once");
    check(toast.current(11.1)->opacity == 1.f, "fully opaque before the fade");
    auto fading = toast.current(11.35);
    check(fading && fading->opacity > 0.f && fading->opacity < 1.f, "last stretch fades");
    check(!toast.current(11.5), "toast expires after its duration");
    check(!toast.current(9.0), "toast ignores the past");
    toast.show("Zoom", false, 20.0);
    auto replaced = toast.current(20.5);
    check(replaced && replaced->text == "Zoom" && !replaced->on, "a new toast replaces the current one");
    toast.show("NightVision", true, 21.0);
    check(static_cast<bool>(toast.current(22.0)), "replacement restarts the timer past the old deadline");
    toast.show("Ignored", true, std::numeric_limits<double>::quiet_NaN());
    check(toast.current(21.2)->text == "NightVision", "invalid time cannot replace a toast");
    check(!toast.current(std::numeric_limits<double>::infinity()), "invalid time shows nothing");
    toast.clear();
    check(!toast.current(21.2), "clear hides the toast");
    check(toastNow() > 0, "toast clock advances");
}
