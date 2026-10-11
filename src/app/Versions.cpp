#include "app/Versions.h"
#include "app/Runtime.h"

#include "ll/api/Versions.h"
#include <Windows.h>
#include <cstddef>
#include <mutex>
#include <set>
#include <vector>
#pragma comment(lib, "version.lib")

#ifndef LAMIUM_VERSION
#define LAMIUM_VERSION "dev"
#endif

namespace lamium {
std::string lamiumVersion() { return LAMIUM_VERSION; }
std::string runningGameVersion() {
    try { return ll::getGameVersion().to_string(); } catch (...) { return "?"; }
}
std::string runningLoaderVersion() {
    try { return ll::getLoaderVersion().to_string(); } catch (...) { return "?"; }
}
std::string runningVersionLine() {
    return versionLine(LAMIUM_VERSION, runningGameVersion(), runningLoaderVersion());
}
namespace {
bool readVerified() {
#ifdef LAMIUM_UNVERIFIED_GAME
    return false;
#else
    wchar_t path[32768];
    DWORD length = GetModuleFileNameW(nullptr, path, 32768);
    if (!length || length == 32768) return false;
    DWORD bytes = GetFileVersionInfoSizeW(path, nullptr);
    if (!bytes || bytes > 1024 * 1024) return false;
    std::vector<std::byte> data(bytes);
    if (!GetFileVersionInfoW(path, 0, bytes, data.data())) return false;
    VS_FIXEDFILEINFO* info = nullptr;
    UINT size = 0;
    if (!VerQueryValueW(data.data(), L"\\", reinterpret_cast<void**>(&info), &size)
        || size < sizeof(*info) || info->dwSignature != 0xfeef04bd) return false;
    return info->dwFileVersionMS == ((1u << 16) | 26u)
        && info->dwFileVersionLS == ((51u << 16) | 1u);
#endif
}
}
bool verifiedGameExecutable() {
    static bool const verified = readVerified();
    return verified;
}
bool versionSensitiveAllowed(std::string_view capability) {
    if (verifiedGameExecutable()) return true;
    static std::mutex mutex;
    static std::set<std::string, std::less<>> logged;
    std::lock_guard lock(mutex);
    if (logged.find(capability) == logged.end()) {
        logged.emplace(capability);
        try {
#ifdef LAMIUM_UNVERIFIED_GAME
            constexpr char const* why = "forced by the unverified_game build option";
#else
            constexpr char const* why = "the game executable is not the verified version";
#endif
            Runtime::instance().self().getLogger().warn("{} stays vanilla: {} (version {})", capability, why, runningGameVersion());
        } catch (...) {}
    }
    return false;
}
}
