#pragma once
#include "features/interaction/AutoClick.h"
class IClientInstance;
// Auto attack / Auto use (the files keep the Periodic name of the first mode).
namespace lamium::interaction::periodic {
enum class Action { Attack, Use };
void start();
void stop();
// Menus, focus loss, dimension changes: stops Periodic and Hold. Fast click
// stays switched on; it only acts while the button is physically held.
void cancel();
// World exit and disable: also switches Fast click off.
void endSession();
AutoMode mode(IClientInstance& client, Action action);
bool fast(IClientInstance& client, Action action);
// Periodic/Hold toggle: the same mode again stops it, another mode replaces it.
void toggle(IClientInstance&, Action, AutoMode);
void toggleFast(IClientInstance&, Action);
}
