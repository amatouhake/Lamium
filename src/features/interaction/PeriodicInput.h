#pragma once
#include "features/interaction/AutoClick.h"
#include <string>
class IClientInstance;
namespace lamium::interaction {
// The mode as toasts and the status line name it: "Hold", "Fast click (while held)".
std::string autoModeText(AutoMode mode, FastTrigger trigger);
}
// Auto Attack / Auto Use (the files keep the Periodic name of the first mode).
// The switches and modes live in Settings; this adapter follows them each
// client tick and feeds vanilla's own attack/use callbacks.
namespace lamium::interaction::periodic {
enum class Action { Attack, Use };
void start();
void stop();
// Focus loss, screen and dimension changes: forget the physical held state
// and release any synthetic press. The switches stay on.
void interrupt();
// World exit: switch both off.
void endSession();
// On, but gameplay input is not ours right now (menu, death, detached camera).
bool paused(IClientInstance& client);
}
