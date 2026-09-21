#pragma once
#include "settings/Settings.h"
#include "ll/api/mod/NativeMod.h"
#include <atomic>
#include <mutex>

namespace lamium {
class Runtime {
    ll::mod::NativeMod& mod = *ll::mod::NativeMod::current();
    Settings settings;
    std::atomic<bool> running{false};
    mutable std::mutex settingsMutex;
public:
    static Runtime& instance();
    bool load();
    bool enable();
    bool disable();
    bool enabled() const { return running.load(); }
    ll::mod::NativeMod& self() { return mod; }
    Settings preferences() const { std::lock_guard lock(settingsMutex); return settings; }
    bool save(Settings value);
};
}
