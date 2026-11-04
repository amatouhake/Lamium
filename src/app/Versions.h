#pragma once
#include "app/PausedFeatures.h"
#include <format>
#include <string>
#include <string_view>

namespace lamium {
// One line for bug reports; English in every locale because it is pasted
// into issues (L-101).
inline std::string versionLine(std::string_view lamium, std::string_view game, std::string_view loader) {
    return std::format("Lamium {} · Minecraft {} · LeviLamina {}", lamium, game, loader);
}
std::string lamiumVersion();
// The running game and loader, not the versions the build targets.
std::string runningGameVersion();
std::string runningLoaderVersion();
std::string runningVersionLine();
// The game executable is the build Lamium's version-sensitive paths were
// checked on (1.26.60.05). Read once. A build configured with
// `--unverified_game=y` answers no, to test the fail-open paths (L-137).
bool verifiedGameExecutable();
// A capability may run only when the shared compatibility policy permits it
// and its adapter is ready (L-202). Native checks stay in the adapter; lazy
// binding uses status().canProbe() until those checks publish ready().
// Failure reasons are shared with the UI and logged at the transition.
bool versionSensitiveAllowed(capabilities::Id capability);
// How much of a feature is paused on this game (none on the verified one).
paused::Level pausedLevel(std::string_view feature);
}
