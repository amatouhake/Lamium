#include "app/Runtime.h"
#include "features/camera/Zoom.h"
#include "features/lighting/NightVision.h"
#include "features/inspection/Inspection.h"
#include "input/Actions.h"
#include "ui/SettingsScreen.h"
#include "settings/SettingsStore.h"
#include "ll/api/mod/RegisterHelper.h"

namespace lamium {
Runtime& Runtime::instance() { static Runtime value; return value; }
bool Runtime::load() {
    auto path = mod.getConfigDir() / "settings.json";
    try {
        if (std::filesystem::exists(path)) settings = readSettings(path);
        else writeSettings(path, settings);
    } catch (std::exception const& error) {
        mod.getLogger().error("Settings load failed: {}", error.what());
        settings = {};
    }
    settings.normalize();
    Zoom::instance().configure(settings);
    NightVision::instance().configure(settings.lighting.nightVision);
    registerActions();
    return true;
}
bool Runtime::enable() {
    if (running) return true;
    if (!Zoom::instance().start()) return false;
    if (!NightVision::instance().start()) {
        mod.getLogger().error("Lighting hooks could not be installed");
        Zoom::instance().stop();
        return false;
    }
    if (!inspection::start()) {
        NightVision::instance().stop();
        Zoom::instance().stop();
        return false;
    }
    try { ui::start(); }
    catch (std::exception const& error) {
        mod.getLogger().error("Settings UI initialization failed: {}", error.what());
        ui::stop();
        inspection::stop();
        NightVision::instance().stop();
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
    inspection::stop();
    NightVision::instance().stop();
    Zoom::instance().stop();
    return true;
}
bool Runtime::save(Settings value) {
    std::lock_guard lock(settingsMutex);
    value.normalize();
    try {
        writeSettings(mod.getConfigDir() / "settings.json", value);
        bool cameraChanged = !(settings.camera == value.camera);
        settings = value;
        if (cameraChanged) Zoom::instance().configure(settings);
        NightVision::instance().configure(settings.lighting.nightVision);
        return true;
    } catch (std::exception const& error) {
        mod.getLogger().error("Settings save failed: {}", error.what());
        return false;
    }
}
}
LL_REGISTER_MOD(lamium::Runtime, lamium::Runtime::instance());
