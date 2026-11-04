#include "app/Versions.h"
#include "app/Runtime.h"

#include "ll/api/Versions.h"
#include <Windows.h>
#include <cstddef>
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
// The game executable the version-sensitive paths were checked on. Change it
// only at step 5 of the playbook in docs/GAME-UPDATES.md, after every gated
// capability was checked on the new game; never as part of an SDK bump.
// New-ReleaseArchive.ps1 refuses a release whose SDK targets another game.
constexpr unsigned verifiedGame[4] = {1, 26, 60, 5};
std::array<unsigned,4> readExecutableVersion() {
    wchar_t path[32768];
    DWORD length = GetModuleFileNameW(nullptr, path, 32768);
    if (!length || length == 32768) return {};
    DWORD bytes = GetFileVersionInfoSizeW(path, nullptr);
    if (!bytes || bytes > 1024 * 1024) return {};
    std::vector<std::byte> data(bytes);
    if (!GetFileVersionInfoW(path, 0, bytes, data.data())) return {};
    VS_FIXEDFILEINFO* info = nullptr;
    UINT size = 0;
    if (!VerQueryValueW(data.data(), L"\\", reinterpret_cast<void**>(&info), &size)
        || size < sizeof(*info) || info->dwSignature != 0xfeef04bd) return {};
    return {HIWORD(info->dwFileVersionMS),LOWORD(info->dwFileVersionMS),HIWORD(info->dwFileVersionLS),LOWORD(info->dwFileVersionLS)};
}
std::array<unsigned,4> executableVersion() {
    static auto const version = readExecutableVersion();
    return version;
}
}
bool verifiedGameExecutable() {
#ifdef LAMIUM_UNVERIFIED_GAME
    return false;
#else
    return executableVersion() == std::to_array(verifiedGame);
#endif
}
paused::Level pausedLevel(std::string_view feature) {
    return static_cast<paused::Level>(capabilities::featureImpact(feature,
        [](capabilities::Id id) { return capabilities::registry().status(id); }));
}
namespace capabilities {
Registry& registry() {
    static Registry value{[] {
        Environment env{verifiedGameExecutable(),-1,-1,-1};
        env.gameBuild = executableVersion();
#ifdef LAMIUM_UNVERIFIED_GAME
        env.forceUnverified = true;
#endif
        try { auto version = ll::getLoaderVersion(); env.major = version.major; env.minor = version.minor; env.patch = version.patch; }
        catch (...) {}
        return env;
    }()};
    return value;
}
namespace {
void logFailure(Id id, Reason reason, char const* detail) noexcept {
    try {
        Runtime::instance().self().getLogger().warn("{} stays vanilla: {}{}{}", definition(id).logName,
            reasonName(reason), detail ? ": " : "", detail ? detail : "");
    } catch (...) {}
}
}
void begin(Id id) noexcept { registry().begin(id); }
void ready(Id id) noexcept { registry().ready(id); }
void fail(Id id, Reason reason, char const* detail) noexcept {
    if (!registry().fail(id,reason)) return;
    auto state = registry().status(id);
    logFailure(id,state.reason(),detail);
    if (state.reason() != state.runtime.reason)
        try { Runtime::instance().self().getLogger().warn("{} native result: {}",definition(id).logName,reasonName(state.runtime.reason)); }
        catch (...) {}
}
void stop(Id id) noexcept { registry().stop(id); }
void hooks(Id id, bool available) noexcept {
    if (available) ready(id);
    else fail(id,Reason::HookUnavailable);
}
}
bool versionSensitiveAllowed(capabilities::Id id) {
    auto state = capabilities::registry().status(id);
    if (state.available()) return true;
    // Runtime failures log at the transition. Policy failures log once per id,
    // without a mutex or string allocation on the successful hot path.
    if (state.compatibility != capabilities::Reason::None) {
        static std::array<std::atomic<bool>,capabilities::count> logged{};
        if (!logged[static_cast<size_t>(id)].exchange(true))
            try { Runtime::instance().self().getLogger().warn("{} stays vanilla: {} (game {}, loader {})",
                capabilities::definition(id).logName,capabilities::reasonName(state.compatibility),runningGameVersion(),runningLoaderVersion()); }
            catch (...) {}
    }
    return false;
}
}
