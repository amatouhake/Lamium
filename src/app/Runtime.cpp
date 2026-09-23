#include "app/Runtime.h"
#include "features/interaction/PermanentSneak.h"
#include "features/interaction/PeriodicInput.h"
#include "features/interaction/AutomationTrace.h"
#include "features/camera/Zoom.h"
#include "features/lighting/NightVision.h"
#include "features/inspection/Inspection.h"
#include "features/inventory/Inventory.h"
#include "features/inventory/ToolSwitch.h"
#include "features/information/FrameTiming.h"
#include "features/interaction/BreakingRestriction.h"
#include "features/interaction/PlacementTrace.h"
#include "features/visuals/HideOffhand.h"
#include "input/CustomInput.h"
#include "overlay/WorldOverlay.h"
#include "ui/SettingsScreen.h"
#include "settings/SettingsStore.h"
#include "ll/api/mod/RegisterHelper.h"
#include "ll/api/io/FileSink.h"
#include "ll/api/io/PatternFormatter.h"
#include "ll/api/io/RotatePolicy.h"

namespace lamium {
Runtime& Runtime::instance() { static Runtime value; return value; }
bool Runtime::load() {
    try {
        ll::io::RotatePolicy rotation;
        rotation.maxFileSize = 4 * 1024 * 1024;
        rotation.maxFiles = 7;
        rotation.totalSizeCap = 32 * 1024 * 1024;
        rotation.compress = false;
        auto sink = std::make_shared<ll::io::FileSink>(
            mod.getModDir() / "logs" / "lamium.log",
            ll::makePolymorphic<ll::io::PatternFormatter>("[{3:.3%F %T.} {2}][{1}] {0}", false), rotation);
        sink->setFlushLevel(ll::io::LogLevel::Info);
        mod.getLogger().addSink(std::move(sink));
    } catch (std::exception const& error) {
        mod.getLogger().warn("Could not create Lamium log: {}", error.what());
    }
    auto path = mod.getConfigDir() / "settings.json";
    try {
        if (std::filesystem::exists(path)) settings = readSettings(path);
        else writeSettings(path, settings);
    } catch (std::exception const& error) {
        mod.getLogger().error("Settings load failed: {}", error.what());
        settings = {};
    }
    settings.normalize();
    try { interaction::periodic::start(); }
    catch (std::exception const& error) {
        mod.getLogger().warn("Periodic input unavailable: {}", error.what());
    }
    try { interaction::automationTrace::start(); }
    catch (std::exception const& error) {
        mod.getLogger().warn("Automation diagnostics unavailable: {}", error.what());
    }
    Zoom::instance().configure(settings);
    NightVision::instance().configure(settings.lighting.nightVision);
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
    if (!inventory::start()) {
        inspection::stop();
        NightVision::instance().stop();
        Zoom::instance().stop();
        return false;
    }
    try { ui::start(); input::startCustomInput(); overlay::start(); visuals::start(); inventory::tools::start(); information::startFrameTiming(); interaction::breaking::start(); interaction::placementTrace::start(); }
    catch (std::exception const& error) {
        mod.getLogger().error("Client feature initialization failed: {}", error.what());
        interaction::placementTrace::stop();
        interaction::breaking::stop();
        information::stopFrameTiming();
        inventory::tools::stop();
        visuals::stop();
        input::stopCustomInput();
        overlay::stop();
        ui::stop();
        inventory::stop();
        inspection::stop();
        NightVision::instance().stop();
        Zoom::instance().stop();
        return false;
    }
    try { interaction::sneak::start(); }
    catch (std::exception const& error) {
        mod.getLogger().error("Sneak initialization failed: {}", error.what());
        disable();
        return false;
    }
    running = true;
    mod.getLogger().info("Lamium enabled. Configure features and bindings in Lamium Settings (default: L), using Features or Hotkeys.");
    return true;
}
bool Runtime::disable() {
    running = false;
    interaction::periodic::stop();
    interaction::automationTrace::stop();
    interaction::sneak::stop();
    interaction::placementTrace::stop();
    interaction::breaking::stop();
    information::stopFrameTiming();
    inventory::tools::stop();
    visuals::stop();
    overlay::stop();
    input::stopCustomInput();
    ui::stop();
    inventory::stop();
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
        if (settings.interaction.breaking != value.interaction.breaking
            || settings.interaction.breakingMode != value.interaction.breakingMode) interaction::breaking::reset();
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
