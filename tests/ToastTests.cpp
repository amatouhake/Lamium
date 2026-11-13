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
    check(fading && fading->opacity > 0.f && fading->opacity < 1.f, "last stretch dims");
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
    toast.show("Unreadable file", false, 30, true, 6, true);
    auto warning = toast.current(35.6);
    check(warning && warning->warning && warning->plain && warning->opacity == 1.f,
          "a warning message keeps its flag and stays opaque until the six-second fade");
    warning = toast.current(35.85);
    check(warning && warning->warning && warning->opacity > 0.f && warning->opacity < 1.f,
          "a warning keeps its flag while fading");
    check(!toast.current(36), "a six-second warning expires on time");
    toast.show("Message", false, 40, true);
    check(!toast.current(40)->warning, "an ordinary message resets the warning flag");
    toast.show("Unreadable file", false, 41, true, 6, true);
    toast.show("Zoom", true, 42);
    check(!toast.current(42)->warning && !toast.current(42)->plain, "a toggle resets the warning and message flags");
    {
        Toast stack;
        stack.showToggle("A", true, 50, 7);
        stack.showToggle("B", false, 50, 7);
        auto both = stack.current(50.5);
        check(both && both->text == "A" && both->on && both->more.size() == 1 && both->more[0].text == "B" && !both->more[0].on,
              "toggles of one key press share the toast, a row each");
        for (auto name : {"C", "D", "E", "F"}) stack.showToggle(name, true, 50, 7);
        both = stack.current(50.5);
        check(both && both->more.size() == Toast::maxRows - 1 && both->dropped == 2, "rows past the limit are counted, not shown");
        stack.showToggle("G", true, 51, 8);
        auto next = stack.current(51);
        check(next && next->text == "G" && next->more.empty() && !next->dropped, "another key press replaces the toast");
        stack.showToggle("H", true, 51, 0);
        check(stack.current(51)->text == "H" && stack.current(51)->more.empty(), "a toggle outside a key press never joins");
        stack.show("Message", false, 52, true);
        stack.showToggle("I", true, 52, 0);
        check(stack.current(52)->text == "I" && !stack.current(52)->plain, "a toggle replaces a message");
    }
    check(toastNow() > 0, "toast clock advances");
}
