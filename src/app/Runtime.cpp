#include "app/Runtime.h"
#include "features/camera/Zoom.h"
#include "input/Actions.h"
#include "ui/SettingsScreen.h"
#include "ll/api/Config.h"
#include "ll/api/mod/RegisterHelper.h"

namespace lamium {
Runtime& Runtime::instance() { static Runtime value; return value; }
bool Runtime::load() {
    auto path = mod.getConfigDir() / "settings.json";
    try {
        if (!ll::config::loadConfig(settings, path)) {
            if (std::filesystem::exists(path)) {
                mod.getLogger().warn("Settings could not be loaded; preserving the file and using defaults");
            } else {
                ll::config::saveConfig(settings, path);
            }
        }
    } catch (std::exception const& error) {
        mod.getLogger().error("Settings load failed: {}", error.what());
        settings = {};
    }
    settings.normalize();
    Zoom::instance().configure(settings);
    registerActions();
    return true;
}
bool Runtime::enable() {
    if (running) return true;
    if (!Zoom::instance().start()) return false;
    try { ui::start(); }
    catch (std::exception const& error) {
        mod.getLogger().error("Settings UI initialization failed: {}", error.what());
        ui::stop();
        Zoom::instance().stop();
        return false;
    }
    running = true;
    mod.getLogger().info("Lamium enabled. Hold C to zoom; scroll while held to adjust.");
    return true;
}
bool Runtime::disable() {
    running = false;
    ui::stop();
    Zoom::instance().stop();
    return true;
}
bool Runtime::save(Settings value) {
    value.normalize();
    try {
        if (!ll::config::saveConfig(value, mod.getConfigDir() / "settings.json")) return false;
        settings = value;
        Zoom::instance().configure(settings);
        return true;
    } catch (std::exception const& error) {
        mod.getLogger().error("Settings save failed: {}", error.what());
        return false;
    }
}
}
LL_REGISTER_MOD(lamium::Runtime, lamium::Runtime::instance());
