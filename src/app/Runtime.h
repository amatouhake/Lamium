#pragma once
#include "settings/Settings.h"
#include "ll/api/mod/NativeMod.h"
#include <atomic>

namespace lamium {
class Runtime {
    ll::mod::NativeMod& mod = *ll::mod::NativeMod::current();
    Settings settings;
    std::atomic<bool> running{false};
public:
    static Runtime& instance();
    bool load();
    bool enable();
    bool disable();
    bool enabled() const { return running.load(); }
    ll::mod::NativeMod& self() { return mod; }
    Settings const& preferences() const { return settings; }
    bool save(Settings value);
};
}
