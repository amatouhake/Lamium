#pragma once
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
// checked on (1.26.51.01). Read once. A build configured with
// `--unverified_game=y` answers no, to test the fail-open paths (L-137).
bool verifiedGameExecutable();
// Whether a version-sensitive capability may run (L-137, docs/GAME-UPDATES.md):
// a path that writes game memory or relies on a meaning a rebuild cannot
// check asks here, at that path, and stays vanilla on a no. Other parts of
// the same feature keep working. The first no per capability is logged.
bool versionSensitiveAllowed(std::string_view capability);
}
