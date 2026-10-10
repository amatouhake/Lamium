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
// The game executable is the build Lamium's version-sensitive render hooks
// were checked on (1.26.51.01); those features stay vanilla otherwise.
bool verifiedGameExecutable();
}
